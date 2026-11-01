#pragma once
// Portable, non-secret source selection storage. Hosts select the directory.
// Credentials MUST be stored separately in a secure platform credential store.
#include "sls/provider_registry.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace playden::sls {
inline bool LoadSelection(const std::filesystem::path& path, ProviderSelection* out) {
    if(!out) return false;
    std::ifstream in(path,std::ios::binary);
    if(!in) return false;
    std::string value;
    if(!std::getline(in,value) || value.size()>512) return false;
    if(in.peek()!=std::char_traits<char>::eof())return false;
    if(!value.empty() && value.back()=='\r')value.pop_back();
    return out->load_disabled_csv(value);
}
inline bool SaveSelection(const std::filesystem::path& path,const ProviderSelection& selection) {
    if(path.empty())return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec)return false;
    auto tmp=path;
    tmp += ".tmp"; // append natively; never round-trip Unicode Windows paths through ANSI
    {
        std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
        if(!out)return false;
        out<<selection.disabled_csv()<<"\n";
        out.flush();
        if(!out.good())return false;
    }
    // Do not remove the original unless replacement is required by the platform.
    std::filesystem::rename(tmp,path,ec);
#ifdef _WIN32
    if(ec) {
        std::error_code ignored;
        std::filesystem::remove(path,ignored);
        ec.clear();
        std::filesystem::rename(tmp,path,ec);
    }
#endif
    return !ec;
}
} // namespace playden::sls
