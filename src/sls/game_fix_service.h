#pragma once
// Lua Den local, curated configuration fixes only. Never execute remote scripts.
// A host adapter must supply a verified installed-game root, not a catalog AppID.
#include "sls/config_fix.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace playden::sls {
enum class GameFixAvailability { Available, NotInstalled, StorageUnavailable,
                                 RuntimeUnsupported, NoDefinitions, VersionMismatch };
struct CuratedConfigFix {
    std::uint32_t steam_app_id = 0;
    std::string id;
    std::string title;
    std::filesystem::path relative_path;
    std::string replacement;  // trusted local bytes, never unvalidated provider content
    std::string installed_version; // exact match when specified
};
struct VerifiedGameRoot {
    std::uint32_t app_id = 0;
    std::filesystem::path root;
    std::string version;
    bool installed = false;
    bool volume_available = false;
    bool runtime_supported = false;
};
inline GameFixAvailability CheckGameFixAvailability(
    const VerifiedGameRoot& game, const std::vector<CuratedConfigFix>& definitions) {
    if (!game.installed || game.root.empty()) return GameFixAvailability::NotInstalled;
    if (!game.volume_available || !std::filesystem::is_directory(game.root))
        return GameFixAvailability::StorageUnavailable;
    if (!game.runtime_supported) return GameFixAvailability::RuntimeUnsupported;
    bool found = false;
    for (const auto& def : definitions) {
        if (def.steam_app_id != game.app_id) continue;
        found = true;
        if (def.installed_version.empty() || def.installed_version == game.version)
            return GameFixAvailability::Available;
    }
    return found ? GameFixAvailability::VersionMismatch : GameFixAvailability::NoDefinitions;
}
inline FixResult ApplyCuratedGameFix(const VerifiedGameRoot& game,
                                     const CuratedConfigFix& def) {
    if (def.steam_app_id != game.app_id || def.id.empty() ||
        (def.installed_version.size() && def.installed_version != game.version) ||
        CheckGameFixAvailability(game, {def}) != GameFixAvailability::Available)
        return FixResult::UnsafePath;
    return ApplyConfigFix(game.root, def.relative_path, def.replacement);
}
inline FixResult RemoveCuratedGameFix(const VerifiedGameRoot& game,
                                      const CuratedConfigFix& def) {
    if (def.steam_app_id != game.app_id || def.id.empty() ||
        !game.installed || !game.volume_available || !game.runtime_supported ||
        game.root.empty() || !std::filesystem::is_directory(game.root))
        return FixResult::UnsafePath;
    return RestoreConfigFix(game.root, def.relative_path);
}
} // namespace playden::sls
