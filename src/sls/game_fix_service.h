#pragma once
// Lua Den local, curated configuration fixes only. Never execute remote scripts.
// A host adapter must supply a verified installed-game root, not a catalog AppID.
#include "sls/config_fix.h"
#include <cstdint>
#include <algorithm>
#include <cctype>
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
// Reject ambiguous identifiers and unsafe catalogue metadata before displaying
// or attempting an operation. This is additional to filesystem containment checks.
inline bool ValidCuratedDefinition(const CuratedConfigFix& def) {
    if (!def.steam_app_id || def.id.empty() || def.id.size()>64 ||
        def.title.empty() || def.title.size()>128 ||
        def.replacement.size()>1024*1024 || def.relative_path.empty() ||
        def.relative_path.is_absolute() || def.relative_path.has_root_name())
        return false;
    if (!std::all_of(def.id.begin(),def.id.end(),[](unsigned char ch) {
        return (ch>='a'&&ch<='z') || (ch>='0'&&ch<='9') || ch=='-';}))
        return false;
    for (const auto& component : def.relative_path)
        if (component.empty() || component=="." || component=="..") return false;
    return true;
}
inline GameFixAvailability CheckGameFixAvailability(
    const VerifiedGameRoot& game, const std::vector<CuratedConfigFix>& definitions) {
    if (!game.installed || game.root.empty()) return GameFixAvailability::NotInstalled;
    std::error_code ec;
    if (!game.volume_available || !std::filesystem::is_directory(game.root,ec) || ec)
        return GameFixAvailability::StorageUnavailable;
    if (!game.runtime_supported) return GameFixAvailability::RuntimeUnsupported;
    bool found = false;
    for (const auto& def : definitions) {
        if (def.steam_app_id != game.app_id || !ValidCuratedDefinition(def)) continue;
        found = true;
        if (def.installed_version.empty() || def.installed_version == game.version)
            return GameFixAvailability::Available;
    }
    return found ? GameFixAvailability::VersionMismatch : GameFixAvailability::NoDefinitions;
}
inline FixResult ApplyCuratedGameFix(const VerifiedGameRoot& game,
                                     const CuratedConfigFix& def) {
    if (def.steam_app_id != game.app_id || !ValidCuratedDefinition(def) ||
        (def.installed_version.size() && def.installed_version != game.version) ||
        CheckGameFixAvailability(game, {def}) != GameFixAvailability::Available)
        return FixResult::UnsafePath;
    return ApplyConfigFix(game.root, def.relative_path, def.replacement);
}
inline FixResult RemoveCuratedGameFix(const VerifiedGameRoot& game,
                                      const CuratedConfigFix& def) {
    std::error_code ec;
    if (def.steam_app_id != game.app_id || !ValidCuratedDefinition(def) ||
        !game.installed || !game.volume_available || !game.runtime_supported ||
        game.root.empty() || !std::filesystem::is_directory(game.root,ec) || ec)
        return FixResult::UnsafePath;
    return RestoreConfigFix(game.root, def.relative_path);
}
} // namespace playden::sls
