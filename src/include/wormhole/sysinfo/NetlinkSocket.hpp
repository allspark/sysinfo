/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#pragma once

#include <deque>
#include <span>
#include <thread>
#include <variant>
#include <vector>

#include <linux/rtnetlink.h>
#include <boost/outcome.hpp>
#include <condition_variable>

#include "NetlinkSocketError.hpp"
#include "errno_error.hpp"
#include "helper.hpp"
#include "types.hpp"

namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;

namespace wormhole::sysinfo::Netlink
{
class Message
{
public:
  using SockAddressNl = struct sockaddr_nl;
  using IoVec = struct iovec;
  using Header = struct msghdr;
  using NetlinkMessageHeader = struct nlmsghdr;

  struct Id
  {
    std::uint32_t seq;
    std::uint32_t pid;

    auto operator<=>(Id const&) const noexcept = default;

    friend std::ostream& operator<<(std::ostream&, Id const&);
  };

  template <typename T>
  struct Response
  {
    Id id;
    T data;
  };

  template <typename DATA, std::uint16_t RT_TYPE, typename RESPONSE>
  struct Request
  {
    using Data_t = DATA;
    using ResponseData_t = RESPONSE;
    using Response_t = Response<ResponseData_t>;
    Request(int family, std::uint16_t flags, std::uint32_t seq, std::uint32_t pid /*, Handler_t handler*/)
      : nlh{.nlmsg_len = NLMSG_LENGTH(sizeof(DATA)), .nlmsg_type = RT_TYPE, .nlmsg_flags = flags, .nlmsg_seq = seq, .nlmsg_pid = pid}
    {
      setFamily(data, family);
    }

    Id GetId() const
    {
      return {nlh.nlmsg_seq, nlh.nlmsg_pid};
    }

    void AddItem(typename ResponseData_t::value_type val)
    {
      response.push_back(std::move(val));
    }

    Response_t GetResponse() &&
    {
      return {GetId(), std::move(response)};
    }

    std::array<IoVec, 2> GetIov()
    {
      std::array<IoVec, 2> iov;
      iov[0].iov_base = &nlh;
      iov[0].iov_len = sizeof(nlh);
      iov[1].iov_base = &data;
      iov[1].iov_len = sizeof(data);
      return iov;
    }

  private:
    static void setFamily(struct rtmsg& d, int family)
    {
      d.rtm_family = family;
    }
    static void setFamily(struct ifaddrmsg& d, int family)
    {
      d.ifa_family = family;
    }
    static void setFamily(struct ifinfomsg& d, int family)
    {
      d.ifi_family = family;
    }

    NetlinkMessageHeader nlh;
    Data_t data{};
    ResponseData_t response;
  };
  using AddressRequest = Request<struct ifaddrmsg, RTM_GETADDR, std::vector<Address>>;
  using LinkRequest = Request<struct ifinfomsg, RTM_GETLINK, std::vector<Interface>>;
  using RouteRequest = Request<struct rtmsg, RTM_GETROUTE, std::vector<Route>>;

  template <typename TYPE>
  Message(std::in_place_type_t<TYPE>, int family, std::uint16_t flags, std::uint32_t seq, std::uint32_t pid)
    : m_iov{{}}
    , m_header{.msg_name = &m_addr, .msg_namelen = sizeof(m_addr), .msg_iov = m_iov.data(), .msg_iovlen = m_iov.size()}
    , m_request{std::in_place_type_t<TYPE>{}, family, flags, seq, pid}
  {
    auto& msg = std::get<TYPE>(m_request);
    m_iov = msg.GetIov();
    m_addr.nl_family = AF_NETLINK;
  }

  Message(Message const&) = delete;
  Message(Message&&) = delete;
  Message& operator=(Message const&) = delete;
  Message& operator=(Message&&) = delete;
  ~Message() = default;

  SockAddressNl& GetAddress();
  Header* GetHeader();
  [[nodiscard]] Id GetId() const;

  template <typename REQ, typename RES>
  outcome::std_result<void> AddResponse(RES&& response)
  {
    if (auto* req = std::get_if<REQ>(&m_request); req)
    {
      req->AddItem(std::forward<RES>(response));
      return outcome::success();
    }
    return SocketError::MessageTypeMismatch;
  }
  using ResponseTypes = std::variant<AddressRequest::Response_t, LinkRequest::Response_t, RouteRequest::Response_t, Address, Interface, Route>;
  outcome::std_result<ResponseTypes> GetResponse() &&;

private:
  SockAddressNl m_addr{};
  std::array<IoVec, 2> m_iov{{}};
  Header m_header{};

