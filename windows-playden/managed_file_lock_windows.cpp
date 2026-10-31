// Windows NTFS lease backed by Win32 file identity and LockFileEx.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/managed_file_lock.h"
#include "ntfs_security.hpp"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cerrno>
#include <utility>
namespace playden::platform {
namespace {
HANDLE OsHandle(int descriptor) {
 return descriptor<0?INVALID_HANDLE_VALUE:
    reinterpret_cast<HANDLE>(_get_osfhandle(descriptor));
}
bool Identity(const BY_HANDLE_FILE_INFORMATION& a,const BY_HANDLE_FILE_INFORMATION& b) {
 return a.dwVolumeSerialNumber==b.dwVolumeSerialNumber &&
   a.nFileIndexHigh==b.nFileIndexHigh && a.nFileIndexLow==b.nFileIndexLow &&
   a.nNumberOfLinks==1 && b.nNumberOfLinks==1 &&
   !(a.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) &&
   !(b.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
bool SameName(HANDLE opened,const std::filesystem::path& path) {
 HANDLE named=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,
   FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
   FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(named==INVALID_HANDLE_VALUE)return false;
 BY_HANDLE_FILE_INFORMATION a{},b{};
 const bool same=GetFileInformationByHandle(opened,&a) &&
   GetFileInformationByHandle(named,&b) && Identity(a,b);
 CloseHandle(named);
 return same;
}
}
std::optional<ManagedFileLock> ManagedFileLock::Acquire(
    const ManagedDirectory& directory,const std::string& filename,LockMode mode) {
 if(directory.policy()!=DirectoryPolicy::kPrivate||!directory.Validate()||
    filename.empty()||filename=="."||filename==".."||
    filename.find_first_of("/\\:")!=std::string::npos ||
    (mode!=LockMode::kReader&&mode!=LockMode::kPublisher)) {
   errno=EACCES;return std::nullopt;
 }
 const auto path=directory.path()/std::filesystem::u8path(filename);
 if(!windows::IsPrivateFile(path)){errno=EACCES;return std::nullopt;}
 HANDLE h=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
    FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)return std::nullopt;
 if(!SameName(h,path)||!directory.Validate()){
   CloseHandle(h);errno=EACCES;return std::nullopt;
 }
 OVERLAPPED ov{};
 DWORD flags=LOCKFILE_FAIL_IMMEDIATELY;
 if(mode==LockMode::kPublisher)flags|=LOCKFILE_EXCLUSIVE_LOCK;
 if(!LockFileEx(h,flags,0,1,0,&ov)){
   CloseHandle(h);return std::nullopt;
 }
 if(!SameName(h,path)||!windows::IsPrivateFile(path)){
   UnlockFileEx(h,0,1,0,&ov);CloseHandle(h);errno=EACCES;return std::nullopt;
 }
 const int descriptor=_open_osfhandle(reinterpret_cast<intptr_t>(h),_O_RDWR|_O_BINARY);
 if(descriptor<0){UnlockFileEx(h,0,1,0,&ov);CloseHandle(h);return std::nullopt;}
 return ManagedFileLock(descriptor);
}
ManagedFileLock::ManagedFileLock(ManagedFileLock&& other) noexcept
 : descriptor_(std::exchange(other.descriptor_,-1)) {}
ManagedFileLock& ManagedFileLock::operator=(ManagedFileLock&& other) noexcept {
 if(this!=&other) {
   if(descriptor_>=0){OVERLAPPED ov{};UnlockFileEx(OsHandle(descriptor_),0,1,0,&ov);_close(descriptor_);}
   descriptor_=std::exchange(other.descriptor_,-1);
 }
 return *this;
}
bool ManagedFileLock::Validate(const ManagedDirectory& directory,const std::string& filename) const {
 return directory.policy()==DirectoryPolicy::kPrivate && directory.Validate() &&
   filename.find_first_of("/\\:")==std::string::npos &&
   Holds(descriptor_,directory.path()/std::filesystem::u8path(filename));
}
bool ManagedFileLock::Holds(int descriptor,const std::filesystem::path& path) {
 HANDLE h=OsHandle(descriptor);
 return h!=INVALID_HANDLE_VALUE && windows::IsPrivateFile(path) && SameName(h,path);
}
ManagedFileLock::~ManagedFileLock() {
 if(descriptor_>=0) {
   OVERLAPPED ov{};UnlockFileEx(OsHandle(descriptor_),0,1,0,&ov);
   _close(descriptor_);
 }
}
} // namespace playden::platform
