#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace playden::sls {
struct DepotPin {
  std::uint32_t depot_id = 0;
  std::uint64_t manifest_gid = 0;
  bool enabled = true;
};
struct ManifestMetadata {
  std::uint32_t app_id = 0;
  std::vector<DepotPin> depots;
};
// Parses a bounded declarative subset of LuaTools configuration.
// No Lua execution, no ownership changes, and no depot download authorization.
bool ParseManifestMetadata(std::uint32_t app_id, std::string_view source,
                           ManifestMetadata* result, std::string* error = nullptr);
}
