// Windows preview does not have PS5 FTP permission elevation or title-root
// access. Never chmod or delete imported files through a console operation.
#include "stores/local/drop_folder_access.h"
#include <filesystem>
#include <utility>
namespace playden::stores::local {
bool IsDropFolderEntry(const std::string& entry) {
  const std::filesystem::path path(entry);
  const auto name=path.filename().string();
  return entry.find('\0')==std::string::npos &&
         path.lexically_normal()==path &&
         path.parent_path()==kDropFolder &&
         !name.empty() && name!="." && name!="..";
}
DropFolderAccessResult OpenDropFolderEntry(const std::string& entry,
                                          const DropFolderAccessOptions& options) {
  DropFolderAccessResult result;
  result.message="Console FTP drop-folder permission changes are unavailable on Windows.";
  if(options.log)options.log("Windows drop folder operation refused: "+entry);
  return result;
}
DropFolderRemoval RemoveDropFolderEntry(const std::string& folder,const std::string& name,
                                        const DropFolderPlanned&,
                                        const runtime::WritableFoldersAsRoot&,
                                        const std::function<void(const std::string&)>& log) {
  DropFolderRemoval result;
  result.message="Console drop-folder deletion is unavailable on Windows.";
  if(log)log("Windows drop folder removal refused: "+folder+"/"+name);
  return result;
}
DropFolderOpener MakeDropFolderOpener(DropFolderAccessOptions options) {
  return [options=std::move(options)](const std::string& entry) {
    return OpenDropFolderEntry(entry,options).message;
  };
}
} // namespace playden::stores::local
