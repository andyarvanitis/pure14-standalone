#ifndef any_map_H_
#define any_map_H_

#include <string>
#include <unordered_map>
#include "memory.hh"

namespace PureScript {

using any_map = std::unordered_map<std::string, const unsafe_any>;

} // namespace PureScript

#endif // any_map_H_
