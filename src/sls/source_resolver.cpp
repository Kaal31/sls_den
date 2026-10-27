#include "sls/source_resolver.h"
#include <utility>

namespace playden::sls {
Resolution ResolveManifest(std::uint32_t app_id, const std::vector<Source>& sources,
                           const SourceFetch& fetch) {
  Resolution result;
  if (!app_id || !fetch) return result;
  // Disabled and API-key-only sources are excluded until a credential-aware transport exists.
  for (const auto& source : ActiveSources(sources)) {
    const auto url = SourceUrl(source, app_id);
    if (url.empty()) continue;
    std::string payload, error;
    if (!fetch(url, &payload, &error)) {
      result.attempts.push_back({source.id, "fetch_failed"});
      continue;
    }
    if (payload.size() > 1024 * 1024) {
      result.attempts.push_back({source.id, "response_too_large"});
      continue;
    }
    ManifestMetadata parsed;
    if (!ParseManifestMetadata(app_id, payload, &parsed, &error) || parsed.depots.empty()) {
      result.attempts.push_back({source.id, "invalid_manifest"});
      continue;
    }
    result.attempts.push_back({source.id, "success"});
    result.ok = true;
    result.source_id = source.id;
    result.metadata = std::move(parsed);
    return result;
  }
  return result;
}
}
