#pragma once
// Platform-independent SLSDeck source catalogue for Windows and PS5 hosts.
// UI and transports depend on this model; this model never accesses networks or secrets.
#include <array>
#include <cstddef>
#include <string_view>

namespace playden::sls {
struct ProviderDefinition {
    std::string_view id;
    std::string_view name;
    std::string_view url_template;
    std::string_view required_key;
};
inline constexpr std::array<ProviderDefinition,5> kDefaultProviders{{
    {"ryuu","Ryuu","https://generator.ryuu.lol/api/download/{appid}?file_type=manifest",""},
    {"sushi","Sushi","https://raw.githubusercontent.com/sushi-dev55-alt/sushitools-games-repo-alt/refs/heads/main/{appid}.zip",""},
    {"sushi-cdn","Sushi CDN Mirror","https://cdn.jsdelivr.net/gh/sushi-dev55-alt/sushitools-games-repo-alt@main/{appid}.zip",""},
    {"twentytwo","TwentyTwo Cloud","https://api.twentytwocloud.com/download?appid={appid}",""},
    {"morrenus","Morrenus","https://hubcapmanifest.com/api/v1/manifest/{appid}?api_key=<moapikey>","moapikey"}
}};
struct KeyDefinition { std::string_view id; std::string_view label; };
inline constexpr std::array<KeyDefinition,3> kKeyDefinitions{{
    {"moapikey","Hubcap / Morrenus API key"},
    {"ryuu","Ryuu API key"},
    {"steamweb","Steam Web API key"}
}};
inline bool ValidKeyId(std::string_view id) {
    for(const auto& field:kKeyDefinitions) if(field.id==id) return true;
    return false;
}
inline bool ProviderEnabled(std::size_t index, std::string_view disabled_csv) {
    if(index>=kDefaultProviders.size()) return false;
    const auto id=kDefaultProviders[index].id;
    std::size_t pos=0;
    while(pos<disabled_csv.size()){
        const auto end=disabled_csv.find(',',pos);
        const auto part=disabled_csv.substr(pos,end==std::string_view::npos?disabled_csv.size()-pos:end-pos);
        if(part==id) return false;
        if(end==std::string_view::npos) break;
        pos=end+1;
    }
    return true;
}
} // namespace playden::sls
