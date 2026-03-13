// RUN: clang++ -cc1 -load %/path/to/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker,custom.EtlDiscardedExpectedChecker,custom.EtlDiscardedOptionalChecker %s -verify

#include "test_etl_mocks.h"

struct ExpectedFactory {
  etl::expected<int, int> build() const;
};

// ==========================================
// Positive Cases: Discarded etl::expected
// ==========================================

void discarded_expected_direct() {
  make_expected();
  // expected-warning@-1 {{Return value of etl::expected is discarded; potential error information is ignored}}
}

void discarded_expected_parenthesized() {
  ((make_expected()));
  // expected-warning@-1 {{Return value of etl::expected is discarded; potential error information is ignored}}
}

void discarded_expected_inside_branch(bool enabled) {
  if (enabled) {
    make_expected();
    // expected-warning@-1 {{Return value of etl::expected is discarded; potential error information is ignored}}
  }
}

void discarded_expected_member_call(ExpectedFactory factory) {
  factory.build();
  // expected-warning@-1 {{Return value of etl::expected is discarded; potential error information is ignored}}
}

void discarded_expected_callback_call() {
  etl::expected<int, int> (*callback)() = make_expected;
  callback();
  // expected-warning@-1 {{Return value of etl::expected is discarded; potential error information is ignored}}
}

// ==========================================
// Negative Cases: Value Is Consumed
// ==========================================

void explicit_discard_expected_c_style() {
  (void)make_expected(); // no-warning
}

void explicit_discard_expected_static_cast() {
  static_cast<void>(make_expected()); // no-warning
}

void assigned_expected_auto() {
  auto value = make_expected(); // no-warning
  (void)value;
}

void assigned_expected_existing_object(etl::expected<int, int> target) {
  target = make_expected(); // no-warning
}

etl::expected<int, int> return_expected_result() {
  return make_expected(); // no-warning
}

void expected_as_function_argument() {
  consume_expected(make_expected()); // no-warning
}

void expected_used_in_if_condition() {
  if (make_expected()) {
    return;
  }
}

void expected_used_in_while_condition() {
  while (make_expected()) {
    return;
  }
}

void expected_used_in_ternary(bool condition) {
  auto value = condition ? make_expected() : make_expected();
  (void)value;
}

void expected_used_in_logical_expressions(bool lhs) {
  if (lhs && make_expected()) {
    return;
  }

  if (!lhs || make_expected()) {
    return;
  }
}

// ==========================================
// Namespace Isolation
// ==========================================

namespace my_custom_lib {
template <typename T, typename E>
struct expected {};
} // namespace my_custom_lib

my_custom_lib::expected<int, int> make_custom_expected();

void ignore_non_etl_expected() {
  make_custom_expected(); // no-warning
}
