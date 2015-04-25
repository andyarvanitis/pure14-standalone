#ifndef PureScript_HH
#define PureScript_HH

// Standard includes
//
#include <functional>
#include <memory>
#include <string>
#include <iostream>
#include <stdexcept>
#include "bind.hh"
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
