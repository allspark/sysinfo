/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#include "wormhole/sysinfo/NetlinkSocket.hpp"

#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <ctime>

#include <numeric>
#include <variant>

#include <fmt/color.h>
#include <fmt/core.h>
#include <boost/asio/ip/address.hpp>

#include "wormhole/sysinfo/errno_error.hpp"
#include "wormhole/sysinfo/helper.hpp"
#include "wormhole/sysinfo/types.hpp"

namespace wormhole::sysinfo::Netlink
{
std::ostream& operator<<(std::ostream& str, Message::Id const& id)
{
  fmt::print(str, "{}:{}", id.seq, id.pid);
  return str;
}

Message::SockAddressNl& Message::GetAddress()
{
  return m_addr;
}

Message::Header* Message::GetHeader()
{
  return &m_header;
}

Message::Id Message::GetId() const
{
  return helper::visitOptional(
      m_request, []()
      {
        return Id{0, 0};
      },
      [](auto const& item) -> Id
      {
        return item.GetId();
      });
}

outcome::std_result<Message::ResponseTypes> Message::GetResponse() &&
{
  return helper::visitOptional(
      std::move(m_request), []() -> outcome::std_result<ResponseTypes>
      {
        return SocketError::MessageTypeMismatch;
      },
      [](auto&& item) -> outcome::std_result<ResponseTypes>
      {
        return std::move(item).GetResponse();
      });
}

outcome::std_result<Socket> Socket::open(std::span<Groups const> groups)
{
  int nl_sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

  if (nl_sock < 0)
  {
    return static_cast<errno_errc>(errno);
  }
  std::uint32_t nlGroups{0};
  nlGroups = std::accumulate(groups.begin(), groups.end(), nlGroups, [](std::uint32_t g, Groups gs)
      {
        return g | std::visit([]<std::uint32_t VAL>(RtNlMG<VAL>)
                       {
                         return VAL;
                       },
                       gs);
      });

  std::uint32_t pid = static_cast<uint32_t>(getpid());
  sockaddr_nl saddr{};
  saddr.nl_family = AF_NETLINK;
  saddr.nl_pid = pid;
  saddr.nl_groups = nlGroups;

  /* Bind current process to the netlink socket */
  if (bind(nl_sock, reinterpret_cast<struct sockaddr*>(&saddr), sizeof(saddr)) < 0)
  {
    int err = errno;
    close(nl_sock);
    return static_cast<errno_errc>(err);
  }

  return Socket{nl_sock, pid};
}

Socket::Socket(Socket&& rhs) noexcept
  : m_pid{rhs.m_pid}
  , m_socket{rhs.m_socket}
{
  rhs.m_socket = -1;
}

Socket& Socket::operator=(Socket&& rhs) noexcept
{
  if (this != std::addressof(rhs))
  {
    std::swap(m_socket, rhs.m_socket);
  }
  return *this;
}

Socket::~Socket()
{
  if (m_socket >= 0)
  {
    close(m_socket);
  }
}

outcome::std_result<Message::ResponseTypes> Socket::receive(ReceiveMode receiveMode)
{
  Message::SockAddressNl nladdr{};
  std::array<Message::IoVec, 1> iov{{}};
  Message::Header msg_header{};
  msg_header.msg_name = &nladdr;
  msg_header.msg_namelen = sizeof(nladdr);
  msg_header.msg_iov = iov.data();
  msg_header.msg_iovlen = iov.size();

  std::optional<Message::ResponseTypes> response;
  do
  {
    iov[0].iov_base = nullptr;
    iov[0].iov_len = 0;
    int flags = MSG_PEEK | MSG_TRUNC;
    if (receiveMode == ReceiveMode::Nonblock)
    {
      flags |= MSG_DONTWAIT;
    }
    ssize_t len = recvmsg(m_socket, &msg_header, flags);
    if (len < 0)
    {
      return static_cast<errno_errc>(errno);
    }

    std::vector<char> buffer;
    buffer.resize(static_cast<size_t>(len));

    iov[0].iov_base = buffer.data();
    iov[0].iov_len = static_cast<size_t>(len);

    len = recvmsg(m_socket, &msg_header, 0);
    if (len < 0)
    {
      return static_cast<errno_errc>(errno);
    }
    buffer.resize(static_cast<size_t>(len));

    auto* nlHeader = reinterpret_cast<struct nlmsghdr*>(buffer.data());
    auto nlHeaderLen = buffer.size();

    if (nlHeader->nlmsg_flags & NLM_F_DUMP_INTR)
    {
      return SocketError::Interrupted;
    }
    if (nlHeader->nlmsg_type == NLMSG_ERROR)
    {
      return SocketError::Error;
    }
    if (nlHeader->nlmsg_type == NLMSG_NOOP)
    {
      return SocketError::Noop;
    }
    if (nlHeader->nlmsg_type == NLMSG_DONE)
    {
      return HandleDone(*nlHeader);
    }
    for (; NLMSG_OK(nlHeader, nlHeaderLen); nlHeader = NLMSG_NEXT(nlHeader, nlHeaderLen))
    {
      if (nlHeader->nlmsg_type == NLMSG_DONE)
      {
        if (response)
        {
          return std::move(*response);
        }
        return HandleDone(*nlHeader);
      }
      if (nlHeader->nlmsg_type == RTM_NEWROUTE || nlHeader->nlmsg_type == RTM_DELROUTE)
      {
        auto& rtMsg = *reinterpret_cast<struct rtmsg*>(NLMSG_DATA(nlHeader));
        BOOST_OUTCOME_TRY(auto r, HandleRoute(*nlHeader, rtMsg));
        if (r && !response)
        {
          response.emplace(std::move(*r));
        }
        continue;
      }
      else if (nlHeader->nlmsg_type == RTM_NEWADDR || nlHeader->nlmsg_type == RTM_DELADDR)
      {
        auto& ifAddrMsg = *reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(nlHeader));
        BOOST_OUTCOME_TRY(auto a, HandleAddress(*nlHeader, ifAddrMsg));
        if (a && !response)
        {
          response.emplace(std::move(*a));
        }
        continue;
      }
      else if (nlHeader->nlmsg_type == RTM_NEWLINK || nlHeader->nlmsg_type == RTM_DELLINK)
      {
        auto& ifLinkMsg = *reinterpret_cast<struct ifinfomsg*>(NLMSG_DATA(nlHeader));
        BOOST_OUTCOME_TRY(auto l, HandleLink(*nlHeader, ifLinkMsg));
        if (l && !response)
        {
          response.emplace(std::move(*l));
        }
        continue;
      }
    }
  } while (m_activeRequest);

