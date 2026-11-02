#include "sls/config_fix.h"
#include "sls/game_fix_service.h"
#include "sls/fix_source_discovery.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>
namespace fs=std::filesystem;
using playden::sls::ApplyConfigFix;
using playden::sls::RestoreConfigFix;
using playden::sls::FixResult;
static void check(bool ok,const char* message){
  if(!ok){std::cerr<<"FAIL: "<<message<<"\n";std::exit(1);}
}
static std::string read(const fs::path& p){
  std::ifstream f(p,std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
}
int main(){
 const auto root=fs::temp_directory_path()/"lua-den-config-fix-test";
 std::error_code ec;
 fs::remove_all(root,ec);
 fs::create_directories(root/"drive_c"/"game",ec);
 check(!ec,"fixture directory created");
 const auto file=root/"drive_c"/"game"/"settings.ini";
 {std::ofstream o(file,std::ios::binary);o<<"original=1\n";}
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","original=2\n")==FixResult::Applied,"apply replacement");
 check(read(file)=="original=2\n","replacement written");
 check(playden::sls::InspectConfigFix(root,"drive_c/game/settings.ini")==FixResult::Applied,"applied-state probe verifies exact contents");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","bad")==FixResult::AlreadyApplied,"no double backup");
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Restored,"unfix succeeds");
 check(read(file)=="original=1\n","original restored byte-for-byte");
 check(playden::sls::InspectConfigFix(root,"drive_c/game/settings.ini")==FixResult::NotApplied,"probe reports restored fix as inactive");
 check(ApplyConfigFix(root,"drive_c/game/new.ini","created=true\n")==FixResult::Applied,"create file");
 check(RestoreConfigFix(root,"drive_c/game/new.ini")==FixResult::Restored && !fs::exists(root/"drive_c"/"game"/"new.ini"),"remove created file");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","patched=1\n")==FixResult::Applied,"apply conflict fixture");
 {std::ofstream o(file,std::ios::binary|std::ios::trunc);o<<"user-change=1\n";}
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Conflict,"preserve user modifications");
 check(read(file)=="user-change=1\n","modified content retained");
 check(playden::sls::InspectConfigFix(root,"drive_c/game/settings.ini")==FixResult::Conflict,"probe detects modified fix");
 {std::ofstream o(file,std::ios::binary|std::ios::trunc);o<<"patched=1\n";}
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Restored,"restore after resolving conflict");
 // A created file changed by the user must never be deleted by Unfix.
 check(ApplyConfigFix(root,"drive_c/game/new.ini","managed=true\n")==FixResult::Applied,"recreate new file");
 {std::ofstream o(root/"drive_c"/"game"/"new.ini",std::ios::binary|std::ios::trunc);o<<"user-owned=true\n";}
 check(RestoreConfigFix(root,"drive_c/game/new.ini")==FixResult::Conflict,"created file changes protected");
 check(read(root/"drive_c"/"game"/"new.ini")=="user-owned=true\n","modified new file kept");
 {std::ofstream o(root/"drive_c"/"game"/"new.ini",std::ios::binary|std::ios::trunc);o<<"managed=true\n";}
 check(RestoreConfigFix(root,"drive_c/game/new.ini")==FixResult::Restored,"recovered after created file conflict");
 check(RestoreConfigFix(root,"drive_c/game/new.ini")==FixResult::NotApplied,"idempotent Unfix");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini","original=3\n")==FixResult::Applied,"apply replacement after successful restore");
 check(read(file)=="original=3\n","replacement after restore");
 check(RestoreConfigFix(root,"drive_c/game/settings.ini")==FixResult::Restored,"restore repeated replacement");
 check(read(file)=="original=1\n","repeated replacement restored original");
 // A stale transaction file must not lead to a new backup, marker, or
 // destructive replacement. Recovery remains explicit until verified.
 const auto blocked=root/"drive_c"/"game"/"blocked.ini";
 {std::ofstream out(blocked,std::ios::binary);out<<"user-value=1\n";}
 const auto stale=fs::path(blocked.string()+".lua-den-restore-staging");
 {std::ofstream out(stale,std::ios::binary);out<<"recovery-bytes\n";}
 check(ApplyConfigFix(root,"drive_c/game/blocked.ini","patched\n")==FixResult::Conflict,
       "stale restore transaction blocks Apply");
 check(read(blocked)=="user-value=1\n","stale restore leaves user file unchanged");
 check(read(stale)=="recovery-bytes\n","stale transaction retained");
 check(!fs::exists(blocked.string()+".lua-den-original"),"stale transaction cannot create backup");
 fs::remove(stale,ec);
 check(ApplyConfigFix(root,"drive_c/game/blocked.ini","patched\n")==FixResult::Applied,
       "Apply works after stale transaction is resolved");
 check(RestoreConfigFix(root,"drive_c/game/blocked.ini")==FixResult::Restored,
       "recovered Apply can be undone");
 check(ApplyConfigFix(root,"../escape.ini","bad")==FixResult::UnsafePath,"reject traversal");
 check(ApplyConfigFix(root,fs::absolute(root/"escape.ini"),"bad")==FixResult::UnsafePath,"reject absolute path");
 check(ApplyConfigFix(root,"drive_c/game/settings.ini",std::string(1024*1024+1,'x'))==FixResult::TooLarge,"reject oversized payload");
 playden::sls::CuratedConfigFix vetted{12345,"local-config","Trusted local test fix","drive_c/game/curated.ini","enabled=1\n","1.0"};
 playden::sls::VerifiedGameRoot authorized{12345,root,"1.0",true,true,true};
 check(playden::sls::CheckGameFixAvailability(authorized,{vetted})==playden::sls::GameFixAvailability::Available,"curated fix is available");
 auto guarded=vetted;
 guarded.relative_path="drive_c/game/settings.ini";
 guarded.expected_original="incorrect-original\\n";
 check(playden::sls::ApplyCuratedGameFix(authorized,guarded)==FixResult::Conflict,
       "vetted fix rejects unexpected original content");
 check(playden::sls::InspectConfigFix(root,guarded.relative_path)!=FixResult::Applied,
       "rejected guarded fix leaves no applied marker");
 guarded.expected_original=read(root/guarded.relative_path);
 check(playden::sls::ApplyCuratedGameFix(authorized,guarded)==FixResult::Applied,
       "vetted fix accepts matching original contents");
 check(playden::sls::RemoveCuratedGameFix(authorized,guarded)==FixResult::Restored,
       "guarded fix can be reversed");
 check(playden::sls::ApplyCuratedGameFix(authorized,vetted)==FixResult::Applied,"curated fix applies to verified game");
 auto upgraded=authorized;upgraded.version="2.0";
 check(playden::sls::InspectConfigFix(root,vetted.relative_path)==FixResult::Applied,"fix remains present after version changes");
 check(playden::sls::RemoveCuratedGameFix(upgraded,vetted)==FixResult::Restored,"Unfix remains available after game update");
 {
   using namespace playden::sls;
   int lookups=0;
   const auto listings=DiscoverFixListings(12345,"Fixture Game",
       {"perondepot"},{"luatools-catalog"},{"ryuu"},
       [&](std::string_view id,std::uint32_t app,std::string_view name,std::vector<std::string>* labels) {
         ++lookups;
         check(id=="ryuu" && app==12345 && name=="Fixture Game","bounded lookup uses selected provider");
         labels->push_back("Example listing");
         return true;
       });
   check(lookups==1 && listings.matches.size()==1 && listings.matches[0].source_id=="ryuu",
         "only enabled and configured source queried");
   check(listings.attempts.size()==kFixSources.size() &&
         listings.attempts[1].status==FixSourceStatus::Disabled &&
         listings.attempts[2].status==FixSourceStatus::AdapterUnavailable &&
         listings.attempts[4].status==FixSourceStatus::AdapterUnavailable,
         "disabled and unsupported adapters never queried");
   const auto independent=DiscoverFixListingsHttp(12345,"Fixture Game",{}, {},
       {"ryuu","perondepot","luatools-direct","nerai"},
       [](std::string_view id,std::uint32_t,std::string_view) -> FixLookupResponse {
         if(id=="ryuu") return {404,{"Not actually available"}};
         if(id=="perondepot") throw std::runtime_error("connection timed out");
         if(id=="luatools-direct") return {503,{"Service unavailable"}};
         if(id=="nerai") return {200,{"Verified listing"}};
         return {0,{}};
       });
   check(independent.matches.size()==1 && independent.matches[0].source_id=="nerai",
         "404 timeout and 503 do not display phantom fixes or suppress good provider");
   check(independent.attempts[0].status==FixSourceStatus::NoMatch &&
         independent.attempts[1].status==FixSourceStatus::LookupFailed &&
         independent.attempts[2].status==FixSourceStatus::LookupFailed &&
         independent.attempts[3].status==FixSourceStatus::Found,
         "provider errors are reported independently");
   const auto missing=DiscoverFixListings(12345,"Fixture Game",{}, {}, {"luatools-catalog"},
       [](std::string_view,std::uint32_t,std::string_view,std::vector<std::string>*) { return true; });
   check(missing.attempts[4].status==FixSourceStatus::MissingAccount,
         "account-gated source refuses unauthenticated discovery");
 }
 check(playden::sls::kFixSources.size()==7, "SLSDeck fix provider registry contains seven source families");
 for (const auto& source : playden::sls::kFixSources) {
   auto sourced=vetted;
   sourced.source_id=std::string(source.id);
   check(playden::sls::ValidCuratedDefinition(sourced), "recognized SLSDeck fix source is valid metadata");
 }
 auto unknown_source=vetted;
 unknown_source.source_id="unknown-provider";
 check(!playden::sls::ValidCuratedDefinition(unknown_source), "unknown fix source metadata rejected");
 auto malformed=vetted;
 malformed.id="../cross-game";
 check(!playden::sls::ValidCuratedDefinition(malformed),"reject action-key injection");
 check(playden::sls::ApplyCuratedGameFix(authorized,malformed)==FixResult::UnsafePath,"invalid fix id cannot apply");
 malformed=vetted;
 malformed.relative_path="../escape.ini";
 check(!playden::sls::ValidCuratedDefinition(malformed),"reject traversal metadata");
 check(playden::sls::CheckGameFixAvailability(authorized,{malformed})==playden::sls::GameFixAvailability::NoDefinitions,
       "unsafe fix definition not listed");
 malformed=vetted;
 malformed.replacement.assign(1024*1024+1,'x');
 check(!playden::sls::ValidCuratedDefinition(malformed),"reject oversized curated payload");
 malformed=vetted;
 malformed.steam_app_id=0;
 check(!playden::sls::ValidCuratedDefinition(malformed),"reject missing AppID");
 auto no_install=authorized; no_install.installed=false;
 check(playden::sls::ApplyCuratedGameFix(no_install,vetted)==FixResult::UnsafePath,"no catalog-only fix application");
 auto disappeared=authorized; disappeared.root=root/"disconnected-drive";
 check(playden::sls::CheckGameFixAvailability(disappeared,{vetted})==playden::sls::GameFixAvailability::StorageUnavailable,
       "deleted or disconnected game directory not fix eligible");
 check(playden::sls::ApplyCuratedGameFix(disappeared,vetted)==FixResult::UnsafePath,
       "reject applying to missing installation directory");
 check(playden::sls::RemoveCuratedGameFix(disappeared,vetted)==FixResult::UnsafePath,
       "reject restoring through missing installation directory");
 auto other=authorized; other.app_id=12346;
 check(playden::sls::ApplyCuratedGameFix(other,vetted)==FixResult::UnsafePath,"no cross-game fix");
 auto unsupported=authorized; unsupported.runtime_supported=false;
 check(playden::sls::ApplyCuratedGameFix(unsupported,vetted)==FixResult::UnsafePath,"no unsupported-runtime fix");
 auto mismatch=authorized; mismatch.version="2.0";
 check(playden::sls::CheckGameFixAvailability(mismatch,{vetted})==playden::sls::GameFixAvailability::VersionMismatch,"version mismatch surfaced");
 fs::remove_all(root,ec);
 std::cout<<"PASS: reversible config fixes and path safety\n";
}
