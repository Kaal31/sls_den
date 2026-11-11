#pragma once
// Playden-native registry for public Steam catalog registrations.
// No record in this registry is evidence of Steam ownership, an install, or a license.
#include "sls/manifest_metadata.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace playden::sls {
struct AddedGameRecord {
  std::uint32_t app_id=0;
  std::string title;
  std::string source_id;
  ManifestMetadata metadata;
};
class AddedGameRegistry {
public:
  const std::map<std::uint32_t,AddedGameRecord>& entries() const {return entries_;}
  const AddedGameRecord* Find(std::uint32_t app) const {
    auto found=entries_.find(app);
    return found==entries_.end()?nullptr:&found->second;
  }
  bool Upsert(std::uint32_t app,std::string title) {
    if(!app || title.empty() || title.size()>256 || entries_.size()>=10000 && !entries_.count(app))
      return false;
    auto& record=entries_[app];
    record.app_id=app;
    record.title=std::move(title);
    record.metadata.app_id=app;
    return true;
  }
  bool ApplyDiscovery(std::uint32_t app,std::string source,const ManifestMetadata& metadata) {
    auto* record=Find(app);
    if(!record || source.empty() || source.size()>128 || metadata.app_id!=app ||
       metadata.depots.size()>4096)return false;
    for(const auto& depot:metadata.depots)
      if(!depot.depot_id || !depot.manifest_gid)return false;
    auto& writable=entries_.at(app);
    writable.source_id=std::move(source);
    writable.metadata=metadata;
    return true;
  }
  bool Erase(std::uint32_t app){return entries_.erase(app)>0;}
  bool Save(const std::filesystem::path& root) const {
    if(root.empty())return false;
    std::error_code ec;
    std::filesystem::create_directories(root,ec);
    if(ec)return false;
    const auto target=root/"added-games-v2.txt";
    const auto temp=root/"added-games-v2.tmp";
    {
      std::ofstream out(temp,std::ios::binary|std::ios::trunc);
      if(!out)return false;
      out<<"SLS-ADDED-GAMES 2\n";
      for(const auto& [app,record]:entries_){
        out<<app<<' '<<std::quoted(record.title)<<' '
           <<std::quoted(record.source_id)<<' '
           <<record.metadata.depots.size()<<'\n';
        for(const auto& depot:record.metadata.depots)
          out<<depot.depot_id<<' '<<depot.manifest_gid<<' '
             <<(depot.enabled?1:0)<<'\n';
      }
      out.flush();
      if(!out)return false;
    }
    std::filesystem::rename(temp,target,ec);
    if(!ec)return true;
    // Windows may refuse replacement. Preserve the original if publication fails.
    const auto backup=root/"added-games-v2.bak";
    std::error_code ignored;
    std::filesystem::remove(backup,ignored);
    ec.clear();
    if(std::filesystem::exists(target,ignored))std::filesystem::rename(target,backup,ec);
    if(!ec)std::filesystem::rename(temp,target,ec);
    if(ec && std::filesystem::exists(backup,ignored))
      std::filesystem::rename(backup,target,ignored);
    if(!ec)std::filesystem::remove(backup,ignored);
    return !ec;
  }
  bool Load(const std::filesystem::path& root) {
    if(root.empty())return false;
    std::map<std::uint32_t,AddedGameRecord> parsed;
    std::ifstream in(root/"added-games-v2.txt",std::ios::binary);
    if(!in) {
      // One-time migration of the old catalog-only registration format.
      std::ifstream legacy(root/"added-games.txt",std::ios::binary);
      if(!legacy)return false;
      std::uint64_t app=0;
      std::string title;
      while(legacy>>app>>std::quoted(title)) {
        if(app==0 || app>UINT32_MAX || title.empty() || title.size()>256 ||
           parsed.size()>=10000 || parsed.count(static_cast<std::uint32_t>(app)))
          return false;
        AddedGameRecord entry;
        entry.app_id=static_cast<std::uint32_t>(app);
        entry.title=std::move(title);
        entry.metadata.app_id=entry.app_id;
        parsed.emplace(entry.app_id,std::move(entry));
      }
      if(!legacy.eof())return false;
      entries_=std::move(parsed);
      return true;
    }
    std::string header;
    if(!std::getline(in,header) || header!="SLS-ADDED-GAMES 2")return false;
    while(true) {
      in>>std::ws;
      if(in.eof())break;
      std::uint64_t app=0;
      AddedGameRecord entry;
      std::size_t count=0;
      if(!(in>>app>>std::quoted(entry.title)>>std::quoted(entry.source_id)>>count) ||
         !app || app>UINT32_MAX || entry.title.empty() ||
         entry.title.size()>256 || entry.source_id.size()>128 ||
         count>4096 || parsed.size()>=10000 ||
         parsed.count(static_cast<std::uint32_t>(app)))return false;
      entry.app_id=static_cast<std::uint32_t>(app);
      entry.metadata.app_id=entry.app_id;
      for(std::size_t i=0;i<count;++i){
        std::uint64_t depot_id=0,gid=0;
        int enabled=0;
        if(!(in>>depot_id>>gid>>enabled) || !depot_id || depot_id>UINT32_MAX ||
           !gid || (enabled!=0 && enabled!=1))return false;
        for(const auto& prior:entry.metadata.depots)
          if(prior.depot_id==depot_id)return false;
        entry.metadata.depots.push_back({static_cast<std::uint32_t>(depot_id),gid,enabled!=0});
      }
      parsed.emplace(entry.app_id,std::move(entry));
    }
    entries_=std::move(parsed);
    return true;
  }
private:
  std::map<std::uint32_t,AddedGameRecord> entries_;
};
} // namespace playden::sls