  if (response)
  {
    return std::move(*response);
  }
  return SocketError::Error;
}

Socket::Socket(int t_socket, std::uint32_t t_pid)
  : m_pid{t_pid}
  , m_socket{t_socket}
{
}

outcome::std_result<Message::Id> Socket::send(std::unique_ptr<Message> msgPtr)
{
  m_activeRequest = std::move(msgPtr);
  auto currentId = m_activeRequest->GetId();
  ssize_t sent = sendmsg(m_socket, m_activeRequest->GetHeader(), 0);

  if (sent < 0)
  {
    return static_cast<errno_errc>(errno);
  }
  return currentId;
}

outcome::std_result<Route> Socket::parse_route(struct nlmsghdr& header, struct rtmsg& rtMsg)
{
  auto rtLen = RTM_PAYLOAD(&header);
  auto tb = parse_rtattr<RTA_MAX>(RTM_RTA(&rtMsg), rtLen);

  auto table = rtm_get_table<RTA_MAX>(rtMsg, tb);

  if (rtMsg.rtm_family != AF_INET && rtMsg.rtm_family != AF_INET6)
  {
    return SocketError::InvalidFamily;
  }

  Route entry;
  entry.action = [&header]()
  {
    if (header.nlmsg_type == RTM_NEWROUTE)
    {
      return Action::New;
    }
    else if (header.nlmsg_type == RTM_DELROUTE)
    {
      return Action::Del;
    }
    return Action::Unknown;
  }();
  entry.table = [&table]()
  {
    switch (table)
    {
      case RT_TABLE_MAIN:
        return Route::Table::Main;
      case RT_TABLE_LOCAL:
        return Route::Table::Local;
    }
    return Route::Table::Default;
  }();

  if (tb[RTA_DST])
  {
    if (rtMsg.rtm_family == AF_INET)
    {
      entry.destination.emplace<boost::asio::ip::network_v4>(Address::convertAddress(rtMsg.rtm_family, RTA_DATA(tb[RTA_DST])).to_v4(), rtMsg.rtm_dst_len);
    }
    else if (rtMsg.rtm_family == AF_INET6)
    {
      entry.destination.emplace<boost::asio::ip::network_v6>(Address::convertAddress(rtMsg.rtm_family, RTA_DATA(tb[RTA_DST])).to_v6(), rtMsg.rtm_dst_len);
    }
  }

  if (tb[RTA_GATEWAY])
  {
    entry.gateway = Address::convertAddress(rtMsg.rtm_family, RTA_DATA(tb[RTA_GATEWAY]));
  }

  if (tb[RTA_OIF])
  {
    char if_nam_buf[IF_NAMESIZE];
    unsigned int ifidx = *reinterpret_cast<uint32_t*>(RTA_DATA(tb[RTA_OIF]));

    entry.interfaceName = if_indextoname(ifidx, if_nam_buf);
  }

  if (tb[RTA_SRC])
  {
    entry.source = Address::convertAddress(rtMsg.rtm_family, RTA_DATA(tb[RTA_SRC]));
  }

  return entry;
}

