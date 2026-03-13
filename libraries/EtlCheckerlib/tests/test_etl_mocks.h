#ifndef ETL_CHECKER_TEST_ETL_MOCKS_H
#define ETL_CHECKER_TEST_ETL_MOCKS_H

namespace std {
template <typename T>
constexpr T&& forward(T& type) noexcept {
  return static_cast<T&&>(type);
}

template <typename T>
constexpr T&& forward(T&& type) noexcept {
  return static_cast<T&&>(type);
}
} // namespace std

namespace etl {
template <typename T>
struct optional {
  optional();
  explicit optional(const T &value);

  bool has_value() const;
  explicit operator bool() const;

  T &value();
  const T &value() const;

  T &operator*();
  const T &operator*() const;

  T *operator->();
  const T *operator->() const;

  template <typename... Args>
  T &emplace(Args &&...args);

  optional<T> &operator=(const optional<T> &other);
  optional<T> &operator=(T value);

  void reset();
  void clear();
  void swap(optional<T> &other);
};

template <typename T, typename E>
struct expected {
  bool has_value() const;
  explicit operator bool() const;

  T &value();
  const T &value() const;

  E &error();
  const E &error() const;

  T &operator*();
  const T &operator*() const;

  T *operator->();
  const T *operator->() const;
};
} // namespace etl

struct Foo {
  void do_something() const {}
};

etl::expected<int, int> make_expected();
etl::optional<int> make_optional();
void consume_expected(etl::expected<int, int> value);
void consume_optional(etl::optional<int> value);

void assertion_handler() __attribute__((analyzer_noreturn));

#endif
