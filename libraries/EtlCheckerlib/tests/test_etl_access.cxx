// RUN: clang++ -cc1 -load /path/to/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker -verify %s

// --- Minimal Mock of ETL ---
namespace etl {
  template <typename T>
  struct optional {
    bool has_value() const;
    explicit operator bool() const;
    T& value();
    T& operator*();
    T* operator->();
  };

  template <typename T, typename E>
  struct expected {
    bool has_value() const;
    explicit operator bool() const;
    T& value();
    E& error();
    T& operator*();
    T* operator->();
  };
}
// ---------------------------

struct Dummy {
    int do_something();
};

// ==========================================
// etl::optional Tests
// ==========================================

void test_optional_unsafe_direct_access() {
    etl::optional<int> opt;
    int x = opt.value(); // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
    int y = *opt;        // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void test_optional_safe_has_value() {
    etl::optional<int> opt;
    if (opt.has_value()) {
        int x = opt.value(); // No warning expected
        int y = *opt;        // No warning expected
    }
}

void test_optional_safe_bool_operator() {
    etl::optional<Dummy> opt;
    if (opt) {
        int x = opt->do_something(); // No warning expected
    }
}

void test_optional_unsafe_else_branch() {
    etl::optional<int> opt;
    if (opt) {
        // safe
    } else {
        int x = opt.value(); // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
    }
}

// ==========================================
// etl::expected Tests
// ==========================================

void test_expected_unsafe_direct_access() {
    etl::expected<int, int> exp;
    int x = exp.value(); // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
    int e = exp.error(); // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
}

void test_expected_safe_value_access() {
    etl::expected<int, int> exp;
    if (exp.has_value()) {
        int x = exp.value(); // No warning expected
    }
}

void test_expected_safe_error_access() {
    etl::expected<int, int> exp;
    if (!exp) {
        int e = exp.error(); // No warning expected
    }
}

void test_expected_unsafe_wrong_branch() {
    etl::expected<int, int> exp;
    if (exp) {
        int e = exp.error(); // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
    } else {
        int x = exp.value(); // expected-warning{{etl::expected/optional is dereferenced without a guaranteed value check}}
    }
}