outcome::std_result<Address> Socket::parse_address(struct nlmsghdr& header, struct ifaddrmsg& msg)
{
  auto rtLen = IFA_PAYLOAD(&header);
  auto tb = parse_rtattr<IFA_MAX>(IFA_RTA(&msg), rtLen);

  Address entry;
  entry.action = [&header]()
  {
    if (header.nlmsg_type == RTM_NEWADDR)
    {
      return Action::New;
    }
    else if (header.nlmsg_type == RTM_DELADDR)
    {
      return Action::Del;
    }
    return Action::Unknown;
  }();
  entry.netmask = msg.ifa_prefixlen;
  entry.scope = [](std::uint32_t scope)
  {
    switch (scope)
    {
      case RT_SCOPE_UNIVERSE:
        return Scope::Universe;
        /* User defined values  */
      case RT_SCOPE_SITE:
        return Scope::Site;
      case RT_SCOPE_LINK:
        return Scope::Link;
      case RT_SCOPE_HOST:
        return Scope::Host;
      case RT_SCOPE_NOWHERE:
        return Scope::Nowhere;
    }
    return Scope::Unknown;
  }(msg.ifa_scope);

  if (tb[IFA_ADDRESS])
  {
    entry.address = Address::convertAddress(msg.ifa_family, RTA_DATA(tb[IFA_ADDRESS]));
  }

  if (tb[IFA_LOCAL])
  {
    entry.local = Address::convertAddress(msg.ifa_family, RTA_DATA(tb[IFA_LOCAL]));
  }

  if (tb[IFA_BROADCAST])
  {
    entry.broadcast = Address::convertAddress(msg.ifa_family, RTA_DATA(tb[IFA_BROADCAST]));
  }

  return entry;
}

