// Windows full-host compilation adapter for PS5 native title coordination.
// No PS5 app.db or ShadowMount observations are assumed on Windows.
// All native registration and title availability remain unknown/unavailable.
#include "platform/ps5/native_coordinator.h"
#include <utility>
namespace playden::ps5 {
TitleSourceObservation ReadTitleSource(const std::filesystem::path&,const std::string&) {
  return {};
}
Ps5NativeCoordinator::Ps5NativeCoordinator(runtime::InstallRecordStore& records,
    runtime::PackageInstaller& package,TitleRegistry& registry,
    std::filesystem::path root,NativeCoordinatorOperations operations)
    : records_(records),package_(package),registry_(registry),
      root_(std::move(root)),operations_(std::move(operations)) {}
std::function<runtime::NativeActivity(const std::string&)> LoggedTitleActivity(
    std::function<void(const std::string&)> log) {
  return [log=std::move(log)](const std::string& title) {
    if(log)log("PS5 title activity unavailable on Windows: "+title);
    return runtime::NativeActivity::kUnknown;
  };
}
runtime::NativeTitleAvailability Ps5NativeCoordinator::Availability(const std::string&) {
  return {};
}
Ps5NativeCoordinator::AvailabilityReport Ps5NativeCoordinator::ReportAvailability(const std::string&) {
  return {};
}
std::string Ps5NativeCoordinator::AvailabilityReport::Text() const {
  return "app.db=unknown folder=unknown shadowmount=unavailable activity=unknown (Windows)";
}
runtime::TitleAvailability Ps5NativeCoordinator::Inventory(const std::string&) {
  return runtime::TitleAvailability::kUnknown;
}
runtime::NativeActivity Ps5NativeCoordinator::Activity(const std::string&) {
  return runtime::NativeActivity::kUnknown;
}
runtime::NativeStatus Ps5NativeCoordinator::Query(const runtime::RuntimeInstall&) {
  return {};
}
runtime::NativeStatus Ps5NativeCoordinator::QueryOwnership(const runtime::RuntimeInstall&) {
  return {};
}
runtime::NativeStatus Ps5NativeCoordinator::QueryWith(const runtime::RuntimeInstall&,bool) {
  return {};
}
runtime::RegistrationResult Ps5NativeCoordinator::Register(const runtime::RuntimeInstall&) {
  return {runtime::RegistrationResultState::kFailed,"windows-native-unavailable",
          "PS5 native title registration is unsupported on Windows."};
}
} // namespace playden::ps5
