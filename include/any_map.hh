#ifndef any_map_H_
#define any_map_H_

#include <memory>
#include <string>
#include <unordered_map>

namespace PureScript {

class unsafe_any {
  private:
  std::shared_ptr<void> ptr;

  template <typename T, typename Enable = void>
  struct Helper;

  // This specialization handles cases where type T is already a (shared) pointer
  template <typename T>
  struct Helper <T, typename std::enable_if<std::is_assignable<decltype(ptr),T>::value>::type> {
    static inline auto getPtr(const T& val) -> std::shared_ptr<void> {
      return val;
    }
    static inline auto castPtr(const std::shared_ptr<void>& ptr) -> T {
      return std::static_pointer_cast<typename T::element_type>(ptr);
    }
  };

  // This specialization handles cases where type T is NOT a pointer
  template <typename T>
  struct Helper <T, typename std::enable_if<!std::is_assignable<decltype(ptr),T>::value>::type> {
    static inline auto getPtr(const T& val) -> std::shared_ptr<void> {
      return std::make_shared<T>(val);
    }
    static inline auto castPtr(const std::shared_ptr<void>& ptr) -> T {
      return *std::static_pointer_cast<T>(ptr);
    }
  };

  public:
  template <typename T>
  unsafe_any(const T& val) : ptr(Helper<T>::getPtr(val)) {}
  unsafe_any(const char * val) : ptr(Helper<std::string>::getPtr(val)) {}

  unsafe_any() = default;
  ~unsafe_any() = default;

  template <typename T>
  inline auto cast() const -> T {
    return Helper<T>::castPtr(ptr);
  }
};

using any_map = std::unordered_map<std::string, const unsafe_any>;

} // namespace PureScript

#endif // any_map_H_
