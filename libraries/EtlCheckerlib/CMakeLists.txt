# Find LLVM and Clang
find_package(LLVM REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LLVM_DIR})
include(AddLLVM)

find_package(Clang REQUIRED)

# Build as a shared module (.so or .dylib)
add_library(EtlChecker MODULE source/EtlChecker.cxx)

target_compile_definitions(EtlChecker PUBLIC ${LLVM_DEFINITIONS})
target_include_directories(EtlChecker PUBLIC ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})

# Link against the necessary Clang Analyzer libraries
target_link_libraries(EtlChecker PRIVATE
    clangStaticAnalyzerCore
    clangStaticAnalyzerFrontend
    clangAST
    clangBasic
)

# LLVM requires RTTI to be disabled in most builds, but standard C++ needs it.
# Usually, matching your LLVM build's RTTI setting is required.
if(NOT LLVM_ENABLE_RTTI)
    set_target_properties(EtlChecker PROPERTIES COMPILE_FLAGS "-fno-rtti")
endif()
