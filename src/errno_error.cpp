/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#include "wormhole/sysinfo/errno_error.hpp"

#include <cstring>

namespace
{
struct errno_error_cat : std::error_category
{
  [[nodiscard]] char const* name() const noexcept override
  {
    return "errno";
  }

  [[nodiscard]] std::string message(int val) const override
  {
    return strerror(val);
  }
};
const errno_error_cat errnoErrorCat;
}  // namespace

namespace wormhole::sysinfo
{
std::error_code make_error_code(errno_errc val)
{
  return {static_cast<int>(val), errnoErrorCat};
}
}  // namespace wormhole::sysinfo
