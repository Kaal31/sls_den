// Windows-only managed filesystem adapter. Private operations use owner-only
// NTFS ACL checks; operations requiring durable directory sync remain blocked.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/managed_directory.h"
#include "ntfs_security.hpp"
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
 if(policy==DirectoryPolicy::kPrivateTraversable||backend!=DirectoryBackend::kAbsolutePath ||
    (IsPrivate(policy)?!windows::IsPrivateDirectory(path):!InspectPath(path,true))){Deny();return std::nullopt;}
 ManagedDirectory out;out.root_=out.path_=path;out.policy_=policy;out.backend_=backend;return out;
}
bool ManagedDirectory::Validate() const {
 return InspectPath(root_,true) &&
    (IsPrivate(policy_)?windows::IsPrivateDirectory(path_):InspectPath(path_,true));
}
bool ManagedDirectory::ValidName(const std::string& name) const{return CleanName(name);}
std::optional<ManagedDirectory> ManagedDirectory::OpenChild(const std::string& name, DirectoryPolicy policy,bool create,mode_t) const {
 if(!Validate()||!CleanName(name)||policy==DirectoryPolicy::kPrivateTraversable ||
    (IsPrivate(policy_) && policy!=DirectoryPolicy::kPrivate)){Deny();return std::nullopt;}
 auto child=path_/fs::u8path(name);
 if(IsPrivate(policy)) {
   if(create && !windows::EnsurePrivateDirectory(child))return std::nullopt;
   if(!windows::IsPrivateDirectory(child))return std::nullopt;
 } else {
   if(create){std::error_code ec;fs::create_directory(child,ec);if(ec){Deny();return std::nullopt;}}
   if(!InspectPath(child,true))return std::nullopt;
 }
 ManagedDirectory out;out.root_=root_;out.path_=child;out.policy_=policy;out.backend_=backend_;return out;
}
bool ManagedDirectory::Inspect(const std::string& name,struct stat* result) const {
 if(!result||!CleanName(name)||!Validate()||!InspectPath(path_/fs::u8path(name),false))return Deny();
 return ::stat((path_/fs::u8path(name)).string().c_str(),result)==0;
}
bool ManagedDirectory::InspectChild(const std::string& name,struct stat* result) const{return Inspect(name,result);}
int ManagedDirectory::OpenFile(const std::string& name,int flags,mode_t) const {
 if(!CleanName(name)||!Validate()){Deny();return -1;}
 const auto target=path_/fs::u8path(name);
 if(!InspectPath(target,false))return -1;
 if(IsPrivate(policy_)&&!windows::IsPrivateFile(target)){Deny();return -1;}
 DWORD access=(flags&_O_RDWR)?(GENERIC_READ|GENERIC_WRITE):((flags&_O_WRONLY)?GENERIC_WRITE:GENERIC_READ);
 HANDLE h=CreateFileW(target.c_str(),access,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
     OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)return -1;
 BY_HANDLE_FILE_INFORMATION info{};
 if(!GetFileInformationByHandle(h,&info)|| (info.dwFileAttributes&
     (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) {CloseHandle(h);return -1;}
 const int fd=_open_osfhandle(reinterpret_cast<intptr_t>(h),flags|_O_BINARY);
 if(fd<0)CloseHandle(h);
 return fd;
}
int ManagedDirectory::OpenRegularFile(const std::string& name,bool require_owner,std::optional<mode_t> expected_mode) const {
 if((require_owner||expected_mode) && (!IsPrivate(policy_) ||
    (expected_mode && *expected_mode != 0600))){Deny();return -1;}
 return OpenFile(name,_O_RDONLY,0);
}
int ManagedDirectory::CreateExclusiveFile(const std::string& name,mode_t mode) const {
 if(policy_!=DirectoryPolicy::kPrivate||mode!=0600||!Validate()||!CleanName(name)){Deny();return -1;}
 const auto file=path_/fs::u8path(name);
 if(!windows::CreatePrivateFile(file)){Deny();return -1;}
 return OpenFile(name,_O_RDWR,mode);
}
bool ManagedDirectory::RenameChild(const std::string& from,const ManagedDirectory& dest,
                                    const std::string& to,bool replace) const {
 if(policy_!=DirectoryPolicy::kPrivate || dest.policy_!=DirectoryPolicy::kPrivate ||
    !Validate()||!dest.Validate()||!CleanName(from)||!CleanName(to))return Deny();
 const auto src=path_/fs::u8path(from),dst=dest.path_/fs::u8path(to);
 if(!windows::IsPrivateFile(src))return Deny();
 const DWORD attr=GetFileAttributesW(dst.c_str());
 if(attr!=INVALID_FILE_ATTRIBUTES && (!replace||!windows::IsPrivateFile(dst)))return Deny();
 const DWORD flags=MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0);
 return MoveFileExW(src.c_str(),dst.c_str(),flags)!=0 && windows::IsPrivateFile(dst);
}
bool ManagedDirectory::Remove(const std::string& name,bool directory) const {
 if(policy_!=DirectoryPolicy::kPrivate||!Validate()||!CleanName(name))return Deny();
 const auto path=path_/fs::u8path(name);
 if(directory)return windows::IsPrivateDirectory(path) && RemoveDirectoryW(path.c_str())!=0;
 return windows::IsPrivateFile(path) && DeleteFileW(path.c_str())!=0;
}
bool ManagedDirectory::RemoveRegularFile(const std::string& name) const{return Remove(name,false);}
bool ManagedDirectory::RemoveEmptyDirectory(const std::string& name) const{return Remove(name,true);}
bool ManagedDirectory::Sync() const{
 // NTFS does not expose POSIX directory fsync through the C runtime.
 // Never claim journal durability without a separately proven equivalent.
 return !IsPrivate(policy_) && Validate();
}
} // namespace playden::platform
