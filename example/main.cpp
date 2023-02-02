/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#include <wormhole/sysinfo/NetlinkSocket.hpp>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/os.h>

using namespace wormhole::sysinfo;

outcome::std_result<void> example()
{
  BOOST_OUTCOME_TRY(auto socket, Netlink::Socket::open({}));
  BOOST_OUTCOME_TRY(auto id, socket.send_request<Netlink::Message::LinkRequest>(AF_UNSPEC));
  fmt::print("Request id: {}\n", id);

  BOOST_OUTCOME_TRY(auto response, socket.receive<Netlink::Message::LinkRequest>(Netlink::Socket::ReceiveMode::Wait));

  fmt::print("Response id: {}\n", response.id);
  fmt::print("{}\n", fmt::join(response.data, "\n"));

  return outcome::success();
}

outcome::std_result<void> network_routes()
{
  constexpr static Netlink::Socket::GroupList groups{Netlink::Socket::GroupIpV4Route{}, Netlink::Socket::GroupIpV6Route{}, Netlink::Socket::GroupIpV4Address{}, Netlink::Socket::GroupIpV6Address{}};

  BOOST_OUTCOME_TRY(auto socket, Netlink::Socket::open(groups));

  {
    BOOST_OUTCOME_TRY(auto id, socket.send_request<Netlink::Message::LinkRequest>(AF_UNSPEC));
    fmt::print("LinkRequest id: {}\n", id);
    BOOST_OUTCOME_TRY(auto response, socket.receive<Netlink::Message::LinkRequest>(Netlink::Socket::ReceiveMode::Wait));

    fmt::print("Response id: {}\n", response.id);
    fmt::print("{}\n", fmt::join(response.data, "\n"));
  }

  {
    BOOST_OUTCOME_TRY(auto id, socket.send_request<Netlink::Message::RouteRequest>(AF_INET));
    fmt::print("RouteRequest IpV4 id: {}\n", id);

    BOOST_OUTCOME_TRY(auto response, socket.receive<Netlink::Message::RouteRequest>(Netlink::Socket::ReceiveMode::Wait));

    fmt::print("Response id: {}\n", response.id);
    fmt::print("{}\n", fmt::join(response.data, "\n"));
  }
  {
    BOOST_OUTCOME_TRY(auto id, socket.send_request<Netlink::Message::RouteRequest>(AF_INET6));
    fmt::print("RouteRequest IpV6 id: {}\n", id);

    BOOST_OUTCOME_TRY(auto response, socket.receive<Netlink::Message::RouteRequest>(Netlink::Socket::ReceiveMode::Wait));

    fmt::print("Response id: {}\n", response.id);
    fmt::print("{}\n", fmt::join(response.data, "\n"));
  }
  {
    BOOST_OUTCOME_TRY(auto id, socket.send_request<Netlink::Message::AddressRequest>(AF_INET));
    fmt::print("AddressRequest IpV6 id: {}\n", id);

    BOOST_OUTCOME_TRY(auto response, socket.receive<Netlink::Message::AddressRequest>(Netlink::Socket::ReceiveMode::Wait));

    fmt::print("Response id: {}\n", response.id);
    fmt::print("{}\n", fmt::join(response.data, "\n"));
  }
  {
    BOOST_OUTCOME_TRY(auto id, socket.send_request<Netlink::Message::AddressRequest>(AF_INET6));
    fmt::print("AddressRequest IpV6 id: {}\n", id);

    BOOST_OUTCOME_TRY(auto response, socket.receive<Netlink::Message::AddressRequest>(Netlink::Socket::ReceiveMode::Wait));

    fmt::print("Response id: {}\n", response.id);
    fmt::print("{}\n", fmt::join(response.data, "\n"));
  }

  std::size_t count{4};
  do
  {
    fmt::print("listening to multicast groups ({})\n", count);
    BOOST_OUTCOME_TRY(auto msg, socket.receive(Netlink::Socket::ReceiveMode::Wait));

    std::visit(helper::overloaded{[](Address const& item)
                   {
                     fmt::print("{}\n", item);
                   },
                   [](Route const& item)
                   {
                     fmt::print("{}\n", item);
                   },
                   [](Interface const& item)
                   {
                     fmt::print("{}\n", item);
                   },
                   [](auto const& response)
                   {
                     fmt::print("Response id: {}\n", response.id);
                     fmt::print("{}\n", fmt::join(response.data, "\n"));
                   }},
        msg);

    --count;
  } while (count > 0);
  return outcome::success();
}
int main()
{
  auto result = network_routes();
  //  auto result = example();
  if (result.has_failure())
  {
    fmt::print(fmt::fg(fmt::color::red), "example failed: {}\n", result.as_failure().error().message());
    return -1;
  }
}
