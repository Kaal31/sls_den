// Copyright (C) 2026 Playden contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "homebrew/mock_console.hpp"
#include "homebrew/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <unistd.h>

namespace playden::homebrew {
namespace fs = std::filesystem;
namespace {
std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing fixture file: " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << bytes;
    output.close();
    if (!output) throw std::runtime_error("mock filesystem write failed");
}
std::string Text(const json::Value& value, const char* key) {
    const auto* field = value.get(key);
    if (!field || !field->string()) throw std::runtime_error("invalid Steam fixture string");
    return *field->string();
}
bool Fail(std::string* reason, const char* text) { if (reason) *reason = text; return false; }
}
MockConsole::MockConsole(const fs::path& fixture) {
    sources = {{"offline", "Unavailable fixture", "https://offline.invalid/{appid}.lua"},
               {"corrupt", "Corrupt fixture", "https://corrupt.invalid/{appid}.lua"},
               {"valid", "Valid fixture", "https://valid.invalid/{appid}.lua"}};
    source_responses["offline"] = std::nullopt;
    source_responses["corrupt"] = Read(fixture.parent_path() / "corrupt.lua");
    source_responses["valid"] = Read(fixture.parent_path() / "valid.lua");
    steam::DepotInfo entitled;
    entitled.id = 221;
    entitled.manifest_gid = 123456789;
    entitled.os_list = "windows";
    authorized_depots.push_back(entitled);
    const auto parsed = json::Parser(Read(fixture)).parse();
    const auto* response = parsed ? parsed->get("response") : nullptr;
    const auto* games = response ? response->get("games") : nullptr;
    if (!games || !games->array() || games->array()->empty()) throw std::runtime_error("invalid Steam owned-games fixture");
    for (const auto& item : *games->array()) {
        const auto* app = item.get("appid");
        const auto number = app ? app->number() : std::nullopt;
        if (!number || *number < 1 || *number > 999 || std::floor(*number) != *number)
            throw std::runtime_error("fixture app ID outside reserved mock range 1..999");
        Fixture entry;
        entry.game.id = GameId::Steam(static_cast<std::uint32_t>(*number));
        entry.game.title = Text(item, "name");
        entry.game.description = "Offline mock console fixture. No hardware runtime tested.";
        entry.executable = Text(item, "executable");
        const fs::path executable(entry.executable);
        if (executable.empty() || executable.is_absolute() || entry.executable.find('\\') != std::string::npos ||
            entry.executable.find(':') != std::string::npos ||
            std::any_of(executable.begin(), executable.end(), [](const auto& p) { return p == ".." || p == "."; }))
            throw std::runtime_error("unsafe fixture executable");
        entry.payload = Text(item, "payload");
        if (entry.payload.empty() || Find(entry.game.id)) throw std::runtime_error("empty or duplicate fixture");
        fixtures_.push_back(std::move(entry));
    }
    auto pattern = (fs::canonical(fs::temp_directory_path()) / "playden-mock-ps5-XXXXXX").string();
    if (!::mkdtemp(pattern.data())) throw std::runtime_error("cannot create temporary mock console");
    root_ = pattern;
    try {
        Write(Path("/data/playden/kits/mock/template/system.reg"), "MOCK BOTTLE ONLY\n");
        fs::create_directories(Path("/data/homebrew"));
        fs::create_directories(Path("/data/playden-games"));
        registry_ = std::make_unique<ps5::TitleRegistry>([this](const auto& route, const auto& body) {
            return Transport(route, body);
        });
    } catch (...) { fs::remove_all(root_); throw; }
}
MockConsole::~MockConsole() { std::error_code error; fs::remove_all(root_, error); }
fs::path MockConsole::Path(const std::string& virtual_path) const {
    const fs::path path(virtual_path);
    if (!path.is_absolute() || virtual_path.find('\\') != std::string::npos || virtual_path.find(':') != std::string::npos)
        throw std::runtime_error("invalid virtual console path");
    fs::path result = root_;
    for (const auto& component : path.relative_path()) {
        if (component == ".." || component == ".") throw std::runtime_error("virtual path traversal");
        result /= component;
        if (fs::is_symlink(fs::symlink_status(result))) throw std::runtime_error("mock console rejects symlinks");
    }
    return result;
}
const MockConsole::Fixture* MockConsole::Find(GameId id) const {
    for (const auto& fixture : fixtures_) if (fixture.game.id == id) return &fixture;
    return nullptr;
}
std::string MockConsole::Title(GameId id) const { return "FAKE78" + std::to_string(1000 + *id.steam_app()).substr(1); }
std::optional<std::string> MockConsole::Transport(const std::string& route, const std::string& body) {
    if (!registration_available) return std::nullopt;
    if (route == "/api/v1/games") {
        if (!registration_pending)
            for (auto& [title, ready] : titles_) ready = true;
        std::string result = "{\"status\":0,\"games\":[";
        bool first = true;
        for (const auto& [title, ready] : titles_) {
            if (!first) result += ',';
            first = false;
            result += "{\"title_id\":\"" + title + "\",\"path\":\"" +
                (title_collision ? "/foreign/" : "/data/homebrew/") + title +
                "\",\"managed\":true,\"installed\":" + (ready ? "true" : "false") +
                ",\"source_available\":true,\"source_type\":\"folder\"}";
        }
        return result + "]}";
    }
    if (route == "/api/v1/manual/add") {
        for (const auto& fixture : fixtures_) {
            const auto title = Title(fixture.game.id);
            if (body.find("/data/homebrew/" + title + "\"") != std::string::npos) titles_[title] = false;
        }
    } else if (route == "/api/v1/scan" && !registration_pending) {
        for (auto& [title, ready] : titles_) ready = true;
    }
    return "{\"status\":0}";
}
bool MockConsole::Install(GameId id, std::string* reason) {
    const auto* fixture = Find(id);
    if (!fixture) return Fail(reason, "Steam fixture does not own this app");
    auto ordered = sources;
    if (!ordered.empty()) std::rotate(ordered.begin(), ordered.begin() + selected_source % ordered.size(), ordered.end());
    last_resolution = sls::ResolveManifest(*id.steam_app(), ordered,
        [this, id](const std::string& url, std::string* payload, std::string*) {
            for (const auto& source : sources) {
                if (sls::SourceUrl(source, *id.steam_app()) != url) continue;
                const auto found = source_responses.find(source.id);
                if (found == source_responses.end() || !found->second) return false;
                *payload = *found->second;
                return true;
            }
            return false;
        });
    last_plan = {};
    if (!last_resolution.ok) return Fail(reason, "No valid fixture manifest source");
    last_plan = sls::PlanAuthorizedDepots(last_resolution.metadata, authorized_depots);
    if (last_plan.selected.empty() || !last_plan.unrecognized.empty())
        return Fail(reason, "Fixture depot is not authorized by mock Steam");
    if (std::any_of(last_plan.selected.begin(), last_plan.selected.end(), [](const auto& depot) {
            return !depot.manifest_gid || *depot.manifest_gid == 0;
        })) return Fail(reason, "Mock Steam supplied no authorized manifest GID");
    if (fixture->payload.size() > free_bytes) return Fail(reason, "Mock console storage is full");
    try {
        const auto base = "/data/playden-games/" + std::to_string(*id.steam_app());
        const auto partial = Path(base + "/download.partial");
        std::string prefix;
        if (fs::exists(partial)) prefix = Read(partial);
        if (fixture->payload.compare(0, prefix.size(), prefix) != 0)
            return Fail(reason, "Corrupt interrupted fixture download");
        resumed_bytes = prefix.size();
        if (interrupt_after_bytes) {
            Write(partial, fixture->payload.substr(0, std::min(*interrupt_after_bytes, fixture->payload.size())));
            return Fail(reason, "Mock installation interrupted; resume to continue");
        }
        Write(Path(base + "/" + fixture->executable), prefix + fixture->payload.substr(prefix.size()));
        if (Read(Path(base + "/" + fixture->executable)) != fixture->payload)
            return Fail(reason, "Fixture installation verification failed");
        if (std::none_of(installed_.begin(), installed_.end(), [id](const auto& item) { return item.game_id == id; })) {
            installed_.push_back({id, fixture->payload.size(), "internal", true});
            free_bytes -= fixture->payload.size();
        }
        fs::remove(partial);
        return true;
    } catch (const std::exception& error) { if (reason) *reason = error.what(); return false; }
}
bool MockConsole::Prepare(GameId id, std::string* reason) {
    const auto* fixture = Find(id);
    if (!fixture) return Fail(reason, "Unknown mock app");
    try {
        const auto number = std::to_string(*id.steam_app());
        if (Read(Path("/data/playden-games/" + number + "/" + fixture->executable)) != fixture->payload)
            return Fail(reason, "Installed fixture bytes changed");
        const auto bottle = Read(Path("/data/playden/kits/mock/template/system.reg"));
        const auto title = Title(id);
        if (title_collision) titles_.try_emplace(title, true);
        const auto before = registry_->Query(title);
        if (before.state == ps5::RegistrationState::kCollision || before.state == ps5::RegistrationState::kUnavailable)
            return Fail(reason, "Mock title registration unavailable or collision");
        Write(Path("/data/playden/games/" + number + "/system.reg"), bottle);
        Write(Path("/data/homebrew/" + title + "/sce_sys/param.json"),
              "{\"titleId\":\"" + title + "\",\"mock\":true}");
        Write(Path("/data/homebrew/" + title + "/launch-preparation.txt"), fixture->executable);
        if (!registry_->Register(title).ok) return Fail(reason, "Mock registration refused");
        if (registry_->Query(title).state != ps5::RegistrationState::kReady)
            return Fail(reason, "Mock registration pending");
        return ValidateLaunch(id, reason);
    } catch (const std::exception& error) { if (reason) *reason = error.what(); return false; }
}
bool MockConsole::ValidateLaunch(GameId id, std::string* reason) const {
    const auto* fixture = Find(id);
    if (!fixture) return Fail(reason, "Unknown mock app");
    try {
        const auto number = std::to_string(*id.steam_app());
        const auto title = Title(id);
        if (registry_->Query(title).state != ps5::RegistrationState::kReady)
            return Fail(reason, "Mock title is not registered");
        if (Read(Path("/data/playden-games/" + number + "/" + fixture->executable)) != fixture->payload ||
            Read(Path("/data/homebrew/" + title + "/launch-preparation.txt")) != fixture->executable ||
            Read(Path("/data/playden/games/" + number + "/system.reg")) !=
            Read(Path("/data/playden/kits/mock/template/system.reg")))
            return Fail(reason, "Mock launch-preparation artifacts differ");
        const auto param = json::Parser(Read(Path("/data/homebrew/" + title + "/sce_sys/param.json"))).parse();
        if (!param || Text(*param, "titleId") != title) return Fail(reason, "Mock title identity differs");
        if (reason) *reason = "Mock launch preparation validated; no hardware runtime tested";
        return true;
    } catch (const std::exception& error) { if (reason) *reason = error.what(); return false; }
}
void MockConsole::Attach(Simulation& model) {
    model.UpdateAccount({AuthState::SignedIn, "76561198000000001", "Offline mock Steam", {}, {}, false});
    LibraryUpdate library;
    for (const auto& fixture : fixtures_) library.games.push_back(fixture.game);
    model.UpdateLibrary(std::move(library));
    for (const auto& fixture : fixtures_)
        model.UpdateAppDetails(fixture.game.id, fixture.payload.size(), fixture.payload.size(), true);
    Publish(model);
}
void MockConsole::Publish(Simulation& model) {
    model.UpdateVolumes({{"internal", "Mock PS5 temporary storage", true, 8ULL * 1024 * 1024 * 1024, free_bytes, true}}, "internal");
    model.UpdateInstalls(jobs_, installed_);
    std::vector<LiveRuntimeGame> games;
    for (const auto& fixture : fixtures_) {
        auto& game = runtime_[fixture.game.id];
        game.game_id = fixture.game.id;
        game.can_prepare = true;
        games.push_back(game);
    }
    model.UpdateRuntime(RuntimeState::Ready, games);
}
void MockConsole::Tick(Simulation& model) {
    selected_source = static_cast<std::size_t>(model.state().mock_source);
    for (const auto& request : model.TakeInstallRequests()) {
        std::string reason;
        if (request.kind == InstallRequestKind::Install || request.kind == InstallRequestKind::Resume) {
            jobs_.clear();
            if (Install(request.game_id, &reason)) model.RequestRuntimePrepare(request.game_id);
            else {
                const auto* fixture = Find(request.game_id);
                const auto total = fixture ? fixture->payload.size() : 0;
                const auto done = interrupt_after_bytes ? std::min(*interrupt_after_bytes, total) : 0;
                jobs_.push_back({request.game_id, interrupt_after_bytes ? JobState::Interrupted : JobState::Failed,
                                 done, total, reason, "internal"});
            }
        }
    }
    if (!last_resolution.attempts.empty()) {
        std::string status;
        for (const auto& attempt : last_resolution.attempts) {
            if (!status.empty()) status += "; ";
            status += attempt.source_id + ": " + attempt.status;
        }
        model.SetMockSourceStatus(status);
    }
    for (const auto id : model.TakeRuntimePrepareRequests()) {
        std::string reason;
        const bool ready = Prepare(id, &reason);
        auto& game = runtime_[id];
        game.game_id = id;
        game.status = ready ? "Ready" : "Preparation refused";
        game.detail = reason;
        game.ready = game.prepared = game.has_title = game.can_play = ready;
        game.activity = TitleActivity::Stopped;
    }
    for (const auto id : model.TakeRuntimePlayRequests()) {
        std::string reason;
        const bool ready = ValidateLaunch(id, &reason);
        auto& game = runtime_[id];
        game.play_status = ready ? "Mock validation passed" : "Mock validation refused";
        game.play_detail = reason;
        game.activity = TitleActivity::Stopped;
        if (!ready) model.NotifyPlayStopped(id, reason);
    }
    Publish(model);
}
} // namespace playden::homebrew
