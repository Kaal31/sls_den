// Windows full-host boundary for PS5-only game settings publication.
// This DOES NOT grant readiness, publish titles, mutate Wine bottle files,
// or infer that console package ownership has been verified.
#include "runtime/settings_publication.h"
namespace playden::runtime {
namespace {
PackageActionResult Unsupported() {
  return {PackageActionState::kFailed,"windows-publication-unsupported",
          "PS5 title settings publication is not supported by Windows desktop Playden."};
}
Result<SettingsPublicationOperation> NoCapture() {
  return Result<SettingsPublicationOperation>::Failure(StorageError::kOpenFailed,
          "PS5 settings publication capture is not supported on Windows.");
}
}
std::optional<GameRootMove> GameRootMoveOf(const RuntimeInstall&) { return std::nullopt; }
PackageActionResult VerifyMovedPublication(const RuntimeInstall&,const GameRootMove&,
    const std::string&,const std::string&,const std::filesystem::path&,bool) {
  return Unsupported();
}
PackageActionResult VerifyCommittedPublicationReceipt(const RuntimeInstall&,
    const std::filesystem::path&) { return Unsupported(); }
std::optional<bool> PublishedGameRootUnowned(const RuntimeInstall&,
    const std::filesystem::path&) { return std::nullopt; }
std::optional<std::string> PublishedProfileDigest(const RuntimeInstall&,
    const std::filesystem::path&) { return std::nullopt; }
PackageActionResult PublishSettingsOperation(const SettingsPublicationOperation&,
    const PublicationPaths&,PublicationDependencies&) { return Unsupported(); }
PackageActionResult VerifySettingsPublicationFiles(const SettingsPublicationOperation&,
    const PublicationPaths&,int,const std::filesystem::path&) { return Unsupported(); }
PackageActionResult ApplyPendingPublicationRegistry(const SettingsPublicationOperation&,
    int,const std::filesystem::path&) { return Unsupported(); }
PackageActionResult ApplyPendingPublicationDxvk(const SettingsPublicationOperation&,
    int,const std::filesystem::path&) { return Unsupported(); }
PackageActionResult ApplyPendingPublicationFiles(const SettingsPublicationOperation&,
    const PublicationPaths&,int,const std::filesystem::path&,
    const std::function<bool(const std::filesystem::path&)>&) { return Unsupported(); }
PackageActionResult PreflightSettingsPublication(const SettingsPublicationOperation&,
    const PublicationPaths&,int,const std::filesystem::path&,const GameRootMove*) {
  return Unsupported();
}
Result<SettingsPublicationOperation> CaptureSettingsPublicationOperation(
    const RuntimeInstall&,const SettingsLaunchPlan&,const std::string&,
    const std::string&,std::int64_t,int,const std::filesystem::path&,
    const GameRootMove*,bool) { return NoCapture(); }
} // namespace playden::runtime
