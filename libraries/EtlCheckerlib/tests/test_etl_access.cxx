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
