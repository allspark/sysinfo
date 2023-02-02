/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#include "wormhole/sysinfo/NetlinkSocketError.hpp"

namespace
{
struct NetlinkSocketError_cat : std::error_category
{
  [[nodiscard]] char const* name() const noexcept override
  {
    return "netlinksocket";
  }

  [[nodiscard]] std::string message(int val) const override
  {
    switch (static_cast<wormhole::sysinfo::Netlink::SocketError>(val))
    {
      case wormhole::sysinfo::Netlink::SocketError::None:
        return "None";
      case wormhole::sysinfo::Netlink::SocketError::Busy:
        return "Busy";
      case wormhole::sysinfo::Netlink::SocketError::Interrupted:
        return "Interrupted";
      case wormhole::sysinfo::Netlink::SocketError::Error:
        return "Error";
      case wormhole::sysinfo::Netlink::SocketError::WrongMessageLength:
        return "WrongMessageLength";
      case wormhole::sysinfo::Netlink::SocketError::InvalidFamily:
        return "InvalidFamily";
      case wormhole::sysinfo::Netlink::SocketError::NotMainTable:
        return "NotMainTable";
      case wormhole::sysinfo::Netlink::SocketError::Noop:
        return "Noop";
      case wormhole::sysinfo::Netlink::SocketError::NoActiveRequest:
        return "NoActiveRequest";
      case wormhole::sysinfo::Netlink::SocketError::MessageIdMismatch:
        return "MessageIdMismatch";
      case wormhole::sysinfo::Netlink::SocketError::UnhandledMessageType:
        return "UnhandledMessageType";
      case wormhole::sysinfo::Netlink::SocketError::MessageTypeMismatch:
        return "MessageTypeMismatch";
    }
    return "unknown";
  }
};
const NetlinkSocketError_cat netlinkSocketErrorCat;
}  // namespace

namespace wormhole::sysinfo::Netlink
{
std::error_code make_error_code(SocketError val)
{
  return {static_cast<int>(val), netlinkSocketErrorCat};
}
}  // namespace wormhole::sysinfo::Netlink
