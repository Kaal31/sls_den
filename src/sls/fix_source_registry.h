#pragma once
// Source identity for Fixes, kept separate from manifest/depot providers.
// Mirrors SLSDeck's experimental fix lookup categories. This is metadata,
// NOT permission to install arbitrary downloaded binaries on PS5.
#include <array>
#include <string_view>

namespace playden::sls {
struct FixSourceDefinition {
    std::string_view id;
    std::string_view title;
    bool network;
    bool requires_account;
    bool requires_platform_adapter;
};
inline constexpr std::array<FixSourceDefinition,7> kFixSources{{
    {"ryuu", "Ryuu fix catalogue", true, false, true},
    {"perondepot", "PerOnDepot online-fix mirror", true, false, true},
    {"luatools-direct", "lua.tools direct fix files", true, false, true},
    {"nerai", "NERAI fixes", true, false, true},
    {"luatools-catalog", "lua.tools authenticated fix catalogue", true, true, true},
    {"unsteam", "Unsteam universal fix", true, false, true},
    {"custom", "User-imported fixes", false, false, true}
}};
inline bool KnownFixSource(std::string_view id) {
    if(id=="local") return true; // vetted, application-bundled config fixes
    for(const auto& source:kFixSources) if(source.id==id) return true;
    return false;
}
} // namespace playden::sls
