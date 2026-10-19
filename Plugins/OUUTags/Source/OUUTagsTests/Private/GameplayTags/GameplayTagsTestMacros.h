// Copyright (c) 2025 Jonas Reich & Contributors

#pragma once

#include "Misc/AutomationTest.h"

/**
 * Shorthands for calling test functions for use in specs.
 *
 * The macros do not introduce any hidden logic (early returns, etc), but they auto-fill the test function description
 * which is passed as "What" parameter. The aim when using these macros is to enforce that test descriptions should be
 * part of the expectations formulated using Describe and It, so the actual test calls must not need any additional
 * descriptions.
 *
 * Whenever you run into instances where the description of the test case is not sufficient to explain the call of test
 * functions, it's a good sign that you should either rephrase the expectation description or break the test case down
 * into separate smaller test cases that are easy to describe.
 */
// clang-format off
#define SPEC_TEST_TRUE(Actual)                                 TestTrue(TEXT(UE_STRINGIZE(Actual)), Actual)
#define SPEC_TEST_FALSE(Actual)                                TestFalse(TEXT(UE_STRINGIZE(Actual)), Actual)
#define SPEC_TEST_EQUAL(Actual, Expected)                      TestEqual(TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
#define SPEC_TEST_NOT_EQUAL(Actual, Expected)                  TestNotEqual(TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
#define SPEC_TEST_EQUAL_TOLERANCE(Actual, Expected, Tolerance) TestEqual(TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected, Tolerance)
#define SPEC_TEST_EQUAL_INSENSITIVE(Actual, Expected)          TestEqualInsensitive(TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
#define SPEC_TEST_SAME(Actual, Expected)                       TestSame(TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
#define SPEC_TEST_NOT_SAME(Actual, Expected)                   TestNotSame(TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
#define SPEC_TEST_VALID(Actual)                                TestValid(TEXT(UE_STRINGIZE(Actual)), Actual)
#define SPEC_TEST_INVALID(Actual)                              TestInvalid(TEXT(UE_STRINGIZE(Actual)), Actual)
#define SPEC_TEST_NULL(Actual)                                 TestNull(TEXT(UE_STRINGIZE(Actual)), Actual)
#define SPEC_TEST_NOT_NULL(Actual)                             TestNotNull(TEXT(UE_STRINGIZE(Actual)), Actual)
#define SPEC_TEST_ARRAYS_EQUAL(Actual, Expected)               TestArraysEqual(*this, TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
#define SPEC_TEST_ARRAYS_MATCH_UNORDERED(Actual, Expected)     TestUnorderedArraysMatch(*this, TEXT(UE_STRINGIZE(Actual vs Expected)), Actual, Expected)
// clang-format on
