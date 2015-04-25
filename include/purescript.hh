#ifndef PureScript_HH
#define PureScript_HH

// Standard includes
//
#include <functional>
#include <string>
#include <stdexcept>
#include "bind.hh"
#include "memory.hh"
#include "any_map.hh"
#include "shared_list.hh"

namespace PureScript {

template <typename A, typename B>
using fn = std::function<B(A)>;

template <typename B>
using eff_fn = std::function<B()>;

using string = std::string;

using runtime_error = std::runtime_error;

template <typename T>
using list = shared_list<T>;

using list_index_type = list<void*>::size_type;

} // namespace PureScript

#endif // PureScript_HH
