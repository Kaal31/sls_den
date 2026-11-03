// Windows compilation boundary for PS5 package staging / publication.
// This adapter deliberately performs no disk mutations and grants no readiness.
// A real Windows installation service must use a separate verified NTFS backend.
#include "runtime/package_installer.h"
#include <utility>
namespace playden::runtime {
namespace {
PackageActionResult Unsupported() {
  return {PackageActionState::kFailed,"windows-package-unsupported",
          "PS5 package preparation and publication are unavailable on Windows."};
}
}
const KitTitleVariant* KitVariantFor(const VerifiedKitView&,const std::string&){return nullptr;}
std::vector<std::string> KitTitleCandidates(const VerifiedKitView&){return {};}
std::string UnmetRecipeRequirements(const std::map<std::string,std::string>&,
                                    const VerifiedKitView&){
  return "PS5 package runtime unavailable on Windows";
}
FilesystemPackageInstaller::FilesystemPackageInstaller(VerifiedKitView kit,
    PackageInstallPaths paths,PackageInstallerOperations operations):
    kit_(std::move(kit)),paths_(std::move(paths)),operations_(std::move(operations)){}
void FilesystemPackageInstaller::AcceptShippedRecipes(std::vector<std::string> recipes){
  shipped_recipe_ids_=std::move(recipes);
}
PackageStatus FilesystemPackageInstaller::Inspect(const RuntimeInstall&){return {};}
PackageStatus FilesystemPackageInstaller::InspectOwnership(const RuntimeInstall&){return {};}
PackageActionResult FilesystemPackageInstaller::WriteTitleMetadata(
    const RuntimeInstall&,const std::string&,const std::optional<std::string>&){return Unsupported();}
bool FilesystemPackageInstaller::TitleMetadataCurrent(const RuntimeInstall&){return false;}
PackageActionResult FilesystemPackageInstaller::StageTitle(const RuntimeInstall&){return Unsupported();}
PackageActionResult FilesystemPackageInstaller::CopyTemplate(const RuntimeInstall&){return Unsupported();}
PackageActionResult FilesystemPackageInstaller::ValidateArtifacts(const RuntimeInstall&){return Unsupported();}
PackageActionResult FilesystemPackageInstaller::PublishProfile(const RuntimeInstall&){return Unsupported();}
bool FilesystemPackageInstaller::TitleCurrent(const RuntimeInstall&){return false;}
bool FilesystemPackageInstaller::SetupFromOtherKit(const RuntimeInstall&){return false;}
std::string FilesystemPackageInstaller::AdmissionDiagnostic(const RuntimeInstall&){
  return "unsupported: no PS5 package admission on Windows";
}
std::string FilesystemPackageInstaller::QuickAdmission(const RuntimeInstall&){
  return "unsupported: no PS5 package admission on Windows";
}
PackageActionResult FilesystemPackageInstaller::RepairBottle(const RuntimeInstall&){return Unsupported();}
PackageActionResult FilesystemPackageInstaller::SettleBottleModes(const RuntimeInstall&){return Unsupported();}
PackageActionResult FilesystemPackageInstaller::RefreshTitle(const RuntimeInstall&){return Unsupported();}
} // namespace playden::runtime
