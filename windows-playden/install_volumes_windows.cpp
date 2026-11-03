// Native Windows space detection; never run PS5 mount-point assumptions on NTFS.
#include "steam/install_volumes.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <limits>
namespace playden::steam {
namespace fs=std::filesystem;
bool VolumeSpaceSupported(){return true;}
std::optional<VolumeSpace> detail::VolumeSpaceFromBlocks(std::int64_t unit,std::uint64_t blocks,std::int64_t available) {
 if(unit<=0||!blocks||available<0||std::uint64_t(available)>blocks||
    blocks>std::numeric_limits<std::uint64_t>::max()/std::uint64_t(unit))return std::nullopt;
 return VolumeSpace{blocks*std::uint64_t(unit),std::uint64_t(available)*std::uint64_t(unit)};
}
VolumeSpace detail::WithoutConsoleReserve(VolumeSpace space,std::uint64_t reserve){
 space.free_bytes-=std::min(space.free_bytes,reserve);return space;
}
bool detail::VolumeMountMatches(const std::string& path,const std::string& mount){
 return !path.empty()&&!mount.empty()&&path==mount;
}
std::optional<VolumeSpace> QueryVolumeSpace(const std::string& input,std::uint64_t reserve){
 if(input.empty())return std::nullopt;
 fs::path path=fs::u8path(input);
 if(!path.is_absolute())return std::nullopt;
 std::error_code ec;
 // No traversal through reparse points, including any existing ancestor.
 path=path.lexically_normal();
 for(fs::path part=path;!part.empty();part=part.parent_path()){
   const DWORD attr=GetFileAttributesW(part.c_str());
   if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_REPARSE_POINT))return std::nullopt;
   if(part==part.root_path())break;
 }
 while(!fs::exists(path,ec)){
   if(ec || path==path.root_path() || path.empty())return std::nullopt;
   path=path.parent_path();
 }
 if(ec||!fs::is_directory(path,ec)||ec)return std::nullopt;
 ULARGE_INTEGER available{},total{},free{};
 if(!GetDiskFreeSpaceExW(path.c_str(),&available,&total,&free))return std::nullopt;
 return detail::WithoutConsoleReserve({total.QuadPart,available.QuadPart},reserve);
}
std::vector<InstallVolume> DiscoverInstallVolumes(const std::vector<VolumeCandidate>& candidates,const AsRoot&){
 std::vector<InstallVolume> volumes;
 for(const auto& candidate:candidates) {
   const fs::path root=fs::u8path(candidate.games_root);
   const fs::path mount=fs::u8path(candidate.mount);
   std::error_code ec;
   if(!root.is_absolute()||!mount.is_absolute()||!fs::is_directory(mount,ec)||ec)continue;
   // Do not create directories merely to list a drive. Privileged live
   // installation must validate its selected root separately.
   volumes.push_back({candidate.id,candidate.label,candidate.games_root,
                      QueryVolumeSpace(candidate.games_root,0).has_value()});
 }
 return volumes;
}
std::vector<VolumeCandidate> ConsoleVolumeCandidates(const std::string& title_root,const std::string& games_root){
 const auto root=games_root.empty()?fs::u8path(title_root)/"games":fs::u8path(games_root);
 const auto folder=root.parent_path();
 return {{InstallService::kPrimaryVolumeId,"Windows storage",folder.u8string().empty()?"":folder.string(),root.string()}};
}
} // namespace playden::steam
