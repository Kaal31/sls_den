#pragma once
// Shared source discovery: policy and provider order are platform independent.
// Hosts provide an HTTP or fixture fetch callback; this module never performs I/O.
// Keyed providers are excluded until an authenticated transport is installed.
#include "sls/provider_registry.h"
#include "sls/source_resolver.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace playden::sls {
inline std::vector<Source> ConfiguredSources(const ProviderSelection& selection) {
    std::vector<Source> sources;
    for(std::size_t i=0;i<kDefaultProviders.size();++i) {
        const auto& provider=kDefaultProviders[i];
        if(!selection.enabled(i))continue;
        if(!provider.required_key.empty())continue; // never expose secrets in URLs or logs
        sources.push_back({std::string(provider.id),std::string(provider.name),
                           std::string(provider.url_template),true,false});
    }
    return sources;
}
struct ProviderDiagnostic {
    std::string id;
    std::string status; // disabled, missing_key, ready, fetch_failed, invalid_manifest, success
};
inline std::vector<ProviderDiagnostic> DiagnoseProviders(
    const ProviderSelection& selection,
    const std::vector<std::string>& configured_key_ids) {
    std::vector<ProviderDiagnostic> result;
    for(std::size_t i=0;i<kDefaultProviders.size();++i) {
        const auto& provider=kDefaultProviders[i];
        std::string status="ready";
        if(!selection.enabled(i))status="disabled";
        else if(!provider.required_key.empty() &&
                std::find(configured_key_ids.begin(),configured_key_ids.end(),
                          std::string(provider.required_key))==configured_key_ids.end())
            status="missing_key";
        else if(!provider.required_key.empty())status="credential_configured";
        result.push_back({std::string(provider.id),status});
    }
    return result;
}
inline Resolution DiscoverManifest(std::uint32_t app_id,
                                   const ProviderSelection& selection,
                                   const SourceFetch& fetch) {
    return ResolveManifest(app_id,ConfiguredSources(selection),fetch);
}
} // namespace playden::sls
