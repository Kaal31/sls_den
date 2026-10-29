// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sls/install_metadata.h"
#include <cstdlib>
#include <iostream>
using namespace playden;
void Check(bool ok, const char* reason) { if (!ok) { std::cerr << reason << '\n'; std::exit(1); } }
int main() {
  steam::DepotInfo base, extra;
  base.id = 221; base.manifest_gid = 123;
  extra.id = 222; extra.manifest_gid = 456;
  const std::vector<steam::DepotInfo> authorized{extra, base};
  const std::vector<sls::Source> sources{{"fixture", "Fixture", "https://fixture.invalid/{appid}.lua"}};
  auto run = [&](const std::string& payload) {
    return sls::ResolveInstallMetadata(220, sources,
      [&](const std::string&, std::string* text, std::string*) { *text = payload; return true; }, authorized);
  };
  const auto matched = run("addappid(221)\nsetManifestid(221,\"999\")\naddappid(222)\nsetManifestid(222,\"888\")\n");
  Check(matched.status == "matched" && matched.depots.size() == 2, "complete metadata matches Steam plan");
  Check(matched.depots[0].id == 222 && matched.depots[0].manifest_gid == 456 &&
        matched.depots[1].id == 221 && matched.depots[1].manifest_gid == 123,
        "Steam order and authorized GIDs are preserved");
  const auto incomplete = run("addappid(221)\nsetManifestid(221,\"999\")\n");
  Check(incomplete.status == "incomplete" && incomplete.depots.size() == 2, "metadata cannot omit required depots");
  const auto unowned = run("addappid(999)\nsetManifestid(999,\"777\")\n");
  Check(unowned.status == "unowned-depot" && unowned.depots.size() == 2 && unowned.depots[0].id == 222,
        "metadata cannot add unowned depots");
  Check(run("invalid source").status == "unavailable", "invalid source falls back to Steam");
  bool fetched = false;
  const auto disabled = sls::ResolveInstallMetadata(220, {}, [&](const auto&, auto*, auto*) {
    fetched = true; return false;
  }, authorized);
  Check(disabled.status == "disabled" && !fetched && disabled.depots.size() == 2, "empty configuration makes no requests");
  const auto unavailable = sls::ResolveInstallMetadata(220, sources, [](const auto&, auto*, auto*) { return false; }, authorized);
  Check(unavailable.status == "unavailable" && unavailable.depots.size() == 2, "network failure retains Steam plan");
  std::cout << "PASS: production metadata decision preserves complete authorized Steam installation\n";
}
