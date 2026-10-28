// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "homebrew/mock_console.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace playden::homebrew;
namespace fs = std::filesystem;
void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void Activate(Simulation& model, const std::string& key) {
    Check(model.action("focus:" + key), ("focus " + key).c_str());
    Check(model.action("cross"), ("activate " + key).c_str());
}
int main(int argc, char** argv) {
    Check(argc == 2, "fixture argument");
    const auto id = GameId::Steam(220);
    std::string reason;
    fs::path removed;
    {
        MockConsole console(argv[1]);
        removed = console.root();
        Check(fs::exists(removed), "fresh temporary PS5 filesystem");
        Check(!console.Prepare(id, &reason), "preparation refuses missing install");
        Check(!console.Install(GameId::Steam(221), &reason), "Steam fixture enforces ownership");
        console.free_bytes = 0;
        Check(!console.Install(id, &reason), "installation refuses full disk");
        console.free_bytes = 8ULL * 1024 * 1024 * 1024;
        Check(console.Install(id, &reason), "fixture installation");
        Check(console.last_resolution.ok && console.last_resolution.source_id == "valid" &&
              console.last_resolution.attempts.size() == 3, "unavailable and corrupt sources fall back to valid fixture");
        Check(console.last_plan.selected.size() == 1 &&
              console.last_plan.selected.front().manifest_gid == 123456789,
              "third-party manifest GID never replaces mock Steam authorized GID");
        Check(!console.ValidateLaunch(id, &reason), "unprepared title cannot launch");
        console.registration_available = false;
        Check(!console.Prepare(id, &reason), "unreachable registration fails closed");
        console.registration_available = true;
        console.title_collision = true;
        Check(!console.Prepare(id, &reason), "foreign registration collision refused");
        console.title_collision = false;
        Check(console.Prepare(id, &reason), "title and private bottle preparation");
        Check(console.ValidateLaunch(id, &reason), "launch preparation validates files and registration");
        Check(reason.find("no hardware runtime tested") != std::string::npos, "validation states hardware limit");
        Check(console.Prepare(id, &reason), "preparation retry is idempotent");
        std::ofstream(console.Path("/data/playden-games/220/bin/mock-game.exe")) << "corrupt";
        Check(!console.ValidateLaunch(id, &reason), "modified installation blocks launch preparation");
        Check(console.Install(id, &reason), "fixture can restore changed installation");
        fs::remove(console.Path("/data/homebrew/FAKE78220/sce_sys/param.json"));
        Check(!console.ValidateLaunch(id, &reason), "missing title metadata blocks validation");
        fs::remove(console.Path("/data/playden/kits/mock/template/system.reg"));
        Check(!console.Prepare(id, &reason), "missing runtime template blocks preparation");
        bool refused = false;
        try { console.Path("/data/../../escape"); } catch (...) { refused = true; }
        Check(refused, "path traversal rejected");
        fs::create_directory_symlink(fs::temp_directory_path(), console.root() / "outside");
        refused = false;
        try { console.Path("/outside/escape"); } catch (...) { refused = true; }
        Check(refused, "symlink escape rejected");
    }
    Check(!fs::exists(removed), "temporary filesystem cleaned up");
    {
        MockConsole console(argv[1]);
        console.authorized_depots.clear();
        Check(!console.Install(id, &reason), "unentitled depot denied even with valid source");
        Check(!fs::exists(console.Path("/data/playden-games/220/bin/mock-game.exe")), "denial writes no game content");
        console.source_responses["valid"] = "setManifestid(221, \"123\")";
        Check(!console.Install(id, &reason) && !console.last_resolution.ok, "corrupt manifest rejected end to end");
    }
    {
        MockConsole console(argv[1]);
        console.interrupt_after_bytes = 8;
        Check(!console.Install(id, &reason), "interrupted fixture install reports failure");
        Check(!console.Prepare(id, &reason), "partial installation cannot prepare title");
        Check(fs::file_size(console.Path("/data/playden-games/220/download.partial")) == 8, "partial progress is persisted");
        console.interrupt_after_bytes.reset();
        Check(console.Install(id, &reason) && console.resumed_bytes == 8, "interrupted install resumes verified prefix");
        Check(console.Prepare(id, &reason), "resumed install prepares normally");
    }
    {
        MockConsole console(argv[1]);
        Check(console.Install(id, &reason), "pending fixture install");
        console.registration_pending = true;
        Check(!console.Prepare(id, &reason) && reason == "Mock registration pending", "pending registration is not ready");
        Check(!console.ValidateLaunch(id, &reason), "pending registration blocks launch validation");
        console.registration_pending = false;
        Check(console.Prepare(id, &reason), "pending registration can become ready on retry");
    }
    {
        Simulation model({}, {}, ModelMode::MockConsole);
        MockConsole console(argv[1]);
        console.Attach(model);
        Check(model.state().mock_console && model.view().show_simulation_label, "desktop mock mode visibly labeled");
        Check(model.state().games.size() == 1, "Steam owned-games response populates real UI model");
        Check(model.action("details:220"), "open fixture in actual desktop details model");
        Activate(model, "details-mock-source");
        Check(model.state().mock_source == 1, "controller cycles source selection");
        Activate(model, "details-install");
        Activate(model, "install-confirm");
        console.Tick(model);
        Check(console.ValidateLaunch(id, &reason), "UI install request prepares mock console title");
        Check(model.state().runtime_games.at(id).ready, "prepared title projected into UI");
        Check(model.state().mock_source_status.find("invalid_manifest") != std::string::npos,
              "source failure and fallback are exposed to UI");
        Activate(model, "details-play");
        console.Tick(model);
        Check(!model.state().quit_requested, "mock Play never hands off to hardware");
        const auto& runtime = model.state().runtime_games.at(id);
        Check(runtime.activity == TitleActivity::Stopped && runtime.play_status == "Mock validation passed",
              "UI Play validates preparation without claiming a running game");
        Check(runtime.play_detail.find("no hardware runtime tested") != std::string::npos, "UI reports runtime limit");
    }
    std::cout << "PASS: mock filesystem, Steam fixture, registration, install, preparation, refusal cases and UI service flow; no hardware runtime tested\n";
}
