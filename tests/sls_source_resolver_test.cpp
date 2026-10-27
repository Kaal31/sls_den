#include "sls/source_resolver.h"
#include <cassert>
#include <string>
int main() {
  using namespace playden::sls;
  std::vector<Source> sources = {
    {"first", "First", "https://first.example/{appid}.lua"},
    {"second", "Second", "https://second.example/{appid}.lua"},
    {"disabled", "Disabled", "https://disabled.example/{appid}.lua", false}
  };
  int requests = 0;
  auto fetch = [&](const std::string& url, std::string* payload, std::string* error) {
    ++requests;
    if (url.find("first.example") != std::string::npos) {
      *payload = "invalid";
      return true;
    }
    *payload = "addappid(100)\nsetManifestid(100, \"123456789\")\n";
    return true;
  };
  auto r = ResolveManifest(480, sources, fetch);
  assert(r.ok && r.source_id == "second" && requests == 2);
  assert(r.metadata.depots.size() == 1 && r.metadata.depots[0].manifest_gid == 123456789);
  assert(r.attempts.size() == 2 && r.attempts[0].status == "invalid_manifest");
  auto failed = ResolveManifest(480, sources, [&](const std::string&, std::string*, std::string*) {
    return false;
  });
  assert(!failed.ok && failed.attempts.size() == 2);
  auto huge = ResolveManifest(480, sources, [&](const std::string&, std::string* payload, std::string*) {
    payload->assign(1024 * 1024 + 1, 'X');
    return true;
  });
  assert(!huge.ok && huge.attempts[0].status == "response_too_large");
}
