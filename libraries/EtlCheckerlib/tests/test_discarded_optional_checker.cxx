// RUN: clang++ -cc1 -load %/path/to/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker,custom.EtlDiscardedExpectedChecker,custom.EtlDiscardedOptionalChecker %s -verify

#include "test_etl_mocks.h"

struct OptionalFactory {
  etl::optional<int> build() const;
};

// ==========================================
// Positive Cases: Discarded etl::optional
// ==========================================

void discarded_optional_direct() {
  make_optional();
  // expected-warning@-1 {{Return value of etl::optional is discarded; potential missing-value handling is ignored}}
}

void discarded_optional_parenthesized() {
  ((make_optional()));
  // expected-warning@-1 {{Return value of etl::optional is discarded; potential missing-value handling is ignored}}
}

void discarded_optional_inside_branch(bool enabled) {
  if (enabled) {
    make_optional();
    // expected-warning@-1 {{Return value of etl::optional is discarded; potential missing-value handling is ignored}}
  }
}

void discarded_optional_member_call(OptionalFactory factory) {
  factory.build();
  // expected-warning@-1 {{Return value of etl::optional is discarded; potential missing-value handling is ignored}}
}

void discarded_optional_callback_call() {
  etl::optional<int> (*callback)() = make_optional;
  callback();
  // expected-warning@-1 {{Return value of etl::optional is discarded; potential missing-value handling is ignored}}
}

// ==========================================
// Negative Cases: Value Is Consumed
// ==========================================

void explicit_discard_optional_c_style() {
  (void)make_optional(); // no-warning
}

void explicit_discard_optional_static_cast() {
  static_cast<void>(make_optional()); // no-warning
}

void assigned_optional_auto() {
  auto value = make_optional(); // no-warning
  (void)value;
}

void assigned_optional_existing_object(etl::optional<int> target) {
  target = make_optional(); // no-warning
}

etl::optional<int> return_optional_result() {
  return make_optional(); // no-warning
}

void optional_as_function_argument() {
  consume_optional(make_optional()); // no-warning
}

void optional_used_in_if_condition() {
  if (make_optional()) {
    return;
  }
}

void optional_used_in_while_condition() {
  while (make_optional()) {
    return;
  }
}

void optional_used_in_ternary(bool condition) {
  auto value = condition ? make_optional() : make_optional();
  (void)value;
}

void optional_used_in_logical_expressions(bool lhs) {
  if (lhs && make_optional()) {
    return;
  }

  if (!lhs || make_optional()) {
    return;
  }
}

// ==========================================
// Namespace Isolation
// ==========================================

namespace my_custom_lib {
template <typename T>
struct optional {};
} // namespace my_custom_lib

my_custom_lib::optional<int> make_custom_optional();

void ignore_non_etl_optional() {
  make_custom_optional(); // no-warning
}
