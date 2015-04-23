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
#include "any_map.hh"

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

namespace Private {
  template <int N>
  struct Bind {
  };
}

template <int N, typename... Args>
constexpr auto bind(Args&&... args) -> decltype(Private::Bind<N>::bind(std::forward<Args>(args)...)) {
  return Private::Bind<N>::bind(std::forward<Args>(args)...);
}

#define BIND_WITH_PLACEHOLDERS(N, ...) \
namespace Private { \
  using namespace std::placeholders; \
  template <> \
  struct Bind<N> { \
    template <typename... Args> \
    static constexpr auto bind(Args&&... args) -> decltype(std::bind(std::forward<Args>(args)..., __VA_ARGS__)) { \
      return std::bind(std::forward<Args>(args)..., __VA_ARGS__); \
    } \
  }; \
}

BIND_WITH_PLACEHOLDERS( 1, _1)
BIND_WITH_PLACEHOLDERS( 2, _1, _2)
BIND_WITH_PLACEHOLDERS( 3, _1, _2, _3)
BIND_WITH_PLACEHOLDERS( 4, _1, _2, _3, _4)
BIND_WITH_PLACEHOLDERS( 5, _1, _2, _3, _4, _5)
BIND_WITH_PLACEHOLDERS( 6, _1, _2, _3, _4, _5, _6)
BIND_WITH_PLACEHOLDERS( 7, _1, _2, _3, _4, _5, _6, _7)
BIND_WITH_PLACEHOLDERS( 8, _1, _2, _3, _4, _5, _6, _7, _8)
BIND_WITH_PLACEHOLDERS( 9, _1, _2, _3, _4, _5, _6, _7, _8, _9)
BIND_WITH_PLACEHOLDERS(10, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10)

#undef BIND_WITH_PLACEHOLDERS

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

} // namespace PureScript

#endif // PureScript_HH
