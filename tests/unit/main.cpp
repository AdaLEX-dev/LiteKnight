// Soul Knight Linux — Unit Test Main
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// This file just ensures doctest is properly linked
// Actual tests are in separate files

TEST_CASE("Build system works" * doctest::test_suite("unit")) {
    CHECK(true);
}