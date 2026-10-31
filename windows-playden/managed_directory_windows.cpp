// Windows-only managed filesystem adapter for OFFLINE preview.
// Reparse points are rejected. Privileged/private publication is fail-closed
// until a dedicated Windows ACL + Win32 HANDLE implementation is audited.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/managed_directory.h"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cerrno>
#include <algorithm>
#include <utility>
namespace playden::platform {
namespace fs = std::filesystem;
namespace {
bool Deny() { errno = EACCES; return false; }
bool IsPrivate(DirectoryPolicy p) {
  return p == DirectoryPolicy::kPrivate || p == DirectoryPolicy::kPrivateTraversable;
}
bool CleanName(const std::string& name) {
  return !name.empty() && name != "." && name != ".." &&
    name.find_first_of("/\\:") == std::string::npos;
}
bool InspectPath(const fs::path& path, bool directory) {
  if (!path.is_absolute()) {errno=EINVAL;return false;}
  fs::path cursor = path.root_path();
  for (auto part : path.relative_path()) {
    cursor /= part;
    auto attr = GetFileAttributesW(cursor.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
      errno=EACCES; return false;
    }
  }
  const DWORD attr = GetFileAttributesW(path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES || !!(attr & FILE_ATTRIBUTE_DIRECTORY) != directory) {
    errno=ENOENT;return false;
  }
  return true;
}
}
std::optional<bool> RecordsOwnership(const fs::path&) { return std::nullopt; }
void SetOwnershipProbeForTesting(std::optional<bool> (*)(const fs::path&)) {}
DirectoryBackend ManagedDirectory::DefaultBackend() {return DirectoryBackend::kAbsolutePath;}
ManagedDirectory::ManagedDirectory(ManagedDirectory&& other) noexcept
 : fd_(std::exchange(other.fd_,-1)),root_(std::move(other.root_)),path_(std::move(other.path_)),
 policy_(other.policy_),backend_(other.backend_),uid_(other.uid_),observations_(std::move(other.observations_)) {}
ManagedDirectory& ManagedDirectory::operator=(ManagedDirectory&& other) noexcept {
 if(this!=&other){if(fd_>=0) _close(fd_);fd_=std::exchange(other.fd_,-1);root_=std::move(other.root_);
 path_=std::move(other.path_);policy_=other.policy_;backend_=other.backend_;uid_=other.uid_;observations_=std::move(other.observations_);}
 return *this;
}
ManagedDirectory::~ManagedDirectory(){if(fd_>=0)_close(fd_);}
std::optional<ManagedDirectory> ManagedDirectory::Open(const fs::path& path,DirectoryPolicy policy,DirectoryBackend backend){
 if(IsPrivate(policy)||backend!=DirectoryBackend::kAbsolutePath||!InspectPath(path,true)){Deny();return std::nullopt;}
 ManagedDirectory out;out.root_=out.path_=path;out.policy_=policy;out.backend_=backend;return out;
}
bool ManagedDirectory::Validate() const {return !IsPrivate(policy_)&&InspectPath(root_,true)&&InspectPath(path_,true);}
bool ManagedDirectory::ValidName(const std::string& name) const{return CleanName(name);}
std::optional<ManagedDirectory> ManagedDirectory::OpenChild(const std::string& name, DirectoryPolicy policy,bool create,mode_t) const {
 if(!Validate()||!CleanName(name)||IsPrivate(policy)){Deny();return std::nullopt;}
 auto child=path_/fs::u8path(name);
 if(create){std::error_code ec;fs::create_directory(child,ec);if(ec){Deny();return std::nullopt;}}
 if(!InspectPath(child,true))return std::nullopt;
 ManagedDirectory out;out.root_=root_;out.path_=child;out.policy_=policy;out.backend_=backend_;return out;
}
bool ManagedDirectory::Inspect(const std::string& name,struct stat* result) const {
 if(!result||!CleanName(name)||!Validate()||!InspectPath(path_/fs::u8path(name),false))return Deny();
 return _wstat((path_/fs::u8path(name)).c_str(),result)==0;
}
bool ManagedDirectory::InspectChild(const std::string& name,struct stat* result) const{return Inspect(name,result);}
int ManagedDirectory::OpenFile(const std::string& name,int flags,mode_t) const {
 if(!CleanName(name)||!Validate()||IsPrivate(policy_)){Deny();return -1;}
 const auto target=path_/fs::u8path(name);
 if(!InspectPath(target,false))return -1;
 return _wopen(target.c_str(),flags|_O_BINARY);
}
int ManagedDirectory::OpenRegularFile(const std::string& name,bool require_owner,std::optional<mode_t> expected_mode) const {
 if(require_owner||expected_mode){Deny();return -1;}
 return OpenFile(name,_O_RDONLY,0);
}
int ManagedDirectory::CreateExclusiveFile(const std::string&,mode_t) const {Deny();return -1;}
bool ManagedDirectory::RenameChild(const std::string&,const ManagedDirectory&,const std::string&,bool) const{return Deny();}
bool ManagedDirectory::Remove(const std::string&,bool) const{return Deny();}
bool ManagedDirectory::RemoveRegularFile(const std::string& name) const{return Remove(name,false);}
bool ManagedDirectory::RemoveEmptyDirectory(const std::string& name) const{return Remove(name,true);}
bool ManagedDirectory::Sync() const{return Validate();}
} // namespace playden::platform
