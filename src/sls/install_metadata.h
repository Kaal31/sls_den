// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sls/source_resolver.h"
#include "sls/authorized_depot_plan.h"

namespace playden::sls {
struct InstallMetadata {
  std::string status = "disabled";
  Resolution resolution;
  std::vector<steam::DepotInfo> depots;
};
// Optional metadata: an unavailable/incomplete source never removes required Steam
// content. Steam order, GIDs, DLC selection and authorization stay authoritative.
inline InstallMetadata ResolveInstallMetadata(std::uint32_t app, const std::vector<Source>& sources,
                                              const SourceFetch& fetch,
                                              const std::vector<steam::DepotInfo>& authorized) {
  InstallMetadata result;
  result.depots = authorized;
  if (ActiveSources(sources).empty()) return result;
  result.resolution = ResolveManifest(app, sources, fetch);
  if (!result.resolution.ok) { result.status = "unavailable"; return result; }
  const auto plan = PlanAuthorizedDepots(result.resolution.metadata, authorized);
  if (!plan.unrecognized.empty()) { result.status = "unowned-depot"; return result; }
  if (plan.selected.size() != authorized.size()) { result.status = "incomplete"; return result; }
  result.status = "matched";
  result.depots = plan.selected;
  return result;
}
} // namespace playden::sls
