// Windows full-host boundary for PS5-native runtime preparation and title launch.
// A Windows Store/Steam catalogue preview must never claim that PS5 registration,
// ShadowMount, privileged Wine bottle publication, or launch succeeds.
#include "homebrew/runtime_host.hpp"
#include <utility>

namespace playden::homebrew {
namespace {
constexpr const char* kReason =
    "PS5 runtime title preparation and launch are unavailable in the Windows preview.";
}
RuntimeHost::RuntimeHost(RuntimeHostConfig config) : config_(std::move(config)) {
  snapshot_.status = RuntimeHostStatus::kUnavailable;
  snapshot_.reason = kReason;
  snapshot_.kit_available = false;
  snapshot_.kit_reason = kReason;
  shared_ = std::make_shared<const RuntimeSnapshot>(snapshot_);
  shared_revision_ = snapshot_.revision;
}
RuntimeHost::~RuntimeHost() { Shutdown(); }
bool RuntimeHost::Start() {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.status = RuntimeHostStatus::kUnavailable;
  snapshot_.reason = kReason;
  snapshot_.kit_available = false;
  snapshot_.kit_reason = kReason;
  ++snapshot_.revision;
  shared_.reset();
  started_ = true;
  return false;
}
void RuntimeHost::Shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  stopping_ = true;
  closing_ = true;
  snapshot_.status = RuntimeHostStatus::kStopped;
  ++snapshot_.revision;
  shared_.reset();
  wake_.notify_all();
  published_.notify_all();
}
RuntimeSnapshot RuntimeHost::Snapshot() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return snapshot_;
}
bool RuntimeHost::WaitForStartup() { return false; }
std::shared_ptr<const RuntimeSnapshot> RuntimeHost::Shared() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if(!shared_ || shared_revision_ != snapshot_.revision) {
    shared_ = std::make_shared<const RuntimeSnapshot>(snapshot_);
    shared_revision_ = snapshot_.revision;
  }
  return shared_;
}
void RuntimeHost::Prepare(RuntimePrepareRequest request) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.refusals[request.runtime_number] = kReason;
  ++snapshot_.prepares_handled;
  ++snapshot_.revision;
  shared_.reset();
}
bool RuntimeHost::PrepareNext(std::uint32_t) { return false; }
bool RuntimeHost::WithdrawPrepare(std::uint32_t) { return false; }
void RuntimeHost::HoldPreparations(bool hold) { prepares_held_ = hold; }
void RuntimeHost::DismissPrepared(std::uint32_t, std::int64_t) {}
void RuntimeHost::ClearPrepared() {}
void RuntimeHost::CheckActivity(std::uint32_t runtime_number) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.activity[runtime_number] = {runtime::NativeActivity::kUnknown, ++snapshot_.activity_serial};
  ++snapshot_.revision;
  shared_.reset();
}
runtime::NativeActivity RuntimeHost::ActivityNow(std::uint32_t, std::chrono::milliseconds) {
  return runtime::NativeActivity::kUnknown;
}
void RuntimeHost::CleanUpRequests() {}
void RuntimeHost::SetTitleNames(std::map<std::uint32_t, std::string>) {}
void RuntimeHost::Play(std::uint32_t runtime_number) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.play_refusals[runtime_number] = kReason;
  ++snapshot_.plays_handled;
  ++snapshot_.revision;
  shared_.reset();
}
void RuntimeHost::SetSteamAccount(std::string account_name,std::uint64_t steam_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  steam_account_name_ = std::move(account_name);
  steam_id_ = steam_id;
}
void RuntimeHost::Remove(std::uint32_t runtime_number,bool) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.removal_failures[runtime_number] = kReason;
  ++snapshot_.removals_handled;
  ++snapshot_.revision;
  shared_.reset();
}
void RuntimeHost::DeleteKeptBottle(std::uint32_t) {}
void RuntimeHost::ScanPrograms(std::uint32_t runtime_number,std::string) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.game_setups[runtime_number] = {};
  ++snapshot_.revision;
  shared_.reset();
}
void RuntimeHost::MeasureSaves() {}
void RuntimeHost::ReloadKit() {}
void RuntimeHost::Maintain(std::uint32_t runtime_number,RuntimeMaintenance action) {
  std::lock_guard<std::mutex> lock(mutex_);
  snapshot_.maintenance[runtime_number] = {action,false,false,kReason};
  ++snapshot_.maintenance_handled;
  ++snapshot_.revision;
  shared_.reset();
}
std::optional<std::uint32_t> RuntimeHost::RuntimeNumberFor(const GameId&,bool* busy) {
  if(busy)*busy=false;
  return std::nullopt;
}
RuntimeRelocationResult RuntimeHost::Relocate(RuntimeRelocation,std::chrono::milliseconds) {
  return {RuntimeRelocationResult::Outcome::kFailed,kReason,false};
}
void RuntimeHost::BeginClosing() {
  std::lock_guard<std::mutex> lock(mutex_);
  closing_ = true;
  published_.notify_all();
}
bool RuntimeHost::Handoff() { return false; }
} // namespace playden::homebrew
