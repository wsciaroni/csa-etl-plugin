#include "clang/AST/ParentMapContext.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Frontend/CheckerRegistry.h"
#include "llvm/ADT/ArrayRef.h"

using namespace clang;
using namespace ento;

// 1. Define the states an etl::optional/expected can be in
enum class EtlState { Unknown, HasValue, Empty };

// 2. Register a custom trait map in the ProgramState
// This maps the memory region of the object to its current known state.
REGISTER_MAP_WITH_PROGRAMSTATE(EtlStateMap, const MemRegion *, EtlState)

namespace {

static bool isInEtlNamespace(const CXXRecordDecl *RD) {
  for (const DeclContext *DC = RD->getDeclContext(); DC; DC = DC->getParent()) {
    if (const auto *NS = dyn_cast<NamespaceDecl>(DC)) {
      if (NS->getName() == "etl") {
        return true;
      }
    }
  }

  return false;
}

// Helper to ensure we only analyze selected etl::* types.
static bool isEtlTypeNamed(const CXXRecordDecl *RD,
                           llvm::ArrayRef<StringRef> AllowedNames) {
  if (!RD) return false;

  // If it's a template instantiation (e.g., expected<int, int>),
  // resolve to the primary template declaration.
  if (const auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(RD)) {
    RD = CTSD->getSpecializedTemplate()->getTemplatedDecl();
  }

  if (!RD->getIdentifier()) return false;

  const StringRef Name = RD->getName();
  if (llvm::find(AllowedNames, Name) == AllowedNames.end()) return false;

  return isInEtlNamespace(RD);
}

// Helper to ensure we only analyze etl::optional and etl::expected
static bool isEtlType(const CXXRecordDecl *RD) {
  return isEtlTypeNamed(RD, {"optional", "expected"});
}

static bool isEtlExpectedType(QualType QT) {
  QT = QT.getNonReferenceType();
  if (QT.isNull()) return false;

  const auto *RD = QT->getAsCXXRecordDecl();
  return isEtlTypeNamed(RD, {"expected"});
}

enum class CallValueUse { Used, Discarded, ExplicitDiscard };

// Classifies whether a call expression's value is consumed by surrounding AST.
static CallValueUse classifyCallValueUse(const Expr *E, ASTContext &Ctx) {
  const Stmt *Current = E;

  while (Current) {
    auto Parents = Ctx.getParents(*Current);
    if (Parents.empty()) {
      return CallValueUse::Used;
    }

    const auto &ParentNode = Parents[0];

    if (const auto *ParentStmt = ParentNode.get<Stmt>()) {
      if (const auto *Cast = dyn_cast<CastExpr>(ParentStmt)) {
        if (Cast->getType()->isVoidType()) {
          return CallValueUse::ExplicitDiscard;
        }
      }

      // Skip wrappers that do not represent value consumption on their own.
      if (isa<ParenExpr>(ParentStmt) ||
          isa<ImplicitCastExpr>(ParentStmt) ||
          isa<ExprWithCleanups>(ParentStmt) ||
          isa<MaterializeTemporaryExpr>(ParentStmt) ||
          isa<CXXBindTemporaryExpr>(ParentStmt)) {
        Current = ParentStmt;
        continue;
      }

      // Bare expression statements are discarded values.
      if (isa<CompoundStmt>(ParentStmt)) {
        return CallValueUse::Discarded;
      }

      return CallValueUse::Used;
    }

    if (ParentNode.get<Decl>()) {
      return CallValueUse::Used;
    }

    return CallValueUse::Used;
  }

  return CallValueUse::Used;
}

class EtlAccessChecker : public Checker<check::PreCall, check::PostCall, check::DeadSymbols> {
  mutable std::unique_ptr<BugType> Bug;

public:
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const {
    const auto *MD = dyn_cast_or_null<CXXMethodDecl>(Call.getDecl());
    if (!MD) return;

    if (!isEtlType(MD->getParent())) return;

    std::string FuncName = MD->getNameAsString();

    if (FuncName == "value" || FuncName == "error" || 
        FuncName == "operator*" || FuncName == "operator->") {
      
      const auto *InstanceCall = dyn_cast<CXXInstanceCall>(&Call);
      if (!InstanceCall) return;

      SVal ThisVal = InstanceCall->getCXXThisVal();
      const MemRegion *Region = ThisVal.getAsRegion();
      if (!Region) return;

      Region = Region->StripCasts();

      ProgramStateRef State = C.getState();
      const EtlState *TrackedState = State->get<EtlStateMap>(Region);

      bool IsBug = false;

      // Logic for .value(), *, and ->
      if (FuncName != "error") {
        // It's a bug if we don't explicitly know it has a value
        if (!TrackedState || *TrackedState != EtlState::HasValue) {
          IsBug = true;
        }
      } 
      // Logic for .error()
      else {
        // It's a bug if we don't explicitly know it's empty/has an error
        if (!TrackedState || *TrackedState != EtlState::Empty) {
          IsBug = true;
        }
      }

      if (IsBug) {
        if (!Bug) {
          Bug = std::make_unique<BugType>(this, "Unchecked ETL Access", "C++ ETL Error Handling");
        }
        
        ExplodedNode *ErrNode = C.generateErrorNode();
        if (ErrNode) {
          auto Report = std::make_unique<PathSensitiveBugReport>(
              *Bug, "etl::expected/optional is dereferenced without a guaranteed value check", ErrNode);
          Report->markInteresting(Region);
          C.emitReport(std::move(Report));
        }
      }
    }
  }

