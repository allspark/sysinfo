/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#pragma once

#include <fstream>
#include <variant>

#include <fmt/ostream.h>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>

namespace wormhole::sysinfo
{
enum struct Action
{
  Unknown,
  New,
  Del
};
std::ostream& operator<<(std::ostream&, Action const&);

enum struct Scope
{
  Unknown,
  Universe,
  Site,
  Link,
  Host,
  Nowhere
};
std::ostream& operator<<(std::ostream&, Scope const&);

struct Address
{
  Address() = default;
  Address(boost::asio::ip::address t_address, boost::asio::ip::address_v4 const& t_netmask);
  Address(boost::asio::ip::address t_address, boost::asio::ip::address_v6 const& t_netmask);
  Address(boost::asio::ip::address t_address, boost::asio::ip::address const& t_netmask);
  Address(boost::asio::ip::address t_address, std::size_t t_netmask);

  static boost::asio::ip::address convertAddress(int family, void* data);

  Action action{Action::New};
  boost::asio::ip::address address;
  std::size_t netmask;
  boost::asio::ip::address broadcast;
  boost::asio::ip::address local;
  Scope scope{Scope::Nowhere};

  friend std::ostream& operator<<(std::ostream& str, Address const& addr);
};

struct Interface
{
  Interface() = default;
  explicit Interface(std::string_view t_name);

  Action action{Action::New};
  std::string name;

  friend std::ostream& operator<<(std::ostream&, Interface const&);
};

struct Route
{
  enum struct Table
  {
    Default,
    Main,
    Local,
  };
  struct Default_t
  {
    friend std::ostream& operator<<(std::ostream&, Default_t const&);
  };
  struct Destination
  {
    std::variant<Default_t, boost::asio::ip::network_v4, boost::asio::ip::network_v6> value;

    template <typename T, typename... ARGs>
    decltype(auto) emplace(ARGs&&... args)
    {
      return value.emplace<T>(std::forward<ARGs>(args)...);
    }
    bool IsDefaultRoute() const noexcept
    {
      return std::holds_alternative<Default_t>(value);
    }

    friend std::ostream& operator<<(std::ostream&, Destination const&);
  };

  Action action{Action::New};
  Table table{Table::Default};
  Destination destination;
  boost::asio::ip::address gateway;
  std::string interfaceName;
  boost::asio::ip::address source;

  friend std::ostream& operator<<(std::ostream&, Route const&);
};

std::ostream& operator<<(std::ostream&, Route::Table const&);
}  // namespace wormhole::sysinfo

template <>
struct fmt::formatter<wormhole::sysinfo::Action> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Scope> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Address> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Interface> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Route::Table> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Route::Default_t> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Route::Destination> : ostream_formatter
{
};
template <>
struct fmt::formatter<wormhole::sysinfo::Route> : ostream_formatter
{
};
template <>
struct fmt::formatter<boost::asio::ip::address> : ostream_formatter
{
};
template <>
struct fmt::formatter<boost::asio::ip::network_v4> : ostream_formatter
{
};
template <>
struct fmt::formatter<boost::asio::ip::network_v6> : ostream_formatter
{
};
