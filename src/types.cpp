/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#include "wormhole/sysinfo/types.hpp"

#include <numeric>

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace
{
template <typename T, std::size_t N>
std::size_t countNetmaskBits(std::array<T, N> const& bytes)
{
  return std::accumulate(bytes.begin(), bytes.end(), 0u, [](std::size_t n, unsigned char b)
      {
        return n + std::countl_one(b);
      });
}
std::size_t countNetmaskBits(boost::asio::ip::address const& netmask)
{
  if (netmask.is_v4())
  {
    return countNetmaskBits(netmask.to_v4().to_bytes());
  }
  if (netmask.is_v6())
  {
    return countNetmaskBits(netmask.to_v6().to_bytes());
  }
  return 0;
}
}  // namespace

namespace wormhole::sysinfo
{
std::ostream& operator<<(std::ostream& str, Action const& action)
{
  switch (action)
  {
    case Action::Unknown:
      str << "unknown";
      break;
    case Action::New:
      str << "new";
      break;
    case Action::Del:
      str << "del";
      break;
  }
  return str;
}

std::ostream& operator<<(std::ostream& str, Scope const& scope)
{
  switch (scope)
  {
    case Scope::Universe:
      str << "Universe";
      break;
    case Scope::Site:
      str << "Site";
      break;
    case Scope::Link:
      str << "Link";
      break;
    case Scope::Host:
      str << "Host";
      break;
    case Scope::Nowhere:
      str << "Nowhere";
      break;
    case Scope::Unknown:
      str << "Unknown";
      break;
  }
  return str;
}

Address::Address(boost::asio::ip::address t_address, boost::asio::ip::address_v4 const& t_netmask)
  : Address{std::move(t_address), countNetmaskBits(t_netmask.to_bytes())}
{
}

Address::Address(boost::asio::ip::address t_address, boost::asio::ip::address_v6 const& t_netmask)
  : Address{std::move(t_address), countNetmaskBits(t_netmask.to_bytes())}
{
}

Address::Address(boost::asio::ip::address t_address, boost::asio::ip::address const& t_netmask)
  : Address{std::move(t_address), countNetmaskBits(t_netmask)}
{
}

Address::Address(boost::asio::ip::address t_address, std::size_t t_netmask)
  : address{std::move(t_address)}
  , netmask{t_netmask}
{
}

std::ostream& operator<<(std::ostream& str, Address const& addr)
{
  fmt::print(str, "{} address {}/{} scope {}", addr.action, addr.address, addr.netmask, addr.scope);
  if (!addr.broadcast.is_unspecified())
  {
    fmt::print(str, " broadcast {}", addr.broadcast);
  }
  if (!addr.local.is_unspecified())
  {
    fmt::print(str, " local {}", addr.local);
  }
  return str;
}

std::ostream& operator<<(std::ostream& str, Interface::Index const index)
{
  fmt::print(str, "{}", index.value);
  return str;
}

boost::asio::ip::address Address::convertAddress(int family, void* data)
{
  if (family == AF_INET)
  {
    boost::asio::ip::address_v4::bytes_type networkOrder;
    memcpy(networkOrder.data(), data, networkOrder.size());
    return boost::asio::ip::make_address_v4(networkOrder);
  }
  else if (family == AF_INET6)
  {
    boost::asio::ip::address_v6::bytes_type networkOrder;
    memcpy(networkOrder.data(), data, networkOrder.size());
    return boost::asio::ip::make_address_v6(networkOrder);
  }
  return {};
}

Interface::Interface(std::string_view t_name)
  : name{t_name}
{
}

std::ostream& operator<<(std::ostream& str, Interface::Type const type)
{
  using namespace std::string_view_literals;
  std::string_view type_name = [type]()
  {
    switch (type)
    {
      case Interface::Type::Ethernet:
        return "ethernet"sv;
      case Interface::Type::IpIpTunnel:
        return "ipiptunnel"sv;
      case Interface::Type::IpIp6Tunnel:
        return "ipip6tunnel"sv;
      case Interface::Type::Loopback:
        return "loopback"sv;
      case Interface::Type::Unknown:
        return "unknown"sv;
      case Interface::Type::None:
        return "none"sv;
    }
    return "<unknown>"sv;
  }();
  fmt::print(str, "{}", type_name);
  return str;
}

std::ostream& operator<<(std::ostream& str, Interface const& interface)
{
  fmt::print(str, "{} index: {} type: {} link: {}", interface.action, interface.index, interface.type, interface.name);
  return str;
}

std::ostream& operator<<(std::ostream& str, Route::Default_t const&)
{
  fmt::print(str, "default");
  return str;
}

std::ostream& operator<<(std::ostream& str, Route::Destination const& destination)
{
  std::visit([&str](auto const& item)
      {
        fmt::print(str, "{}", item);
      },
      destination.value);
  return str;
}

std::ostream& operator<<(std::ostream& str, Route const& route)
{
  fmt::print(str, "{} route: {}", route.action, route.destination);
  if (!route.gateway.is_unspecified())
  {
    fmt::print(str, " via {}", route.gateway);
  }

  if (!route.interfaceName.empty())
  {
    fmt::print(str, " dev {}", route.interfaceName);
  }
  if (!route.source.is_unspecified())
  {
    fmt::print(str, " src {}", route.source);
  }
  fmt::print(str, " table {}", route.table);
  return str;
}

std::ostream& operator<<(std::ostream& str, Route::Table const& table)
{
  switch (table)
  {
    case Route::Table::Default:
      fmt::print(str, "default");
      break;
    case Route::Table::Main:
      fmt::print(str, "main");
      break;
    case Route::Table::Local:
      fmt::print(str, "local");
      break;
  }
  return str;
}
}  // namespace wormhole::sysinfo
