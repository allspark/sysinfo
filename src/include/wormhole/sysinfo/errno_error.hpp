/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#pragma once

#include <system_error>
#include <type_traits>

namespace wormhole::sysinfo
{
enum struct errno_errc : int
{
};

std::error_code make_error_code(errno_errc);
}  // namespace wormhole::sysinfo

template <>
struct std::is_error_code_enum<wormhole::sysinfo::errno_errc> : std::true_type
{
};
