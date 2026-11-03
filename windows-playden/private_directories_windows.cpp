// Fail-closed Windows boundary for Unix descriptor-walk permission operations.
// Native NTFS ACL and file-lease primitives live in ntfs_security.cpp.
// This module never weakens or publishes private game/credential trees.
#include "runtime/private_directories.h"
#include "ntfs_security.hpp"
#include <cerrno>

namespace playden::runtime {
namespace fs=std::filesystem;
bool IsPrivateDirectory(const fs::path& directory,bool traversable) {
  // Windows DACLs have no POSIX 0711 equivalent: require strict private ACL.
  (void)traversable;
  return windows::IsPrivateDirectory(directory);
}
bool IsPrivateDirectoryTree(const fs::path& root,const fs::path& target,
                            const PrivateTraversal& traversal) {
  (void)traversal;
  if(!root.is_absolute() || !target.is_absolute())return false;
  auto relative=target.lexically_relative(root);
  if(relative.empty())return false;
  if(relative!=".") {
    for(const auto& component:relative)
      if(component==".." || component=="." || component.empty())return false;
  }
  if(!windows::IsPrivateDirectory(root))return false;
  fs::path current=root;
  if(relative!=".")for(const auto& component:relative) {
    current/=component;
    if(!windows::IsPrivateDirectory(current))return false;
  }
  return true;
}
bool EnsurePrivateDirectories(const fs::path&,const fs::path&,
                              const fs::path&,std::string* error,
                              const PrivateTraversal&) {
  if(error)*error="Windows private directory creation requires an NTFS publication adapter.";
  return false;
}
bool SetTraversableMode(const fs::path&,mode_t) {errno=ENOSYS;return false;}
const std::vector<fs::path>& PrivateProfilePaths(){
  static const std::vector<fs::path> paths; return paths;
}
bool IsPrivateProfilePath(const fs::path& relative,const std::vector<fs::path>& paths){
  if(relative.empty() || relative.is_absolute())return false;
  for(const auto& part:relative)if(part==".." || part=="." || part.empty())return false;
  for(const auto& path:paths){
    const auto within=relative.lexically_relative(path);
    if(within=="." )return true;
    if(within.empty() || within.is_absolute())continue;
    bool safe=true;
    for(const auto& part:within)if(part==".." || part=="." || part.empty())safe=false;
    if(safe)return true;
  }
  return false;
}
SharedTreeResult OpenTreeToEveryone(const fs::path&,const SharedTreeOptions&){
  // Windows never emulates POSIX chmod 0777 against NTFS trees.
  errno=ENOSYS;return {};
}
bool DescriptorRelativeOpens(){return false;}
int OpenFolderNoFollow(const fs::path&){errno=ENOSYS;return -1;}
int OpenChildFolderNoFollow(int,const fs::path&,const std::string&,bool){
  errno=ENOSYS;return -1;
}
} // namespace playden::runtime
