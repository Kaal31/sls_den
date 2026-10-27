#pragma once
#include "sls/manifest_metadata.h"
#include "sls/source_catalog.h"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace playden::sls {
// Transport is supplied by the host, allowing fixture tests and a future Playden HTTP adapter.
// Implementations must enforce TLS validation, redirects policy, timeouts and response size bounds.
using SourceFetch = std::function<bool(const std::string& url, std::string* payload, std::string* error)>;
struct SourceAttempt {
  std::string source_id;
  std::string status;
};
struct Resolution {
  bool ok = false;
  std::string source_id;
  ManifestMetadata metadata;
  std::vector<SourceAttempt> attempts;
};
Resolution ResolveManifest(std::uint32_t app_id, const std::vector<Source>& sources,
                           const SourceFetch& fetch);
}
