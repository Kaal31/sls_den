// Real shared SLS->Playden manifest handoff test with a recording installer adapter.
// Offline deterministic inputs. No real Steam credentials or network requests.
#include "sls/install_handoff.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace playden;
namespace {
void Check(bool ok,const char* why) {
  if(!ok){std::cerr<<"FAIL: "<<why<<"\n";std::exit(1);}
}
struct RecordingInstaller {
  sls::HandoffRecord latest;
  unsigned deliveries=0;
  void Receive(const sls::HandoffRecord& record){latest=record;++deliveries;}
};
}
int main() {
  steam::DepotInfo first{},second{};
  first.id=410; first.manifest_gid=1111;
  second.id=420; second.manifest_gid=2222;
  const std::vector<steam::DepotInfo> entitled{second,first};
  const std::vector<sls::Source> sources{
    {"primary","Primary","https://primary.example/{appid}.lua",true},
    {"backup","Backup","https://backup.example/{appid}.lua",true},
    {"off","Off","https://off.example/{appid}.lua",false}};
  const std::string valid="addappid(410)\nsetManifestid(410, \"9999\")\n"
                          "addappid(420)\nsetManifestid(420, \"8888\")\n";
  RecordingInstaller installer;
  std::vector<std::string> calls;
  const auto record=[&](const sls::HandoffRecord& value){installer.Receive(value);};
  const auto result=sls::ResolveInstallHandoff(400,sources,
    [&](const std::string& url,std::string* bytes,std::string*) {
      calls.push_back(url);
      if(url.find("primary.example")!=std::string::npos)return false;
      *bytes=valid;return true;
    },entitled,record);
  Check(result.status=="matched","provider metadata accepted");
  Check(installer.deliveries==1 && installer.latest.app_id==400 &&
        installer.latest.provider_id=="backup","actual handoff observer called");
  Check(calls.size()==2 && calls[0].find("primary")!=std::string::npos &&
        calls[1].find("backup")!=std::string::npos,"source order and fallback");
  Check(installer.latest.depots.size()==2 &&
        installer.latest.depots[0].id==420 && installer.latest.depots[0].manifest_gid==2222 &&
        installer.latest.depots[1].id==410 && installer.latest.depots[1].manifest_gid==1111,
        "installer receives Steam authorized depot order and GIDs, never provider overrides");

  calls.clear();
  auto bad=sls::ResolveInstallHandoff(400,sources,
    [&](const std::string& url,std::string* bytes,std::string*) {
      calls.push_back(url);
      *bytes="addappid(999)\nsetManifestid(999, \"1234\")\n";return true;
    },entitled,record);
  Check(bad.status=="unowned-depot" && bad.depots.size()==2 &&
        bad.depots[0].manifest_gid==2222,"unauthorized provider depot cannot enter plan");
  auto malformed=sls::ResolveInstallHandoff(400,sources,
    [](const std::string&,std::string* bytes,std::string*){*bytes="garbage";return true;},
    entitled,record);
  Check(malformed.status=="unavailable" && malformed.depots==result.depots,
        "malformed manifest leaves authorized plan untouched");
  int requests=0;
  auto none=sls::ResolveInstallHandoff(400,{},
    [&](const std::string&,std::string*,std::string*){++requests;return false;},entitled,record);
  Check(none.status=="disabled" && requests==0,"disabled configuration makes no network calls");
  auto unlicensed=sls::ResolveInstallHandoff(400,sources,
    [&](const std::string&,std::string* bytes,std::string*){*bytes=valid;return true;},{},record);
  Check(unlicensed.depots.empty() && unlicensed.status!="matched",
        "provider metadata cannot create Steam entitlement");

  // Credentials are supplied as a separate parameter to the provider adapter;
  // the URL template and diagnostics never contain the secret.
  sls::ProviderSelection selection;
  sls::HandoffRecord credential_record;
  unsigned keyed_requests=0;
  const auto cred_fetch=[&](const std::string& provider,const std::string& url,
                             std::uint32_t app,std::string_view credential,
                             std::string* bytes,std::string*) {
    Check(app==400 && url.find("fixture-secret")==std::string::npos,
          "credential not embedded in provider URL");
    if(provider!="morrenus")return false;
    ++keyed_requests;
    Check(credential=="fixture-secret","key passed only to keyed provider");
    *bytes=valid;return true;
  };
  auto with_key=sls::ResolveCredentialHandoff(400,selection,
    [](std::string_view id,std::string* value) {
      if(id!="moapikey")return false;
      *value="fixture-secret";return true;
    },cred_fetch,entitled,[&](const sls::HandoffRecord& x){credential_record=x;});
  Check(with_key.status=="matched" && credential_record.provider_id=="morrenus" &&
        keyed_requests==1,"credential-aware provider reaches handoff");
  Check(credential_record.depots[0].manifest_gid==2222,
        "credential provider cannot substitute Steam manifest GID");
  keyed_requests=0;
  auto without_key=sls::ResolveCredentialHandoff(400,selection,{},cred_fetch,entitled);
  Check(without_key.status=="unavailable" && keyed_requests==0,
        "missing credential skips keyed transport");
  Check(without_key.resolution.attempts.back().status=="missing_key",
        "missing credentials reported without their values");
  auto canceled=sls::ResolveInstallHandoff(400,sources,
    [](const std::string&,std::string*,std::string*){return false;},entitled,record);
  Check(canceled.status=="unavailable" && canceled.depots[0].manifest_gid==2222,
        "failed transport does not mutate the authorized download plan");
  std::cout<<"PASS: SLS->Playden handoff, fallback, credential boundary and depot authorization\n";
}
