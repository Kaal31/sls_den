#pragma once
// Platform-independent SLSDeck source catalogue for Windows and PS5 hosts.
// UI and transports depend on this model; this model never accesses networks or secrets.
#include <array>
#include <cstddef>
#include <set>
#include <string>
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
inline constexpr std::array<KeyDefinition,4> kKeyDefinitions{{
    {"moapikey","Hubcap / Morrenus API key"},
    {"ryuu","Ryuu API key"},
    {"steamweb","Steam Web API key"},
    {"luatools","lua.tools manual access token"}
}};
inline bool ValidKeyId(std::string_view id) {
    for(const auto& field:kKeyDefinitions) if(field.id==id) return true;
    return false;
}
class ProviderSelection {
public:
    bool enabled(std::size_t index) const {
        return index<kDefaultProviders.size() &&
               !disabled_.contains(std::string(kDefaultProviders[index].id));
    }
    std::string disabled_csv() const {
        std::string result;
        for(const auto& id:disabled_){
            if(!result.empty()) result += ",";
            result += id;
        }
        return result;
    }
    bool load_disabled_csv(std::string_view csv) {
        std::set<std::string> next;
        std::size_t pos=0;
        while(pos<csv.size()){
            const auto end=csv.find(',',pos);
            const auto part=csv.substr(pos,end==std::string_view::npos?csv.size()-pos:end-pos);
            bool found=false;
            for(const auto& provider:kDefaultProviders)
                if(provider.id==part){found=true;break;}
            if(!found)return false;
            next.insert(std::string(part));
            if(end==std::string_view::npos)break;
            pos=end+1;
        }
        disabled_=std::move(next);
        return true;
    }
    bool toggle(std::size_t index) {
        if(index>=kDefaultProviders.size()) return false;
        const std::string id(kDefaultProviders[index].id);
        if(!disabled_.erase(id)) disabled_.insert(id);
        return true;
    }
private:
    std::set<std::string> disabled_;
};
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
