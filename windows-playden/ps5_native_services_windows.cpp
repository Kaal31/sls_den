// Windows port of PS5-only native title services.
// These operations require console kernel APIs and cannot run on Windows.
// Refuse every privileged mutation and never infer that a title is stopped.
#include "platform/ps5/title_activity.h"
#include "platform/ps5/title_allow_list.h"
#include "platform/ps5/title_remover.h"
#include <utility>
namespace playden::ps5 {
std::optional<std::vector<std::int32_t>> DecodeTitleActivityProcessIds(
    const std::vector<std::uint8_t>&) { return std::nullopt; }
runtime::NativeActivity QueryTitleActivity(
    const std::string&,const TitleActivityApi&,std::string* reason) {
  if(reason)*reason="PS5 native process inventory unavailable on Windows";
  return runtime::NativeActivity::kUnknown;
}
runtime::NativeActivity ReadTitleActivity(const std::string&,std::string* reason) {
  if(reason)*reason="PS5 kernel activity unavailable on Windows";
  return runtime::NativeActivity::kUnknown;
}
bool WriteRootOwnedFile(const std::filesystem::path&,const std::string&,
    const std::string&,const RootFileOps&,std::string* error) {
  if(error)*error="PS5 root-owned title files unsupported on Windows";
  return false;
}
bool WithRoot(const std::function<bool()>&) { return false; }
std::shared_ptr<void> HoldEffectiveUid() { return {}; }
std::shared_ptr<void> TryHoldEffectiveUid() { return {}; }
bool WriteTitleAllowList(const std::string&,std::string* error) {
  if(error)*error="PS5 title allow-list unsupported on Windows";
  return false;
}
TitleRemoverOutcome RunTitleRemover(
    const TitleRemoverConfig&,const std::string&,const std::string&) {
  return {false,std::nullopt,"PS5 title removal unsupported on Windows"};
}
bool SendToPayloadLoader(const std::string&,std::string* error) {
  if(error)*error="PS5 payload loader unsupported on Windows";
  return false;
}
} // namespace playden::ps5
