// Offline Windows preview: PS5 game-modification actions are unavailable.
// Never touch a real executable, DLC manifest, or Steam emulator directory.
#include "runtime/steamworks.h"
#include "runtime/steam_stub.h"
#include "runtime/dotnet_heap.h"
namespace playden::runtime {
std::vector<std::string> RetiredSteamApiSha256(){return {};}
std::vector<std::string> FindSteamApi(const std::string&){return {};}
SteamworksResult EmulateSteamworks(const std::string&,const std::string&,const SteamworksIdentity&,const std::vector<std::string>&){
 return {false,"Steamworks emulation is disabled in the Windows offline preview",{},{}};
}
SteamworksResult RestoreSteamworks(const std::string&,const std::vector<std::string>&){
 return {false,"Steamworks modifications are disabled in the Windows offline preview",{},{}};
}
SteamworksResult ProvideLegacySteam(const std::string&,const std::string&,const std::vector<std::string>&){
 return {false,"Legacy Steam modifications are disabled in the Windows offline preview",{},{}};
}
SteamworksResult RemoveLegacySteam(const std::string&,const std::vector<std::string>&){
 return {false,"Legacy Steam modifications are disabled in the Windows offline preview",{},{}};
}
std::vector<std::string> SteamInterfaces(std::string_view){return {};}
std::string SerializeSteamworksFacts(const std::vector<std::uint32_t>&,const std::vector<std::uint32_t>&){return {};}
bool WriteSteamworksFacts(const std::string&,const std::vector<std::uint32_t>&,const std::vector<std::uint32_t>&){return false;}
bool ReadSteamworksFacts(const std::string&,SteamworksIdentity*){return false;}
SteamStubInfo InspectSteamStub(const std::string&){return {SteamStubKind::kNone,"Unavailable in Windows offline preview",0};}
bool UnpackSteamStub(std::string*,std::string* why){
 if(why)*why="SteamStub unpacking is disabled in the Windows offline preview";
 return false;
}
SteamStubResult UnpackSteamStubPrograms(const std::string&,const std::vector<std::string>&,std::size_t){
 return {false,"SteamStub unpacking is disabled in the Windows offline preview",{},{}};
}
DotnetHeapResult LimitDotnetHeap(const std::string&){
 return {false,"Game runtime modifications are disabled in the Windows offline preview",{},{}};
}
}
