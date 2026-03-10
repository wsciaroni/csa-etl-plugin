# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.25)

project("EtlAnalyzerPlugin")

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 17)

# To organize projects in VisualStudio:
if(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif(MSVC)

# include(cmake/EnableGTest.cmake)
# enable_gtest()
include(CTest)

include(cmake/AddCommonLibraryDirectory.cmake)
option(COMMON_LIB_GLOBAL_USE_CTEST_ONLY "Force all common libraries to use CTest only (skip gtest discovery)" OFF)

# Include sub-projects.
add_subdirectory ("${CMAKE_CURRENT_LIST_DIR}/libraries")
# add_subdirectory ("${CMAKE_CURRENT_LIST_DIR}/applications")

# add_subdirectory ("${CMAKE_CURRENT_LIST_DIR}/tests")

add_test(NAME "Test"
    COMMAND clang++ -cc1 -load ${CMAKE_BINARY_DIR}/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker ${CMAKE_SOURCE_DIR}/libraries/EtlCheckerlib/tests/test_etl_access.cxx
)