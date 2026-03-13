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

set(ETL_ANALYZER_CHECKERS
    "custom.EtlAccessChecker,custom.EtlDiscardedExpectedChecker,custom.EtlDiscardedOptionalChecker")

set(ETL_VERIFY_TEST_FILES
    test_access_checker.cxx
    test_discarded_expected_checker.cxx
    test_discarded_optional_checker.cxx)

# clang++ -cc1 -load build_clang/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=${ETL_ANALYZER_CHECKERS} libraries/EtlCheckerlib/tests/<file>.cxx -verify
foreach(TEST_FILE IN LISTS ETL_VERIFY_TEST_FILES)
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_test(NAME ${TEST_NAME}
        COMMAND clang++ -cc1 -load ${CMAKE_BINARY_DIR}/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=${ETL_ANALYZER_CHECKERS} ${CMAKE_SOURCE_DIR}/libraries/EtlCheckerlib/tests/${TEST_FILE} -verify
    )
endforeach()