// Windows build-only boundary for console layout migration.
// No partial relocation is ever performed without a verified Windows journal.
#include "runtime/layout_migration.h"
namespace playden::runtime {
namespace {
constexpr const char* kUnavailable="Console layout migration is unsupported by the Windows host.";
}
std::optional<LayoutJournal> LoadLayoutJournal(const std::filesystem::path&,std::string* error){
  if(error)*error=kUnavailable;
  return std::nullopt;
}
bool MarkLayoutDone(const std::filesystem::path&,std::string* error){
  if(error)*error=kUnavailable;
  return false;
}
LayoutResult MigrateLayout(const LayoutPaths& paths,const LayoutOps&){
  LayoutResult result;
  result.runtime_ready=false;
  result.user_root=paths.user_root;
  result.log_root=paths.log_root;
  result.games_root=paths.games_root;
  result.notice=kUnavailable;
  return result;
}
RevertResult RevertLayout(const LayoutPaths&,const LayoutOps&){
  return {false,kUnavailable};
}
} // namespace playden::runtime