  std::variant<std::monostate, LinkRequest, RouteRequest, AddressRequest> m_request;
};

class Socket final
{
public:
  template <std::uint32_t VAL>
  using RtNlMG = std::integral_constant<std::uint32_t, VAL>;
  // clang-format off
  struct GroupLink : RtNlMG<RTMGRP_LINK> {};
  struct GroupIpV4Route : RtNlMG<RTMGRP_IPV4_ROUTE> {};
  struct GroupIpV6Route : RtNlMG<RTMGRP_IPV6_ROUTE> {};
  struct GroupIpV4Address : RtNlMG<RTMGRP_IPV4_IFADDR> {};
  struct GroupIpV6Address : RtNlMG<RTMGRP_IPV6_IFADDR> {};
  // clang-format on

  using Groups = std::variant<GroupLink, GroupIpV4Route, GroupIpV6Route, GroupIpV4Address, GroupIpV6Address>;
  using GroupList = std::initializer_list<Groups>;

  static outcome::std_result<Socket> open(std::span<Groups const>);

  Socket(Socket const&) = delete;
  Socket(Socket&&) noexcept;
  Socket& operator=(Socket const&) = delete;
  Socket& operator=(Socket&&) noexcept;
  ~Socket();

  template <typename Request>
  outcome::std_result<Message::Id> send_request(int family)
  {
    if (m_activeRequest)
    {
      return make_error_code(SocketError::Busy);
    }

    auto msgPtr = std::make_unique<Message>(std::in_place_type_t<Request>{}, family, NLM_F_DUMP | NLM_F_REQUEST, ++m_seqNum, static_cast<uint32_t>(m_pid));

    return send(std::move(msgPtr));
  }

  enum struct ReceiveMode
  {
    Wait,
    Nonblock
  };
  outcome::std_result<Message::ResponseTypes> receive(ReceiveMode);
  template <typename Request>
  outcome::std_result<typename Request::Response_t> receive(ReceiveMode mode)
  {
    BOOST_OUTCOME_TRY(auto r, receive(mode));
    if (auto* response = std::get_if<typename Request::Response_t>(&r); response)
    {
      return *response;
    }
    return SocketError::MessageTypeMismatch;
  }

private:
  explicit Socket(int t_socket, std::uint32_t t_pid);

  outcome::std_result<Message::Id> send(std::unique_ptr<Message>);

  outcome::std_result<Route> parse_route(struct nlmsghdr&, struct rtmsg& r);
  outcome::std_result<Address> parse_address(struct nlmsghdr&, struct ifaddrmsg&);
  outcome::std_result<Interface> parse_link(struct nlmsghdr&, struct ifinfomsg&);

  template <std::size_t MAX>
  using AttrTable = std::array<struct rtattr*, MAX + 1>;
  template <std::size_t MAX>
  AttrTable<MAX> parse_rtattr(struct rtattr* rta, std::size_t len);
  template <std::size_t MAX>
  uint32_t rtm_get_table(struct rtmsg& r, AttrTable<MAX> const&);

  outcome::std_result<Message::ResponseTypes> HandleDone(struct nlmsghdr&);
  outcome::std_result<std::optional<Route>> HandleRoute(struct nlmsghdr&, struct rtmsg&);
  outcome::std_result<std::optional<Address>> HandleAddress(struct nlmsghdr&, struct ifaddrmsg&);
  outcome::std_result<std::optional<Interface>> HandleLink(struct nlmsghdr&, struct ifinfomsg&);

  template <typename Request, typename T>
  outcome::std_result<void> addResponse(Message::Id id, T&& t);
  std::unique_ptr<Message> PopRequest(Message::Id const&);

  const std::uint32_t m_pid;
  int m_socket;
  std::uint32_t m_seqNum{0};
  std::unique_ptr<Message> m_activeRequest;
};
}  // namespace wormhole::sysinfo::Netlink

template <>
struct fmt::formatter<wormhole::sysinfo::Netlink::Message::Id> : ostream_formatter
{
};
