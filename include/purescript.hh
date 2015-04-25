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

// Type aliases

template <typename A, typename B>
using fn = std::function<B(A)>;

template <typename T>
using managed = std::shared_ptr<T>;

template <typename T>
using list = shared_list<T>;

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
auto construct(ArgTypes... args) ->
    typename std::enable_if<std::is_assignable<std::shared_ptr<void>,T>::value,T>::type {
  return std::make_shared<typename T::element_type>(args...);
}

template <typename T, typename... ArgTypes>
constexpr auto construct(ArgTypes... args) ->
    typename std::enable_if<!std::is_assignable<std::shared_ptr<void>,T>::value,std::shared_ptr<T>>::type {
  return std::make_shared<T>(args...);
}

template <typename Ctor, typename... CArgs, typename... Args>
constexpr auto constructor(Args&&... args) ->
    decltype(Private::Bind<sizeof...(CArgs) - sizeof...(Args)>::bind(construct<Ctor, CArgs...>,std::forward<Args>(args)...)) {
  return Private::Bind<sizeof...(CArgs) - sizeof...(Args)>::bind(construct<Ctor, CArgs...>, std::forward<Args>(args)...);
}

template <typename T, typename U>
constexpr auto instance_of(const std::shared_ptr<U>& a) ->
    typename std::enable_if<std::is_assignable<std::shared_ptr<void>,T>::value,T>::type {
  return std::dynamic_pointer_cast<typename T::element_type>(a);
}

template <typename T, typename U>
constexpr auto instance_of(const std::shared_ptr<U> a) ->
  typename std::enable_if<!std::is_assignable<std::shared_ptr<void>,T>::value,std::shared_ptr<T>>::type {
  return std::dynamic_pointer_cast<T>(a);
}

template <typename T, typename U>
constexpr auto get(const std::shared_ptr<U>& a) -> typename T::element_type {
  return *std::dynamic_pointer_cast<typename T::element_type>(a);
}

template <typename U>
constexpr auto get(const std::shared_ptr<U>& a) -> U {
  return *std::dynamic_pointer_cast<U>(a);
}

} // namespace PureScript

#endif // PureScript_HH
