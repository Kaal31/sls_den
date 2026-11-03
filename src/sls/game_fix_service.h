#pragma once
// Lua Den local, curated configuration fixes only. Never execute remote scripts.
// A host adapter must supply a verified installed-game root, not a catalog AppID.
#include "sls/config_fix.h"
#include "sls/fix_source_registry.h"
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace playden::sls {
enum class GameFixAvailability { Available, NotInstalled, StorageUnavailable,
                                 RuntimeUnsupported, NoDefinitions, VersionMismatch };
// Shared capability contract. Providers describe fixes but never grant a host
// permission to write files. Windows and PS5 supply their own vetted adapters.
enum class FixOperation { ConfigFile, IniEdit, FileReplacement, WineRegistry, Proton, Dxvk };
struct FixCapabilities {
    bool config_files = false;
    bool ini_editing = false;
    bool file_replacement = false;
    bool wine_registry = false;
    bool proton = false;
    bool dxvk = false;
    bool Supports(FixOperation operation) const {
        switch (operation) {
          case FixOperation::ConfigFile: return config_files;
          case FixOperation::IniEdit: return ini_editing;
          case FixOperation::FileReplacement: return file_replacement;
          case FixOperation::WineRegistry: return wine_registry;
          case FixOperation::Proton: return proton;
          case FixOperation::Dxvk: return dxvk;
        }
        return false;
    }
};
struct FixApplicationAdapter {
    FixCapabilities capabilities;
    // Called only after shared installation, version and expected-byte checks.
    std::function<FixResult(const std::filesystem::path&, const std::filesystem::path&,
                            std::string_view)> apply_config;
    std::function<FixResult(const std::filesystem::path&, const std::filesystem::path&)> restore_config;
};
inline FixApplicationAdapter LocalConfigFileFixAdapter() {
    FixApplicationAdapter adapter;
    adapter.capabilities.config_files = true;
    adapter.apply_config = ApplyConfigFix;
    adapter.restore_config = RestoreConfigFix;
    return adapter;
}
struct CuratedConfigFix {
    std::uint32_t steam_app_id = 0;
    std::string id;
    std::string title;
    std::filesystem::path relative_path;
    std::string replacement;  // trusted local bytes, never unvalidated provider content
    std::string installed_version; // exact match when specified
    std::string expected_original; // optional exact contents, checked before first Apply
    std::string source_id = "local"; // source identity is not executable trust
    FixOperation operation = FixOperation::ConfigFile; // default preserves existing catalogues
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
    // Only config-file operations have an audited shared apply contract today.
    if (def.operation != FixOperation::ConfigFile) return false;
    if (!def.steam_app_id || !KnownFixSource(def.source_id) ||
        def.id.empty() || def.id.size()>64 ||
        def.title.empty() || def.title.size()>128 ||
        def.replacement.size()>1024*1024 || def.expected_original.size()>1024*1024 || def.relative_path.empty() ||
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
inline FixResult ApplyCuratedGameFixWithAdapter(const VerifiedGameRoot& game,
                                     const CuratedConfigFix& def, const FixApplicationAdapter& adapter) {
    if (!adapter.capabilities.Supports(def.operation) || !adapter.apply_config)
        return FixResult::UnsafePath;
    if (def.steam_app_id != game.app_id || !ValidCuratedDefinition(def) ||
        (def.installed_version.size() && def.installed_version != game.version) ||
        CheckGameFixAvailability(game, {def}) != GameFixAvailability::Available)
        return FixResult::UnsafePath;
    // Reject incompatible or user-modified config files before changing bytes.
    // Existing sidecars remain the fix engine's responsibility; never bypass them.
    if (!def.expected_original.empty()) {
        std::error_code ec;
        const auto target = game.root / def.relative_path;
        if (std::filesystem::is_symlink(target, ec) || ec)
            return FixResult::UnsafePath;
        if (!std::filesystem::is_regular_file(target, ec) || ec ||
            std::filesystem::file_size(target, ec) != def.expected_original.size() || ec)
            return FixResult::Conflict;
        std::ifstream input(target, std::ios::binary);
        if (!input) return FixResult::IoError;
        const std::string current((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
        if (current != def.expected_original) return FixResult::Conflict;
    }
    return adapter.apply_config(game.root, def.relative_path, def.replacement);
}
inline FixResult ApplyCuratedGameFix(const VerifiedGameRoot& game, const CuratedConfigFix& def) {
    return ApplyCuratedGameFixWithAdapter(game, def, LocalConfigFileFixAdapter());
}
inline FixResult RemoveCuratedGameFixWithAdapter(const VerifiedGameRoot& game,
                                      const CuratedConfigFix& def, const FixApplicationAdapter& adapter) {
    if (!adapter.capabilities.Supports(def.operation) || !adapter.restore_config)
        return FixResult::UnsafePath;
    std::error_code ec;
    if (def.steam_app_id != game.app_id || !ValidCuratedDefinition(def) ||
        !game.installed || !game.volume_available || !game.runtime_supported ||
        game.root.empty() || !std::filesystem::is_directory(game.root,ec) || ec)
        return FixResult::UnsafePath;
    return adapter.restore_config(game.root, def.relative_path);
}
inline FixResult RemoveCuratedGameFix(const VerifiedGameRoot& game, const CuratedConfigFix& def) {
    return RemoveCuratedGameFixWithAdapter(game, def, LocalConfigFileFixAdapter());
}
} // namespace playden::sls
