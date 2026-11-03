// Windows filesystem write checks for the desktop preview.
// No PS5 root/FTP permission elevation or global chmod semantics on Windows.
#include "runtime/writable_folders.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <string>
namespace playden::runtime {
namespace {
namespace fs=std::filesystem;
std::string CheckDirectory(const fs::path& folder) {
  std::error_code ec;
  if(!folder.is_absolute() || fs::is_symlink(folder,ec) || ec ||
     !fs::is_directory(folder,ec) || ec)
    return "Windows writable folder is missing or not a regular directory: "+folder.string();
  wchar_t name[MAX_PATH + 1]{};
  // GetTempFileNameW reserves a unique file and never truncates a preexisting one.
  const auto path=folder.wstring();
  if(path.size()>=MAX_PATH || !GetTempFileNameW(path.c_str(),L"pdn",0,name))
    return "Cannot write into Windows directory: "+folder.string();
  if(!DeleteFileW(name))
    return "Cannot remove write-test file from Windows directory: "+folder.string();
  return {};
}
}
std::string EnsureWritableFolders(const std::vector<fs::path>& folders,
    const WritableFoldersAsRoot&,const std::function<void(const std::string&)>& log,
    const fs::path& namespace_root) {
  if(namespace_root != fs::path("/") && !namespace_root.empty())
    return "PS5 namespace remapping is unavailable in Windows desktop mode.";
  for(const auto& folder:folders) {
    const auto error=CheckDirectory(folder);
    if(!error.empty()) {if(log)log(error);return error;}
  }
  return {};
}
std::string PrepareRuntimeRoot(const fs::path& root,const WritableFoldersAsRoot&,
    const std::function<void(const std::string&)>& log,const fs::path&) {
  if(!root.is_absolute() || root.empty())
    return "Windows runtime root must be an absolute native path.";
  std::error_code ec;
  if(fs::is_symlink(root,ec) || ec) return "Windows runtime root may not be a link.";
  fs::create_directories(root,ec);
  if(ec)return "Cannot create Windows runtime directory: "+root.string();
  const auto error=CheckDirectory(root);
  if(!error.empty() && log)log(error);
  return error;
}
std::string PrepareDropFolder(const fs::path&,const WritableFoldersAsRoot&,
    const std::function<void(const std::string&)>& log,const fs::path&) {
  const std::string error="PS5 FTP drop folder is not available in Windows desktop mode.";
  if(log)log(error);
  return error;
}
std::string PrepareAppFolderStubs(const fs::path&,const WritableFoldersAsRoot&,
    const std::function<void(const std::string&)>& log,const fs::path&) {
  const std::string error="PS5 /data/homebrew folder stubs are not used on Windows.";
  if(log)log(error);
  return error;
}
} // namespace playden::runtime
