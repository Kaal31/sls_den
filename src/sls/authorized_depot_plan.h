#pragma once
#include "sls/manifest_metadata.h"
#include "steam/backend/app_info.h"
#include <algorithm>
#include <set>
#include <string>
#include <vector>
namespace playden::sls {
// Preparation only: never creates entitlements or downloads content.
// The Steam installer must independently authenticate and authorize every depot.
struct DepotPlan {
  std::vector<steam::DepotInfo> selected;
  std::vector<std::uint32_t> unrecognized;
};
inline DepotPlan PlanAuthorizedDepots(const ManifestMetadata& metadata,
                                     const std::vector<steam::DepotInfo>& authorized) {
  DepotPlan result;
  std::set<std::uint32_t> seen;
  for (const auto& depot : authorized) {
    auto match = std::find_if(metadata.depots.begin(), metadata.depots.end(),
      [&](const DepotPin& pin) { return pin.depot_id == depot.id && pin.enabled; });
    if (match == metadata.depots.end() || !seen.insert(depot.id).second) continue;
    auto record = depot;
    // An unverified third-party manifest GID is *not* used for downloading.
    // Only Steam's already authorized GID may be retained.
    result.selected.push_back(record);
  }
  for (const auto& pin : metadata.depots) {
    if (!pin.enabled) continue;
    if (std::none_of(authorized.begin(), authorized.end(),
                     [&](const steam::DepotInfo& info) { return info.id == pin.depot_id; }))
      result.unrecognized.push_back(pin.depot_id);
  }
  return result;
}
}
