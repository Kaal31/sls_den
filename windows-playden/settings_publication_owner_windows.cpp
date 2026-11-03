// Windows port boundary: PS5 OWNER settings publication must never be armed,
// committed, or resumed on a host without native title registration.
#include "runtime/settings_publication_owner.h"
#include <utility>
namespace playden::runtime {
namespace {
PackageActionResult Refuse() {
  return {PackageActionState::kFailed,"windows-owner-unsupported",
          "PS5 publication OWNER journal is unavailable in Windows desktop mode."};
}
}
SettingsPublicationOwner::SettingsPublicationOwner(InstallRecordStore& records,
    PackageInstaller& package,NativeCoordinator& native,bool unowned,
    std::filesystem::path prefix)
    : records_(records),package_(package),native_(native),
      unowned_game_roots_(unowned),host_namespace_prefix_(std::move(prefix)) {}
PackageActionResult SettingsPublicationOwner::Request(const SettingsPublicationRequest&,
    const std::filesystem::path&) { return Refuse(); }
PackageActionResult SettingsPublicationOwner::ResumeRequested(const RuntimeInstall&,
    const std::filesystem::path&,const std::filesystem::path&) { return Refuse(); }
PackageActionResult SettingsPublicationOwner::DiscardRequested(const RuntimeInstall&,
    const std::filesystem::path&,const std::filesystem::path&) { return Refuse(); }
PackageActionResult SettingsPublicationOwner::InitializeControl(const RuntimeInstall&,
    const std::string&,const std::filesystem::path&,
    const std::function<bool(PublicationControlStep)>&) { return Refuse(); }
Result<SettingsPublicationOperation> SettingsPublicationOwner::Prepare(const RuntimeInstall&,
    const SettingsLaunchPlan&,const std::string&,const std::string&,std::int64_t,
    const std::filesystem::path&) {
  return Result<SettingsPublicationOperation>::Failure(StorageError::kOpenFailed,
      "PS5 OWNER operation preparation is unavailable on Windows.");
}
PackageActionResult SettingsPublicationOwner::Resume(const RuntimeInstall&,
    const std::filesystem::path&) { return Refuse(); }
PublicationDependencies SettingsPublicationOwner::Dependencies(
    const std::filesystem::path&,std::optional<GameRootMove>) {
  return {};
}
PackageActionResult SettingsPublicationOwner::Block(const SettingsPublicationOperation&) {
  return Refuse();
}
PackageActionResult SettingsPublicationOwner::Reconcile(const SettingsPublicationOperation&) {
  return Refuse();
}
PackageActionResult SettingsPublicationOwner::AuthorizeRetirement(
    const SettingsPublicationOperation&,const SettingsPublicationOperation&) {
  return Refuse();
}
PackageActionResult SettingsPublicationOwner::Persist(const RuntimeInstall&) {
  return Refuse();
}
} // namespace playden::runtime
