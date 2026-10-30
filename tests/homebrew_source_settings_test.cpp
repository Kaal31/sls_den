// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "homebrew/simulation.hpp"
#include "sls/source_list.h"
#include <cstdlib>
#include <iostream>
using namespace playden::homebrew;
void Check(bool ok, const char* reason) { if (!ok) { std::cerr << reason << '\n'; std::exit(1); } }
int main() {
  Simulation model({}, {}, ModelMode::Live);
  Check(model.action("focus:startup-skip") && model.action("cross"), "skip sign-in");
  for (int n = 0; n < 5 && model.state().navigation.route != Route::Settings; ++n) model.action("r1");
  Check(model.action("focus:settings-manifest-source") && model.action("cross"), "native settings opens source editor");
  model.text("http://invalid/{appid}");
  model.action("done");
  Check(model.state().editor.active && model.ExportPreferences().sls_source_url.empty(), "invalid HTTP source is refused");
  model.action("clear");
  model.text("https://fixtures.invalid/{appid}.lua");
  model.action("done");
  Check(!model.state().editor.active && model.ExportPreferences().sls_source_url == "https://fixtures.invalid/{appid}.lua",
        "source edit becomes persistable preference");
  Check(model.PreferencesDirty(), "source changes participate in normal persistence tracking");
  model.action("focus:settings-manifest-source"); model.action("cross");
  model.action("clear"); model.action("done");
  Check(model.ExportPreferences().sls_source_url.empty(), "clear disables source requests");
  Check(model.action("focus:settings-sls-add") && model.action("cross"), "SLS Den provider editor opens");
  model.text("https://one.example/{appid}.lua");
  model.action("done");
  std::vector<playden::sls::Source> providers;
  Check(playden::sls::ParseSourceList(model.ExportPreferences().sls_source_list, &providers) &&
        providers.size() == 1 && providers[0].enabled, "provider is recorded in preferences");
  Check(model.action("focus:settings-sls-toggle-0") && model.action("cross"), "toggle provider");
  Check(playden::sls::ParseSourceList(model.ExportPreferences().sls_source_list, &providers) &&
        !providers[0].enabled, "provider can be disabled");
  Check(model.action("focus:settings-sls-remove-0") && model.action("cross"), "remove provider");
  Check(model.ExportPreferences().sls_source_list.empty(), "provider removal persisted in state");
}
