// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "homebrew/simulation.hpp"
#include "platform/ps5/title_registry.h"
#include <memory>

namespace playden::homebrew {
// Host-only service double. No sockets, console SDK calls, Wine or process launch.
// Owns a fresh temporary directory; never maps virtual paths onto the real /data.
class MockConsole {
public:
    explicit MockConsole(const std::filesystem::path& steam_fixture);
    ~MockConsole();
    MockConsole(const MockConsole&) = delete;
    MockConsole& operator=(const MockConsole&) = delete;
    std::filesystem::path Path(const std::string& virtual_path) const;
    const std::filesystem::path& root() const { return root_; }
    void Attach(Simulation& model);
    void Tick(Simulation& model);
    bool Install(GameId id, std::string* reason);
    bool Prepare(GameId id, std::string* reason);
    bool ValidateLaunch(GameId id, std::string* reason) const;
    // Fault injection is local to this double, never a production console setting.
    bool registration_available = true;
    bool registration_pending = false;
    bool title_collision = false;
    std::uint64_t free_bytes = 8ULL * 1024 * 1024 * 1024;
private:
    struct Fixture { GameSummary game; std::string executable; std::string payload; };
    std::filesystem::path root_;
    std::vector<Fixture> fixtures_;
    std::vector<LiveInstalledGame> installed_;
    std::map<GameId, LiveRuntimeGame> runtime_;
    std::map<std::string, bool> titles_;
    std::unique_ptr<ps5::TitleRegistry> registry_;
    const Fixture* Find(GameId id) const;
    std::string Title(GameId id) const;
    std::optional<std::string> Transport(const std::string& route, const std::string& body);
    void Publish(Simulation& model);
};
} // namespace playden::homebrew
