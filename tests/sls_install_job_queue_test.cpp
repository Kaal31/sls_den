// Deterministic catalog -> manifest -> authorized install handoff lifecycle test.
// The fake installer records calls, never retrieves or installs game content.
#include "sls/install_job_queue.h"
#include "sls/steam_catalog_search.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace playden;
static void Check(bool ok,const char* why) {
  if(!ok){std::cerr<<"FAIL: "<<why<<"\n";std::exit(1);}
}
int main(){
  sls::SteamCatalogPage catalog;
  Check(sls::ParseSteamStoreSearchResponse(
    R"({"total_count":1,"results_html":"<a data-ds-appid=\"620\"><span class=\"title\">Portal 2</span></a>"})",
    &catalog) && catalog.titles.size()==1 && catalog.titles[0].app_id==620,
    "public Steam-wide catalog returns an unowned search entry");
  const std::vector<sls::Source> sources{
    {"down","Unavailable","https://down.example/{appid}.lua",true},
    {"up","Available","https://up.example/{appid}.lua",true}};
  int fetches=0;
  const auto fetch=[&](const std::string& url,std::string* payload,std::string*){
    ++fetches;
    if(url.find("down.example")!=std::string::npos)return false;
    *payload=R"(addappid(620)
setManifestid(620, "999999")
)";
    return true;
  };
  sls::HandoffJobQueue queue;
  // A catalog title and available SLS metadata alone never authorize installation.
  const auto no_license=sls::ResolveInstallHandoff(620,sources,fetch,{});
  Check(no_license.resolution.ok && no_license.depots.empty() &&
        queue.Prepare({620,no_license.status,no_license.resolution.source_id,
                       no_license.depots},{}).state==
          sls::HandoffJobState::AwaitingAuthorization,
        "unowned public result remains awaiting authorization");
  int installs=0;
  Check(!queue.Submit(620,[&](std::uint32_t,const auto&,std::string*){
      ++installs;return true;
    }) && installs==0,"unlicensed catalog game cannot reach installer");
  steam::DepotInfo depot{};
  depot.id=620;depot.manifest_gid=123456;
  std::vector<steam::DepotInfo> licensed{depot};
  sls::HandoffRecord recorded;
  const auto authorized=sls::ResolveInstallHandoff(620,sources,fetch,licensed,
    [&](const auto& x){recorded=x;});
  Check(authorized.status=="matched" && recorded.depots.size()==1 &&
        recorded.depots[0].manifest_gid==123456 &&
        authorized.resolution.metadata.depots[0].manifest_gid==999999,
        "Steam-authorized manifest GID overrides third-party metadata");
  auto& initial=queue.Prepare(recorded,licensed);
  Check(initial.state==sls::HandoffJobState::Queued &&
        initial.depots[0].manifest_gid==123456,
        "verified handoff creates queued install job");
  sls::HandoffRecord forged=recorded;
  forged.depots[0].manifest_gid=999999;
  sls::HandoffJobQueue forbidden;
  Check(forbidden.Prepare(forged,licensed).state==
          sls::HandoffJobState::AwaitingAuthorization,
        "forged source-provided GID cannot create authorized install");
  std::string last_error;
  bool fail_once=true;
  const auto installer=[&](std::uint32_t app,const auto& depots,std::string* error){
    ++installs;
    Check(app==620 && depots.size()==1 && depots[0].manifest_gid==123456,
          "installer receives only Steam-approved depot plan");
    if(fail_once){fail_once=false;*error="temporary storage outage";return false;}
    return true;
  };
  Check(!queue.Submit(620,installer) &&
        queue.Find(620)->state==sls::HandoffJobState::Failed &&
        queue.Find(620)->error=="temporary storage outage",
        "installer rejection becomes a failed job");
  Check(queue.Retry(620) && queue.Submit(620,installer) &&
        queue.Find(620)->state==sls::HandoffJobState::Downloading &&
        queue.Find(620)->attempts==2,
        "failed job retry queues exactly one new submission");
  Check(queue.Progress(620,50,100) && !queue.Progress(620,40,100) &&
        queue.Progress(620,100,100) &&
        queue.Find(620)->state==sls::HandoffJobState::Verifying,
        "progress monotonicity and verify transition");
  Check(queue.Finish(620,false,"checksum mismatch") &&
        queue.Find(620)->state==sls::HandoffJobState::Failed,
        "verification failure visible to queue");
  Check(queue.Retry(620) && queue.Submit(620,installer) &&
        queue.Progress(620,100,100) && queue.Finish(620,true) &&
        queue.Find(620)->state==sls::HandoffJobState::Installed &&
        !queue.Cancel(620),"successful re-verification creates installed job");
  sls::HandoffJobQueue cancellation;
  cancellation.Prepare(recorded,licensed);
  Check(cancellation.Cancel(620) &&
        !cancellation.Submit(620,installer) &&
        cancellation.Find(620)->state==sls::HandoffJobState::Cancelled,
        "queued cancellation cannot submit files");
  Check(fetches==4 && installs==3,
        "fallback and retry do not duplicate unrelated installer submissions");
  std::cout<<"PASS: Steam-wide catalog to authorization-gated job, retry, progress, verify and cancel\n";
}
