// RUN: clang++ -cc1 -load %/path/to/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker,custom.EtlDiscardedExpectedChecker,custom.EtlDiscardedOptionalChecker %s -verify

#include "test_etl_mocks.h"

// ==========================================
// Basic Unchecked Access
// ==========================================

void unchecked_optional_value_access(const etl::optional<int> &opt) {
  opt.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void unchecked_optional_deref_operator_access(const etl::optional<int> &opt) {
  *opt;
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void unchecked_optional_arrow_operator_access(const etl::optional<Foo> &opt) {
  opt->do_something();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void unchecked_expected_error_access(const etl::expected<int, int> &exp) {
  exp.error();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void unchecked_expected_value_access(const etl::expected<int, int> &exp) {
  exp.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

// ==========================================
// Checked Access
// ==========================================

void checked_optional_access_has_value(const etl::optional<int> &opt) {
  if (opt.has_value()) {
    opt.value(); // no-warning
  }
}

void checked_optional_access_bool_op(const etl::optional<int> &opt) {
  if (opt) {
    *opt; // no-warning
  }
}

void checked_optional_access_early_return(const etl::optional<int> &opt) {
  if (!opt) {
    return;
  }
  opt.value(); // no-warning
}

void checked_expected_error(const etl::expected<int, int> &exp) {
  if (!exp) {
    exp.error(); // no-warning
  } else {
    exp.error();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void checked_expected_value(const etl::expected<int, int> &exp) {
  if (exp) {
    exp.value(); // no-warning
  } else {
    exp.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void checked_expected_if_else(const etl::expected<int, int> &exp) {
  if (exp.has_value()) {
    exp.value(); // no-warning
  } else {
    exp.error(); // no-warning
  }
}

// ==========================================
// Optional State Mutation Tracking
// ==========================================

void optional_default_constructor_is_empty() {
  etl::optional<int> opt;
  opt.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void optional_constructor_with_value_is_safe() {
  etl::optional<int> opt(7);
  opt.value(); // no-warning
}

void optional_emplace_sets_has_value(etl::optional<int> opt) {
  opt.emplace(11);
  opt.value(); // no-warning
}

void optional_reset_sets_empty(etl::optional<int> opt) {
  if (opt) {
    opt.reset();
    opt.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void optional_clear_sets_empty(etl::optional<int> opt) {
  if (opt) {
    opt.clear();
    opt.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void optional_assignment_from_value_sets_has_value(etl::optional<int> opt) {
  opt = 13;
  opt.value(); // no-warning
}

void optional_assignment_from_checked_source(etl::optional<int> src,
                                             etl::optional<int> dst) {
  if (src) {
    dst = src;
    dst.value(); // no-warning
  }
}

void optional_assignment_from_unknown_source(etl::optional<int> src,
                                             etl::optional<int> dst) {
  dst = src;
  dst.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void optional_swap_transfers_state(etl::optional<int> opt1,
                                   etl::optional<int> opt2) {
  if (opt1) {
    opt1.swap(opt2);
    opt2.value(); // no-warning
    opt1.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void optional_self_swap_keeps_state(etl::optional<int> opt) {
  if (opt) {
    opt.swap(opt);
    opt.value(); // no-warning
  }
}

// ==========================================
// Complex Control Flow
// ==========================================

void short_circuit_and_is_safe(const etl::optional<int> &opt) {
  if (opt && opt.value() > 0) {
    opt.value(); // no-warning
  }
}

void short_circuit_or_is_safe(const etl::optional<int> &opt) {
  if (!opt || opt.value() == 0) {
    return;
  }
  opt.value(); // no-warning
}

void ternary_operator_is_safe(const etl::optional<int> &opt) {
  int x = opt ? opt.value() : 0;
  (void)x;
}

void sequential_checks_do_not_leak_state(etl::optional<int> opt) {
  if (opt) {
    opt.value(); // no-warning
  }

  opt.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void for_loop_condition_sets_safe_state(etl::optional<int> opt) {
  for (; opt; opt.reset()) {
    opt.value(); // no-warning
  }
}

void std_forward_copy_is_unsafe(etl::optional<int> opt) {
  std::forward<etl::optional<int>>(opt).value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void std_forward_after_check_is_safe(etl::optional<int> opt) {
  if (!opt) {
    return;
  }

  std::forward<etl::optional<int>>(opt).value(); // no-warning
}

void analyzer_noreturn_guard_is_safe(const etl::optional<int> &opt) {
  if (!opt) {
    assertion_handler();
  } else {
    *opt; // no-warning
  }
}

// ==========================================
// Boolean Casts and Symbol Tracking
// ==========================================

void optional_static_cast_tracking(const etl::optional<int> &opt) {
  bool has_val = static_cast<bool>(opt);
  if (has_val) {
    opt.value(); // no-warning
  } else {
    opt.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void expected_static_cast_tracking(const etl::expected<int, int> &exp) {
  bool is_valid = static_cast<bool>(exp);
  if (is_valid) {
    exp.value(); // no-warning
    exp.error();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  } else {
    exp.error(); // no-warning
    exp.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void bool_variable_reassignment_tracks_symbol(const etl::optional<int> &opt) {
  bool is_safe = static_cast<bool>(opt);
  bool still_safe = is_safe;
  if (still_safe) {
    opt.value(); // no-warning
  }
}

// ==========================================
// Instance Isolation
// ==========================================

void mismatched_check_on_other_instance(const etl::optional<int> &opt1,
                                        const etl::optional<int> &opt2) {
  if (opt1) {
    opt2.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void nested_checks_track_each_instance(const etl::optional<int> &opt1,
                                       const etl::optional<int> &opt2) {
  if (opt1) {
    if (opt2) {
      opt1.value(); // no-warning
      opt2.value(); // no-warning
    }
    opt1.value(); // no-warning
    opt2.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

// ==========================================
// Namespace Isolation
// ==========================================

namespace my_custom_lib {
struct optional {
  bool has_value() const { return false; }
  int value() { return 0; }
  int operator*() { return 0; }
};
} // namespace my_custom_lib

struct DummyPtr {
  int operator*() { return 0; }
  void reset() {}
};

void ignore_unrelated_types() {
  my_custom_lib::optional custom_opt;
  custom_opt.value(); // no-warning: Right name, wrong namespace
  *custom_opt;        // no-warning

  DummyPtr ptr;
  *ptr;   // no-warning: Not an ETL type
  ptr.reset();
}
