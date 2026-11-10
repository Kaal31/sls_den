#pragma once
// Shared Playden/SLS metadata handoff: accepts only depots Steam already authorized.
// A manifest source is advisory; the returned target always retains Steam's
// authorized depot order and GIDs, regardless of provider failures or claims.
#include "sls/install_metadata.h"
#include <functional>
#include <string>
#include <vector>

namespace playden::sls {
struct HandoffRecord {
  std::uint32_t app_id=0;
  std::string status;
  std::string provider_id;
  std::vector<steam::DepotInfo> depots;
};
using HandoffObserver=std::function<void(const HandoffRecord&)>;
inline InstallMetadata ResolveInstallHandoff(
    std::uint32_t app_id,const std::vector<Source>& sources,const SourceFetch& fetch,
    const std::vector<steam::DepotInfo>& steam_authorized,
    const HandoffObserver& observe={}) {
  auto metadata=ResolveInstallMetadata(app_id,sources,fetch,steam_authorized);
  if(observe)observe({app_id,metadata.status,metadata.resolution.source_id,metadata.depots});
  return metadata;
}
} // namespace playden::sls