outcome::std_result<Interface> Socket::parse_link(struct nlmsghdr& header, struct ifinfomsg& msg)
{
  auto rtLen = IFLA_PAYLOAD(&header);
  auto tb = parse_rtattr<IFLA_MAX>(IFLA_RTA(&msg), rtLen);

  Interface entry;
  entry.action = [&header]()
  {
    if (header.nlmsg_type == RTM_NEWLINK)
    {
      return Action::New;
    }
    else if (header.nlmsg_type == RTM_DELLINK)
    {
      return Action::Del;
    }
    return Action::Unknown;
  }();

  if (tb[IFLA_IFNAME])
  {
    entry.name = reinterpret_cast<char const*>(RTA_DATA(tb[IFLA_IFNAME]));
  }

  return entry;
}

template <std::size_t MAX>
Socket::AttrTable<MAX> Socket::parse_rtattr(struct rtattr* rta, std::size_t len)
{
  AttrTable<MAX> tb{};
  while (RTA_OK(rta, len))
  {
    if (rta->rta_type < tb.size())
    {
      tb[rta->rta_type] = rta;
    }

    rta = RTA_NEXT(rta, len);
  }
  return tb;
}
template <std::size_t MAX>
uint32_t Socket::rtm_get_table(struct rtmsg& r, AttrTable<MAX> const& tb)
{
  uint32_t table = r.rtm_table;

  if (tb[RTA_TABLE])
  {
    table = *reinterpret_cast<uint32_t*>(RTA_DATA(tb[RTA_TABLE]));
  }

  return table;
}

outcome::std_result<Message::ResponseTypes> Socket::HandleDone(struct nlmsghdr& header)
{
  auto req = PopRequest(Message::Id{header.nlmsg_seq, header.nlmsg_pid});
  if (req)
  {
    return std::move(*req).GetResponse();
  }
  else
  {
    return SocketError::MessageIdMismatch;
  }
}

outcome::std_result<std::optional<Route>> Socket::HandleRoute(struct nlmsghdr& header, struct rtmsg& rtMsg)
{
  BOOST_OUTCOME_TRY(auto route, parse_route(header, rtMsg));
  if (header.nlmsg_pid != m_pid)
  {
    return route;
  }

  BOOST_OUTCOME_TRY(addResponse<Message::RouteRequest>({header.nlmsg_seq, header.nlmsg_pid}, std::move(route)));
  return std::nullopt;
}

outcome::std_result<std::optional<Address>> Socket::HandleAddress(struct nlmsghdr& header, struct ifaddrmsg& ifMsg)
{
  BOOST_OUTCOME_TRY(auto address, parse_address(header, ifMsg));
  if (header.nlmsg_pid != m_pid)
  {
    return address;
  }

  BOOST_OUTCOME_TRY(addResponse<Message::AddressRequest>({header.nlmsg_seq, header.nlmsg_pid}, std::move(address)));
  return std::nullopt;
}

outcome::std_result<std::optional<Interface>> Socket::HandleLink(struct nlmsghdr& header, struct ifinfomsg& ifMsg)
{
  BOOST_OUTCOME_TRY(auto link, parse_link(header, ifMsg));
  if (header.nlmsg_pid != m_pid)
  {
    return link;
  }

  BOOST_OUTCOME_TRY(addResponse<Message::LinkRequest>({header.nlmsg_seq, header.nlmsg_pid}, std::move(link)));
  return std::nullopt;
}

template <typename Request, typename T>
outcome::std_result<void> Socket::addResponse(Message::Id id, T&& t)
{
  if (!m_activeRequest)
  {
    return SocketError::NoActiveRequest;
  }
  if (m_activeRequest->GetId() != id)
  {
    return SocketError::MessageIdMismatch;
  }
  return m_activeRequest->AddResponse<Request>(std::forward<T>(t));
}

std::unique_ptr<Message> Socket::PopRequest(Message::Id const& id)
{
  if (!m_activeRequest || m_activeRequest->GetId() != id)
  {
    return nullptr;
  }
  return std::move(m_activeRequest);
}
}  // namespace wormhole::sysinfo::Netlink
