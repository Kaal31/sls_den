#pragma once
#include "sls/install_handoff.h"
#include "sls/provider_discovery.h"

namespace playden::sls {
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
