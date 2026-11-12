#include "sls/manifest_metadata.h"
#include <cassert>
#include <string>
int main() {
  using namespace playden::sls;
  ManifestMetadata out;
  std::string error;
  assert(ParseManifestMetadata(480, "addappid(100)\nsetManifestid(100, \"123456789012345\")\n", &out, &error));
  assert(out.app_id == 480 && out.depots.size() == 1);
  assert(out.depots[0].depot_id == 100 && out.depots[0].manifest_gid == 123456789012345ULL);
  assert(ParseManifestMetadata(480,
    R"(addappid(100)
setManifestid(100, "123456789", 4096)
)", &out, &error));
  assert(out.depots.size()==1 && out.depots[0].declared_manifest_bytes==4096);
  assert(!ParseManifestMetadata(480,
  assert(!ParseManifestMetadata(480,
    R"(addappid(100)
setManifestid(100, "123456789", 1048577)
)", &out, &error));
  assert(!ParseManifestMetadata(480, "addappid(-1)", &out, &error));
}
