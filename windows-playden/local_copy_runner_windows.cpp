// Windows prototype boundary: the PS5 local-copy runner uses descriptor walks,
// root-owned drop folders and console title relocation. Never copy or delete
// game content until a native Windows installer has its own durable journal.
#include "stores/local/copy_runner.h"
#include <utility>
namespace playden::stores::local {
namespace {
constexpr const char* kUnavailable=
    "Local game copying requires a Windows-native installer and is not implemented yet.";
}
LocalCopyRunner::LocalCopyRunner(ImportStore& store,LocalCopyOptions options)
    : store_(store),options_(std::move(options)) {}
void LocalCopyRunner::SetChanged(std::function<void()> changed) {
  std::lock_guard<std::mutex> lock(changed_mutex_);
  changed_=std::move(changed);
}
std::string LocalCopyRunner::Enqueue(installs::InstallService&,const ImportEntry&,
    const std::string&,const std::string&,std::uint32_t) { return kUnavailable; }
std::string LocalCopyRunner::Move(installs::InstallService&,const ImportEntry&,
    const std::string&,std::uint32_t) { return kUnavailable; }
LocalCopyRunner::Result LocalCopyRunner::Install(const GameId&,const std::string&,
    const std::atomic<bool>&,
    const std::function<void(std::uint64_t,std::uint64_t,std::uint64_t)>&,
    const std::function<void(installs::InstallStep)>&) {
  Result result;
  result.failure=kUnavailable;
  return result;
}
std::string LocalCopyRunner::Remove(const std::string&) { return kUnavailable; }
void LocalCopyRunner::Forget(const GameId&) {}
} // namespace playden::stores::local
