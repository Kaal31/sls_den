// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "homebrew/simulation.hpp"
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
}
