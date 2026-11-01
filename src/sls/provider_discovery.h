#pragma once
// Shared source discovery: policy and provider order are platform independent.
// Hosts provide an HTTP or fixture fetch callback; this module never performs I/O.
// Keyed providers are excluded until an authenticated transport is installed.
#include "sls/provider_registry.h"
#include "sls/source_resolver.h"
#include <algorithm>
#include <cstdint>
#include <functional>
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
// An adapter receives the credential separately from the URL template. Callers must
// never embed secrets into logs, diagnostic URLs or persisted source definitions.
using CredentialLookup = std::function<bool(std::string_view key_id, std::string* value)>;
using CredentialSourceFetch = std::function<bool(
    const std::string& provider_id, const std::string& url_template,
    std::uint32_t app_id, std::string_view credential,
    std::string* payload, std::string* error)>;

inline Resolution DiscoverManifestWithCredentials(
    std::uint32_t app_id, const ProviderSelection& selection,
    const CredentialLookup& lookup, const CredentialSourceFetch& fetch) {
    Resolution result;
    if(!app_id || !fetch)return result;
    for(std::size_t i=0;i<kDefaultProviders.size();++i){
        const auto& provider=kDefaultProviders[i];
        const std::string provider_id(provider.id);
        if(!selection.enabled(i)){
            result.attempts.push_back({provider_id,"disabled"});
            continue;
        }
        std::string secret;
        if(!provider.required_key.empty() &&
           (!lookup || !lookup(provider.required_key,&secret) || secret.empty())){
            result.attempts.push_back({provider_id,"missing_key"});
            continue;
        }
        std::string payload,error;
        const bool fetched=fetch(provider_id,std::string(provider.url_template),
                                 app_id,secret,&payload,&error);
        // Do not retain credential contents in the resolver beyond this request.
        std::fill(secret.begin(),secret.end(),'\\0');
        secret.clear();
        if(!fetched){
            result.attempts.push_back({provider_id,"fetch_failed"});
            continue;
        }
        if(payload.size()>1024*1024){
            result.attempts.push_back({provider_id,"response_too_large"});
            continue;
        }
        ManifestMetadata parsed;
        if(!ParseManifestMetadata(app_id,payload,&parsed,&error) || parsed.depots.empty()){
            result.attempts.push_back({provider_id,"invalid_manifest"});
            continue;
        }
        result.attempts.push_back({provider_id,"success"});
        result.ok=true;
        result.source_id=provider_id;
        result.metadata=std::move(parsed);
        return result;
    }
    return result;
}
} // namespace playden::sls
