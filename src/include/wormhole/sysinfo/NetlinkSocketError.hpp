/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#pragma once

#include <system_error>

namespace wormhole::sysinfo::Netlink
{
enum class SocketError
{
  None,
  Busy,
  Interrupted,
  Error,
  WrongMessageLength,
  InvalidFamily,
  NotMainTable,
  Noop,
  NoActiveRequest,
  MessageIdMismatch,
  MessageTypeMismatch,
  UnhandledMessageType,
};
std::error_code make_error_code(SocketError);
}  // namespace wormhole::sysinfo::Netlink

template <>
struct std::is_error_code_enum<wormhole::sysinfo::Netlink::SocketError> : true_type
{
};
