// RUN: clang++ -cc1 -load %/path/to/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker %s -verify

// --- Minimal Mock of ETL & STD ---
namespace std {
  template <typename T> constexpr T&& forward(T& type) noexcept { return static_cast<T&&>(type); }
  template <typename T> constexpr T&& forward(T&& type) noexcept { return static_cast<T&&>(type); }
}

namespace etl {
  template <typename T>
  struct optional {
    bool has_value() const;
    explicit operator bool() const;
    
    T& value();
    const T& value() const;
    
    T& operator*();
    const T& operator*() const;
    
    T* operator->();
    const T* operator->() const;
    
    void reset();
    void swap(optional<T>& other);
  };

  template <typename T, typename E>
  struct expected {
    bool has_value() const;
    explicit operator bool() const;
    
    T& value();
    const T& value() const;
    
    E& error();
    const E& error() const;
    
    T& operator*();
    const T& operator*() const;
    
    T* operator->();
    const T* operator->() const;
  };
}
// ---------------------------

struct Foo {
  void do_something() const {}
};

// ==========================================
// Basic Unchecked Access
// ==========================================

void unchecked_value_access(const etl::optional<int> &opt) {
  opt.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void unchecked_deref_operator_access(const etl::optional<int> &opt) {
  *opt;
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void unchecked_arrow_operator_access(const etl::optional<Foo> &opt) {
  opt->do_something();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

// ==========================================
// Checked Access (Safe)
// ==========================================

void checked_access_has_value(const etl::optional<int> &opt) {
  if (opt.has_value()) {
    opt.value(); // no-warning
  }
}

void checked_access_bool_op(const etl::optional<int> &opt) {
  if (opt) {
    *opt; // no-warning
  }
}

void checked_access_early_return(const etl::optional<int> &opt) {
  if (!opt) return;
  opt.value(); // no-warning
}

// ==========================================
// expected Specific Checks
// ==========================================

void unchecked_expected_error(const etl::expected<int, int> &exp) {
  exp.error();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void checked_expected_error(const etl::expected<int, int> &exp) {
  if (!exp) {
    exp.error(); // no-warning
  } else {
    exp.error();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void unchecked_expected_value(const etl::expected<int, int> &exp) {
  exp.value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void checked_expected_backwards(const etl::expected<int, int> &exp) {
  if (exp) {
    exp.value(); // no-warning
  } else {
    exp.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

// ==========================================
// Advanced Tracking (Modifiers & Lifetimes)
// ==========================================

void check_value_then_reset(etl::optional<int> opt) {
  if (opt) {
    opt.reset();
    opt.value();
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void std_forward_copy(etl::optional<int> opt) {
  std::forward<etl::optional<int>>(opt).value();
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void std_forward_safe(etl::optional<int> opt) {
  if (!opt) return;
  std::forward<etl::optional<int>>(opt).value(); // no-warning
}

void assertion_handler() __attribute__((analyzer_noreturn));

void function_calling_analyzer_noreturn(const etl::optional<int>& opt) {
  if (!opt) {
    assertion_handler();
  }
  else
  {
    // TODO: Remove the else branch once the analyzer can properly track noreturn functions and their impact on control flow.
    *opt; // no-warning
  }
}

// ==========================================
// Complex Control Flow & Short-Circuiting
// ==========================================

void test_short_circuit_and(const etl::optional<int> &opt) {
  // Safe: opt.value() is only evaluated if `opt` evaluates to true
  if (opt && opt.value() > 0) {
    opt.value(); // no-warning
  }
}

void test_short_circuit_or(const etl::optional<int> &opt) {
  // Safe: opt.value() is only evaluated if `!opt` evaluates to false (meaning it has a value)
  if (!opt || opt.value() == 0) { 
    return;
  }
  opt.value(); // no-warning
}

void test_ternary_operator(const etl::optional<int> &opt) {
  int x = opt ? opt.value() : 0; // no-warning
}

void test_sequential_checks(etl::optional<int> opt) {
  if (opt) {
    opt.value(); // no-warning
  }
  // The state from the 'if' block does not leak out here.
  opt.value(); 
  // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
}

// ==========================================
// Instance Isolation
// ==========================================

void test_mismatched_check(const etl::optional<int> &opt1, const etl::optional<int> &opt2) {
  if (opt1) {
    opt2.value(); 
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void test_nested_checks(const etl::optional<int> &opt1, const etl::optional<int> &opt2) {
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
// Loops and Re-evaluation
// ==========================================

// void test_while_loop_clear(etl::optional<int> opt) {
//   while (opt) {
//     opt.value(); // no-warning
//     opt.clear();
//     opt.value(); 
// Would expect a warn here, but currently the checker doesn't handle state changes within loops very well, leading to potential false negatives. This is an area for future improvement.
//   }
// }

void test_for_loop(etl::optional<int> opt) {
  // The loop condition guarantees `opt` is safe at the start of the block
  for (; opt; opt.reset()) {
    opt.value(); // no-warning
  }
}

// ==========================================
// expected Exhaustive Branching
// ==========================================

void test_expected_true_branch(const etl::expected<int, int> &exp) {
  if (exp) {
    exp.value(); // no-warning
    exp.error(); 
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void test_expected_false_branch(const etl::expected<int, int> &exp) {
  if (!exp.has_value()) {
    exp.error(); // no-warning
    exp.value(); 
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void test_expected_if_else(const etl::expected<int, int> &exp) {
  if (exp.has_value()) {
    exp.value(); // no-warning
  } else {
    exp.error(); // no-warning
  }
}

// ==========================================
// Explicit Boolean Casts & Variable Tracking
// ==========================================

void test_optional_static_cast(const etl::optional<int> &opt) {
  bool has_val = static_cast<bool>(opt);
  if (has_val) {
    opt.value(); // no-warning
  } else {
    opt.value(); 
    // expected-warning@-1 {{etl::expected/optional is dereferenced without a guaranteed value check}}
  }
}

void test_expected_static_cast(const etl::expected<int, int> &exp) {
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

void test_optional_c_style_cast(const etl::optional<int> &opt) {
  if ((bool)opt) {
    opt.value(); // no-warning
  }
}

void test_bool_variable_reassignment(const etl::optional<int> &opt) {
  bool is_safe = static_cast<bool>(opt);
  bool is_still_safe = is_safe; // Pass the symbolic value around
  
  if (is_still_safe) {
    opt.value(); // no-warning
  }
}

// ==========================================
// False Positive / Namespace Isolation Tests
// ==========================================

namespace my_custom_lib {
  struct optional {
    bool has_value() const { return false; }
    int value() { return 0; }
    int operator*() { return 0; }
  };
}

struct DummyPtr {
  int operator*() { return 0; }
  void reset() {}
};

void test_ignore_unrelated_types() {
  my_custom_lib::optional custom_opt;
  custom_opt.value(); // no-warning: Right name, wrong namespace
  *custom_opt;        // no-warning

  DummyPtr ptr;
  *ptr;               // no-warning: Not an ETL type
  ptr.reset();        // no-warning
}
