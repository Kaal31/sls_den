#pragma once
// SLSDeck-aligned Fixes discovery status. This component never downloads,
// extracts, executes, or installs third-party fix payloads.
// Platform adapters must separately verify provenance and compatibility.
#include "sls/fix_source_registry.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace playden::sls {
enum class FixSourceStatus {
    Disabled, MissingAccount, AdapterUnavailable, LookupFailed, NoMatch, Found
};
struct FixSourceMatch {
    std::string source_id;
    std::string label;
    // Display only; never an authorization to apply or execute a payload.
};
struct FixSourceAttempt {
    std::string source_id;
    FixSourceStatus status;
};
struct FixDiscoveryResult {
    std::vector<FixSourceMatch> matches;
    std::vector<FixSourceAttempt> attempts;
};
// A host implements a bounded, read-only provider-specific catalogue lookup.
// No raw download URL or executable bytes cross this interface.
using FixCatalogueLookup = std::function<bool(
    std::string_view source_id, std::uint32_t app_id, std::string_view game_name,
    std::vector<std::string>* labels)>;

inline FixDiscoveryResult DiscoverFixListings(
    std::uint32_t app_id, std::string_view game_name,
    const std::vector<std::string>& disabled_ids,
    const std::vector<std::string>& available_accounts,
    const std::vector<std::string>& available_adapters,
    const FixCatalogueLookup& lookup) {
    FixDiscoveryResult result;
    for(const auto& source : kFixSources) {
        const auto id = std::string(source.id);
        const auto contains = [&](const std::vector<std::string>& values) {
            return std::find(values.begin(), values.end(), id) != values.end();
        };
        if(contains(disabled_ids)) {
            result.attempts.push_back({id, FixSourceStatus::Disabled});
            continue;
        }
        if(source.requires_account && !contains(available_accounts)) {
            result.attempts.push_back({id, FixSourceStatus::MissingAccount});
            continue;
        }
        if(source.requires_platform_adapter && !contains(available_adapters)) {
            result.attempts.push_back({id, FixSourceStatus::AdapterUnavailable});
            continue;
        }
        if(app_id==0 || !lookup) {
            result.attempts.push_back({id, FixSourceStatus::LookupFailed});
            continue;
        }
        std::vector<std::string> labels;
        bool ok=false;
        try { ok=lookup(source.id, app_id, game_name, &labels); }
        catch(...) { ok=false; }
        if(!ok) {
            result.attempts.push_back({id, FixSourceStatus::LookupFailed});
            continue;
        }
        unsigned count=0;
        for(const auto& label : labels) {
            if(!label.empty() && label.size()<=128 && count<32) {
                result.matches.push_back({id,label});
                ++count;
            }
        }
        result.attempts.push_back({id, count ? FixSourceStatus::Found : FixSourceStatus::NoMatch});
    }
    return result;
}

// Transport-independent result: 404 and connection failures are never listings.
// A host adapter maps its HTTP status and network errors to this structure.
struct FixLookupResponse {
    int http_status = 0;  // 200 / 404 / 0 for connection failure
    std::vector<std::string> labels;
};
using FixHttpLookup = std::function<FixLookupResponse(
    std::string_view source_id, std::uint32_t app_id, std::string_view game_name)>;

inline FixDiscoveryResult DiscoverFixListingsHttp(
    std::uint32_t app_id, std::string_view game_name,
    const std::vector<std::string>& disabled_ids,
    const std::vector<std::string>& available_accounts,
    const std::vector<std::string>& available_adapters,
    const FixHttpLookup& lookup) {
    FixDiscoveryResult result;
    for(const auto& source : kFixSources) {
        const std::string id(source.id);
        const auto has = [&](const std::vector<std::string>& ids) {
            return std::find(ids.begin(),ids.end(),id)!=ids.end();
        };
        if(has(disabled_ids)) { result.attempts.push_back({id,FixSourceStatus::Disabled}); continue; }
        if(source.requires_account && !has(available_accounts)) {
            result.attempts.push_back({id,FixSourceStatus::MissingAccount}); continue;
        }
        if(!has(available_adapters)) {
            result.attempts.push_back({id,FixSourceStatus::AdapterUnavailable}); continue;
        }
        if(!app_id || !lookup) {
            result.attempts.push_back({id,FixSourceStatus::LookupFailed}); continue;
        }
        FixLookupResponse response;
        try { response=lookup(id,app_id,game_name); }
        catch(...) { result.attempts.push_back({id,FixSourceStatus::LookupFailed}); continue; }
        // Only HTTP 200 with a valid nonempty listing can produce a visible fix.
        if(response.http_status==404) {
            result.attempts.push_back({id,FixSourceStatus::NoMatch}); continue;
        }
        if(response.http_status!=200) {
            result.attempts.push_back({id,FixSourceStatus::LookupFailed}); continue;
        }
        unsigned count=0;
        for(const auto& label:response.labels) {
            if(!label.empty() && label.size()<=128 && count<32) {
                result.matches.push_back({id,label}); ++count;
            }
        }
        result.attempts.push_back({id,count?FixSourceStatus::Found:FixSourceStatus::NoMatch});
    }
    return result;
}

} // namespace playden::sls