  void checkPostCall(const CallEvent &Call, CheckerContext &C) const {
    const auto *MD = dyn_cast_or_null<CXXMethodDecl>(Call.getDecl());
    if (!MD) return;

    if (!isEtlType(MD->getParent())) return;

    std::string FuncName = MD->getNameAsString();

    // 1. Handle state-clearing methods like reset()
    if (FuncName == "reset" || FuncName == "clear") {
      const auto *InstanceCall = dyn_cast<CXXInstanceCall>(&Call);
      if (!InstanceCall) return;

      SVal ThisVal = InstanceCall->getCXXThisVal();
      const MemRegion *Region = ThisVal.getAsRegion();
      if (!Region) return;
      
      // Strip casts to ensure matching keys for references
      Region = Region->StripCasts(); 

      ProgramStateRef State = C.getState();
      State = State->set<EtlStateMap>(Region, EtlState::Empty);
      C.addTransition(State);
      return;
    }

    // 2. Intercept state-checking methods
    if (FuncName == "has_value" || FuncName == "operator bool") {
      const auto *InstanceCall = dyn_cast<CXXInstanceCall>(&Call);
      if (!InstanceCall) return;

      SVal ThisVal = InstanceCall->getCXXThisVal();
      const MemRegion *Region = ThisVal.getAsRegion();
      if (!Region) return;
      
      Region = Region->StripCasts();

      SVal RetVal = Call.getReturnValue();
      auto DefinedRetVal = RetVal.getAs<DefinedSVal>();
      if (!DefinedRetVal) return;

      ProgramStateRef State = C.getState();
      ProgramStateRef StateTrue, StateFalse;
      
      std::tie(StateTrue, StateFalse) = State->assume(*DefinedRetVal);

      if (StateTrue) {
        StateTrue = StateTrue->set<EtlStateMap>(Region, EtlState::HasValue);
        C.addTransition(StateTrue);
      }

      if (StateFalse) {
        StateFalse = StateFalse->set<EtlStateMap>(Region, EtlState::Empty);
        C.addTransition(StateFalse);
      }
    }
  }

  void checkDeadSymbols(SymbolReaper &SR, CheckerContext &C) const {
    ProgramStateRef State = C.getState();
    EtlStateMapTy TrackedMap = State->get<EtlStateMap>();

    for (auto I = TrackedMap.begin(), E = TrackedMap.end(); I != E; ++I) {
      if (!SR.isLiveRegion(I->first)) {
        State = State->remove<EtlStateMap>(I->first);
      }
    }
    C.addTransition(State);
  }
};

class EtlDiscardedExpectedChecker : public Checker<check::PostStmt<CallExpr>> {
  mutable std::unique_ptr<BugType> Bug;

public:
  void checkPostStmt(const CallExpr *Call, CheckerContext &C) const {
    QualType ReturnType = Call->getCallReturnType(C.getASTContext());
    if (!isEtlExpectedType(ReturnType)) {
      return;
    }

    const CallValueUse Use = classifyCallValueUse(Call, C.getASTContext());
    if (Use != CallValueUse::Discarded) {
      return;
    }

    if (!Bug) {
      Bug = std::make_unique<BugType>(
          this,
          "Discarded etl::expected Return Value",
          "C++ ETL Error Handling");
    }

    ExplodedNode *ErrNode = C.generateErrorNode();
    if (!ErrNode) {
      return;
    }

    auto Report = std::make_unique<PathSensitiveBugReport>(
        *Bug,
        "Return value of etl::expected is discarded; potential error information is ignored",
        ErrNode);
    Report->addRange(Call->getSourceRange());
    C.emitReport(std::move(Report));
  }
};
} // end anonymous namespace

// 3. The Plugin Registration Boilerplate
extern "C" const char clang_analyzerAPIVersionString[] = CLANG_ANALYZER_API_VERSION_STRING;

extern "C" void clang_registerCheckers(CheckerRegistry &registry) {
  registry.addChecker<EtlAccessChecker>(
      "custom.EtlAccessChecker",
      "Checks for unchecked accesses to etl::optional and etl::expected",
      "");

  registry.addChecker<EtlDiscardedExpectedChecker>(
      "custom.EtlDiscardedExpectedChecker",
      "Warns when etl::expected return values are discarded",
      "");
}
