#ifndef PureScript_HH
#define PureScript_HH

// Standard includes
//
#include <functional>
#include <memory>
#include <string>
#include <iostream>
#include <stdexcept>
#include "shared_list.hh"

namespace PureScript {

// Type support

template <typename T, typename Enable = void>
struct ADT;

template <typename T>
struct ADT <T, typename std::enable_if<std::is_fundamental<T>::value>::type> {
  using type = T;
  template <typename... ArgTypes>
  constexpr static auto make(ArgTypes... args) -> type {
    return T(args...);
  }
};

template <typename T>
struct ADT <T, typename std::enable_if<!std::is_fundamental<T>::value>::type> {
  using type = std::shared_ptr<T>;
  template <typename... ArgTypes>
  constexpr static auto make(ArgTypes... args) -> type {
    return std::make_shared<T>(args...);
  }
};

// Type aliases
//
template <typename A, typename B> using fn = std::function<B(A)>;
template <typename T> using data = typename ADT<T>::type;
template <typename T> using list = shared_list<T>;
using list_index_type = list<void*>::size_type;
using string = std::string;
template <typename B> using eff_fn = std::function<B()>;
using runtime_error = std::runtime_error;

// Function aliases

#define DECLARE_BIND(N, ...) \
template <typename... Args> \
constexpr auto bind ## N(Args&&... args) -> decltype(std::bind(std::forward<Args>(args)..., __VA_ARGS__)) { \
  return std::bind(std::forward<Args>(args)..., __VA_ARGS__); \
} \

#define PLACEHOLDER(N) std::forward<decltype(std::placeholders::_ ## N)>(std::placeholders::_ ## N)

DECLARE_BIND(1, PLACEHOLDER(1))
DECLARE_BIND(2, PLACEHOLDER(1), PLACEHOLDER(2))
DECLARE_BIND(3, PLACEHOLDER(1), PLACEHOLDER(2), PLACEHOLDER(3))
DECLARE_BIND(4, PLACEHOLDER(1), PLACEHOLDER(2), PLACEHOLDER(3), PLACEHOLDER(4))
DECLARE_BIND(5, PLACEHOLDER(1), PLACEHOLDER(2), PLACEHOLDER(3), PLACEHOLDER(4), PLACEHOLDER(5))
DECLARE_BIND(6, PLACEHOLDER(1), PLACEHOLDER(2), PLACEHOLDER(3), PLACEHOLDER(4), PLACEHOLDER(5), PLACEHOLDER(6))
DECLARE_BIND(7, PLACEHOLDER(1), PLACEHOLDER(2), PLACEHOLDER(3), PLACEHOLDER(4), PLACEHOLDER(5), PLACEHOLDER(6), PLACEHOLDER(7))

#undef DECLARE_BIND
#undef PLACEHOLDER

template <typename T, typename... ArgTypes>
constexpr auto construct(ArgTypes... args) -> typename ADT<T>::type {
  return ADT<T>::make(args...);
}

template <typename T, typename U>
constexpr auto cast(const std::shared_ptr<U>& a) -> T {
  return *(std::dynamic_pointer_cast<T>(a));
}

template <typename T, typename U>
constexpr auto instanceof(const std::shared_ptr<U>& a) -> std::shared_ptr<T> {
  return std::dynamic_pointer_cast<T>(a);
}

// Records support
//
#define RECORD(...) \
  []() {            \
    struct {        \
      __VA_ARGS__;  \
    } s;            \
    return construct<decltype(s)>(); \
  }()

#define DATA_RECORD(data_type, ...) \
  []() { \
    auto _ = construct<data_type>(); \
    __VA_ARGS__; \
    return _;    \
  }()

}

#endif // PureScript_HH
