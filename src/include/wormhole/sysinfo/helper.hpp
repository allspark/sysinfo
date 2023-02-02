/*
 * This file is distributed under the MIT License.
 * See "LICENSE" for details.
 * Copyright 2023, Dennis Börm (allspark@wormhole.eu)
 */

#pragma once

#include <variant>
#include <optional>

namespace wormhole::sysinfo::helper
{
template <class... Ts>
struct overloaded : Ts...
{
  using Ts::operator()...;
};

template <typename... Ts, typename... CBs>
void visit(std::variant<std::monostate, Ts...>& variant, CBs&&... cbs)
{
  std::visit(overloaded{[](std::monostate)
                 {
                 },
                 std::forward<CBs>(cbs)...},
      variant);
}

template <typename... Ts, typename DEF, typename... CBs>
auto visitOptional(std::variant<std::monostate, Ts...> const& variant, DEF&& def, CBs&&... cbs)
{
  return std::visit(overloaded{[&](std::monostate)
                        {
                          return std::forward<DEF>(def)();
                        },
                        std::forward<CBs>(cbs)...},
      variant);
}

template <typename... Ts, typename DEF, typename... CBs>
auto visitOptional(std::variant<std::monostate, Ts...>&& variant, DEF&& def, CBs&&... cbs)
{
  return std::visit(overloaded{[&](std::monostate)
                        {
                          return std::forward<DEF>(def)();
                        },
                        std::forward<CBs>(cbs)...},
      std::move(variant));
}

}
