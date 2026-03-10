#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Frontend/CheckerRegistry.h"

using namespace clang;
using namespace ento;

// 1. Define the states an etl::optional/expected can be in
enum class EtlState { Unknown, HasValue, Empty };

// 2. Register a custom trait map in the ProgramState
// This maps the memory region of the object to its current known state.
REGISTER_MAP_WITH_PROGRAMSTATE(EtlStateMap, const MemRegion *, EtlState)

namespace {
class EtlAccessChecker : public Checker<check::PreCall, check::PostCall, check::DeadSymbols> {
  mutable std::unique_ptr<BugType> Bug;

public:
  // Runs BEFORE a function call is evaluated
  void checkPreCall(const CallEvent &Call, CheckerContext &C) const {
    if (!Call.isGlobalCFunction() && Call.getCalleeIdentifier()) {
      StringRef FuncName = Call.getCalleeIdentifier()->getName();

      // Intercept .value(), .error(), operator*, operator->
      if (FuncName == "value" || FuncName == "error" || 
          FuncName == "operator*" || FuncName == "operator->") {
        
        const auto *InstanceCall = dyn_cast<CXXInstanceCall>(&Call);
        if (!InstanceCall) return;

        // Get the memory region of the object being called
        SVal ThisVal = InstanceCall->getCXXThisVal();
        const MemRegion *Region = ThisVal.getAsRegion();
        if (!Region) return;

        // Check our custom state map
        ProgramStateRef State = C.getState();
        const EtlState *TrackedState = State->get<EtlStateMap>(Region);

        // If the state is Empty or we never checked it (Unknown), emit a warning!
        if (!TrackedState || *TrackedState != EtlState::HasValue) {
          if (!Bug) {
            Bug = std::make_unique<BugType>(this, "Unchecked ETL Access", "C++ ETL Error Handling");
          }
          
          ExplodedNode *ErrNode = C.generateErrorNode();
          if (ErrNode) {
            auto Report = std::make_unique<PathSensitiveBugReport>(
                *Bug, "etl::expected/optional is dereferenced without a guaranteed value check.", ErrNode);
            Report->markInteresting(Region);
            C.emitReport(std::move(Report));
          }
        }
      }
    }
  }

  // Runs AFTER a function call is evaluated
  void checkPostCall(const CallEvent &Call, CheckerContext &C) const {
    // TODO: This is where you intercept .has_value() or operator bool().
    // You will split the ProgramState here: 
    // State 1 (Return True) -> map Region to EtlState::HasValue
    // State 2 (Return False) -> map Region to EtlState::Empty
  }

  // Cleans up tracked regions when they go out of scope to save memory
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
} // end anonymous namespace

// 3. The Plugin Registration Boilerplate
extern "C" const char clang_analyzerAPIVersionString[] = CLANG_ANALYZER_API_VERSION_STRING;

extern "C" void clang_registerCheckers(CheckerRegistry &registry) {
  registry.addChecker<EtlAccessChecker>(
      "custom.EtlAccessChecker",
      "Checks for unchecked accesses to etl::optional and etl::expected",
      "");
}
