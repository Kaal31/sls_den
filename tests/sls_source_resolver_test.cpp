#include "sls/source_resolver.h"
#include "sls/provider_discovery.h"
#include <cassert>
#include <algorithm>
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
    *payload = R"(addappid(480)
setManifestid(480, "123456789")
)";
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
  // Provider discovery is transport-independent, so fixture tests exercise the
  // same selection, credential boundary and fallback logic on Windows and PS5.
  ProviderSelection selection;
  auto defaults = ConfiguredSources(selection);
  assert(defaults.size() == 4); // keyed Morrenus is not sent unauthenticated
  assert(std::none_of(defaults.begin(), defaults.end(), [](const Source& s) {
    return s.id == "morrenus";
  }));

  // A disabled source must never contact the transport.
  assert(selection.toggle(0));
  int calls = 0;
  auto discovered = DiscoverManifestWithCredentials(
    480, selection,
    [](std::string_view key, std::string* secret) {
      if (key != "moapikey") return false;
      *secret = "fixture-key";
      return true;
    },
    [&](const std::string& provider, const std::string&, std::uint32_t app,
        std::string_view credential, std::string* payload, std::string*) {
      ++calls;
      assert(app == 480 && provider != "ryuu");
      if (provider == "morrenus") {
        assert(credential == "fixture-key");
    *payload = R"(addappid(480)
setManifestid(480, "123456789")
)";
        return true;
      }
      assert(credential.empty());
      return false;
    });
  assert(discovered.ok && discovered.source_id == "morrenus");
  assert(discovered.metadata.depots.size() == 1);
  assert(calls == 4);
  assert(!discovered.attempts.empty() && discovered.attempts.front().status == "disabled");

  // Missing credentials block the keyed source before the fetch callback.
  int unkeyed_calls = 0;
  auto missing = DiscoverManifestWithCredentials(
    480, selection, {},
    [&](const std::string& provider, const std::string&, std::uint32_t,
        std::string_view, std::string*, std::string*) {
      assert(provider != "morrenus");
      ++unkeyed_calls;
      return false;
    });
  assert(!missing.ok && unkeyed_calls == 3);
  assert(missing.attempts.back().status == "missing_key");
  // ZIPs are rejected until a bounded, CRC-checking host decoder is supplied.
  auto zip_payload=[](const std::string&,const std::string&,std::uint32_t,
                      std::string_view,std::string* payload,std::string*) {
    *payload=std::string("PK\\x03\\x04",4)+"fixture-zip";
    return true;
  };
  auto blocked=DiscoverManifestWithCredentials(480,selection,
    [](std::string_view,std::string* out){*out="fixture-key";return true;},zip_payload);
  assert(!blocked.ok && blocked.attempts.back().status=="invalid_archive");
  auto decoded=DiscoverManifestWithCredentials(480,selection,
    [](std::string_view,std::string* out){*out="fixture-key";return true;},zip_payload,
    [](std::string_view bytes,std::uint32_t app,std::string* manifest,std::string*) {
      assert(bytes.size()>4 && app==480);
      *manifest=R"(addappid(480)
setManifestid(480, "123456789")
)";
      return true;
    });
  assert(decoded.ok && decoded.source_id=="sushi" && decoded.metadata.depots.size()==1);

}
