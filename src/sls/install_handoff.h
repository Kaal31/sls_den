#pragma once
// Shared Playden/SLS metadata handoff: accepts only depots Steam already authorized.
// A manifest source is advisory; the returned target always retains Steam's
// authorized depot order and GIDs, regardless of provider failures or claims.
#include "sls/install_metadata.h"
#include "sls/provider_discovery.h"
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
// Credential-aware variant for hosts with a protected key lookup and a
// provider-specific authenticated transport. This does not grant depots or
// replace the Steam-owned installation manifest IDs.
inline InstallMetadata ResolveCredentialHandoff(
    std::uint32_t app_id,const ProviderSelection& selection,
    const CredentialLookup& credentials,const CredentialSourceFetch& fetch,
    const std::vector<steam::DepotInfo>& steam_authorized,
    const HandoffObserver& observe={}) {
  InstallMetadata result;
  result.depots=steam_authorized;
  result.resolution=DiscoverManifestWithCredentials(app_id,selection,credentials,fetch);
  if (!result.resolution.ok)result.status="unavailable";
  else {
    const auto plan=PlanAuthorizedDepots(result.resolution.metadata,steam_authorized);
    if(!plan.unrecognized.empty())result.status="unowned-depot";
    else if(plan.selected.size()!=steam_authorized.size())result.status="incomplete";
    else {
      result.status="matched";
      result.depots=plan.selected;
    }
  }
  if(observe)observe({app_id,result.status,result.resolution.source_id,result.depots});
  return result;
}
} // namespace playden::sls
