// Windows provider-to-authorized-installer integration fixture. No network access.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sls/install_metadata.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
using namespace playden;
static void require(bool yes, const char* message) {
  if (!yes) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
  steam::DepotInfo one{}, two{};
  one.id=410; one.manifest_gid=1111;
  two.id=420; two.manifest_gid=2222;
  const std::vector<steam::DepotInfo> granted{two, one};
  const std::vector<sls::Source> sources{
    {"primary","Primary","https://primary.example/{appid}.lua",true},
    {"backup","Backup","https://backup.example/{appid}.lua",true},
    {"disabled","Disabled","https://disabled.example/{appid}.lua",false}
  };
  std::vector<std::string> calls;
  const auto resolution = sls::ResolveInstallMetadata(400,sources,
    [&](const std::string& url,std::string* text,std::string*) {
      calls.push_back(url);
      if (url.find("primary.example")!=std::string::npos) return false;
      if (url.find("backup.example")==std::string::npos) return false;
      *text = "addappid(410)\nsetManifestid(410, \"8888\")\n"
              "addappid(420)\nsetManifestid(420, \"9999\")\n";
      return true;
    },granted);
  require(resolution.status=="matched","secondary provider used after primary failure");
  require(calls.size()==2,"only ordered enabled providers fetched");
  require(calls[0].find("primary.example")!=std::string::npos,"primary tried first");
  require(calls[1].find("backup.example")!=std::string::npos,"backup used next");
  require(resolution.depots.size()==2 &&
          resolution.depots[0].id==420 && resolution.depots[0].manifest_gid==2222 &&
          resolution.depots[1].id==410 && resolution.depots[1].manifest_gid==1111,
          "Steam authorized order and GIDs retained");
  int disabled_calls=0;
  const auto empty=sls::ResolveInstallMetadata(400,{},
      [&](const std::string&,std::string*,std::string*) {++disabled_calls;return false;},granted);
  require(disabled_calls==0 && empty.status=="disabled","offline mode makes no provider requests");
  // Third-party manifests must never manufacture Steam entitlement.
  const std::vector<steam::DepotInfo> no_grants{};
  const auto unowned=sls::ResolveInstallMetadata(400,sources,
    [&](const std::string& url,std::string* text,std::string*) {
      if(url.find("backup.example")==std::string::npos)return false;
      *text="addappid(410)\\nsetManifestid(410, \\"9999\\")\\n";
      return true;
    },no_grants);
  require(unowned.depots.empty(),"unowned game may not generate authorized depots");
  // A provider can also advertise a depot Steam has not authorized.
  const auto injected=sls::ResolveInstallMetadata(400,sources,
    [&](const std::string& url,std::string* text,std::string*) {
      if(url.find("backup.example")==std::string::npos)return false;
      *text="addappid(410)\\nsetManifestid(410, \\"9999\\")\\n"
            "addappid(999)\\nsetManifestid(999, \\"3333\\")\\n";
      return true;
    },granted);
  require(injected.depots.size()==1 && injected.depots[0].id==410 &&
          injected.depots[0].manifest_gid==1111,
          "unentitled depot excluded; native Steam manifest retained");
  std::cout<<"PASS: provider fallback, disabled isolation and authorized plan\n";
}
