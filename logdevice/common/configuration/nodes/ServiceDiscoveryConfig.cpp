/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include "logdevice/common/configuration/nodes/ServiceDiscoveryConfig.h"

#include <folly/Format.h>
#include <folly/Range.h>

#include "logdevice/common/configuration/nodes/utils.h"
#include "logdevice/common/debug.h"
#include "logdevice/common/types_internal.h"

namespace facebook { namespace logdevice { namespace configuration {
namespace nodes {

namespace {
template <typename F>
bool isFieldValid(const F& field, folly::StringPiece name) {
  if (!field.valid()) {
    RATELIMIT_ERROR(
        std::chrono::seconds(10), 5, "invalid %s.", name.str().c_str());
    return false;
  }
  return true;
}

template <typename F>
bool isOptionalFieldValid(const F& field, folly::StringPiece name) {
  return !field.hasValue() || isFieldValid(field.value(), name);
}

} // namespace

bool NodeServiceDiscovery::isValid() const {
  if (!isFieldValid(address, "address") &&
      !isFieldValid(gossip_address, "gossip_address") &&
      !isOptionalFieldValid(ssl_address, "ssl_address")) {
    return false;
  }

  if (roles.count() == 0) {
    RATELIMIT_ERROR(std::chrono::seconds(10),
                    5,
                    "no role is set. expect at least one role.");
    return false;
  }

  std::string name_invalid_reason;
  if (!name.empty() && !nodes::isValidServerName(name, &name_invalid_reason)) {
    RATELIMIT_ERROR(std::chrono::seconds(10),
                    5,
                    "Invalid server name: %s: %s",
                    name.c_str(),
                    name_invalid_reason.c_str());
    return false;
  }

  // TODO(T44427489): Check that the name field is not empty.
  return true;
}

std::string NodeServiceDiscovery::toString() const {
  return folly::sformat("[{} => A:{},G:{},S:{},L:{},R:{}]",
                        name,
                        address.toString(),
                        gossip_address.toString(),
                        ssl_address.hasValue() ? ssl_address->toString() : "",
                        location.hasValue() ? location->toString() : "",
                        logdevice::toString(roles));
}

const Sockaddr&
NodeServiceDiscovery::getSockaddr(SocketType type,
                                  ConnectionType conntype) const {
  switch (type) {
    case SocketType::GOSSIP:
      return gossip_address;

    case SocketType::DATA:
      if (conntype == ConnectionType::SSL) {
        if (!ssl_address.hasValue()) {
          return Sockaddr::INVALID;
        }
        return ssl_address.value();
      } else {
        return address;
      }

    default:
      RATELIMIT_CRITICAL(
          std::chrono::seconds(1), 2, "Unexpected Socket Type:%d!", (int)type);
      ld_check(false);
  }

  return Sockaddr::INVALID;
}

namespace {
bool validateAddressUniqueness(ServiceDiscoveryConfig::MapType node_states) {
  std::unordered_map<Sockaddr, node_index_t, Sockaddr::Hash> seen_addresses;
  for (const auto& kv : node_states) {
    auto res = seen_addresses.emplace(kv.second.address, kv.first);
    if (!res.second) {
      RATELIMIT_ERROR(std::chrono::seconds(10),
                      5,
                      "Multiple nodes with the same address (idx: %hd, %hd).",
                      res.first->second,
                      kv.first);
      return false;
    }
  }
  return true;
}

bool validateNameUniqueness(ServiceDiscoveryConfig::MapType node_states) {
  std::unordered_map<std::string, node_index_t> seen_names;
  for (const auto& kv : node_states) {
    if (kv.second.name.empty()) {
      continue;
    }
    auto res =
        seen_names.emplace(normalizeServerName(kv.second.name), kv.first);
    if (!res.second) {
      RATELIMIT_ERROR(std::chrono::seconds(10),
                      5,
                      "Multiple nodes with the same name '%s' (idx: %hd, %hd).",
                      kv.second.name.c_str(),
                      res.first->second,
                      kv.first);
      return false;
    }
  }
  return true;
}

} // namespace

template <>
bool ServiceDiscoveryConfig::attributeSpecificValidate() const {
  return validateAddressUniqueness(node_states_) &&
      validateNameUniqueness(node_states_);
}

}}}} // namespace facebook::logdevice::configuration::nodes
