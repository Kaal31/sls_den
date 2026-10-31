// Windows-only NTFS private ACL and exclusive file-lock support.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ntfs_security.hpp"
#include <aclapi.h>
#include <sddl.h>
#include <vector>
#include <string>
#include <cwchar>
#include <utility>
#pragma comment(lib, "advapi32.lib")
namespace playden::windows {
namespace fs=std::filesystem;
namespace {
struct Handle { HANDLE h=INVALID_HANDLE_VALUE; ~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);} };
struct Local { HLOCAL p=nullptr; ~Local(){if(p)LocalFree(p);} };
bool UserSid(std::vector<unsigned char>& storage, PSID* sid, std::wstring* sid_text) {
 Handle token;
 if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token.h))return false;
 DWORD length=0;
 GetTokenInformation(token.h,TokenUser,nullptr,0,&length);
 if(!length)return false;
 storage.resize(length);
 if(!GetTokenInformation(token.h,TokenUser,storage.data(),length,&length))return false;
 *sid=reinterpret_cast<TOKEN_USER*>(storage.data())->User.Sid;
 LPWSTR name=nullptr;
 if(!ConvertSidToStringSidW(*sid,&name))return false;
 *sid_text=name;
 LocalFree(name);
 return true;
}
bool SecureAttributes(std::wstring* sddl, Local* security) {
 std::vector<unsigned char> token;
 PSID sid=nullptr;std::wstring name;
 if(!UserSid(token,&sid,&name))return false;
 *sddl=L"O:"+name+L"D:P(A;;FA;;;SY)(A;;FA;;;"+name+L")";
 PSECURITY_DESCRIPTOR descriptor=nullptr;
 if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl->c_str(),
     SDDL_REVISION_1,&descriptor,nullptr))return false;
 security->p=descriptor;
 return true;
}
bool NoReparseComponents(const fs::path& path) {
 if(!path.is_absolute())return false;
 fs::path prefix=path.root_path();
 for(const auto& part:path.relative_path()) {
   prefix/=part;
   const DWORD attr=GetFileAttributesW(prefix.c_str());
   if(attr==INVALID_FILE_ATTRIBUTES || (attr&FILE_ATTRIBUTE_REPARSE_POINT))return false;
 }
 return true;
}
bool NTFS(const fs::path& path) {
 wchar_t volume[MAX_PATH]{}, name[MAX_PATH]{};
 if(!GetVolumePathNameW(path.c_str(),volume,MAX_PATH))return false;
 if(!GetVolumeInformationW(volume,nullptr,0,nullptr,nullptr,nullptr,name,MAX_PATH))return false;
 return _wcsicmp(name,L"NTFS")==0 || _wcsicmp(name,L"ReFS")==0;
}
bool PrivateAcl(const fs::path& path) {
 if(!NoReparseComponents(path)||!NTFS(path))return false;
 std::vector<unsigned char> storage;PSID sid=nullptr;std::wstring name;
 if(!UserSid(storage,&sid,&name))return false;
 PSID owner=nullptr; PACL dacl=nullptr; PSECURITY_DESCRIPTOR raw=nullptr;
 DWORD error=GetNamedSecurityInfoW(const_cast<wchar_t*>(path.c_str()),SE_FILE_OBJECT,
    OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&dacl,nullptr,&raw);
 Local descriptor;descriptor.p=raw;
 if(error!=ERROR_SUCCESS||!raw||!owner||!EqualSid(owner,sid)||!dacl)return false;
 SECURITY_DESCRIPTOR_CONTROL control{};DWORD revision=0;
 if(!GetSecurityDescriptorControl(raw,&control,&revision)||
    !(control&SE_DACL_PROTECTED))return false;
 BYTE system_sid_buf[SECURITY_MAX_SID_SIZE];DWORD size=sizeof(system_sid_buf);
 if(!CreateWellKnownSid(WinLocalSystemSid,nullptr,system_sid_buf,&size))return false;
 for(DWORD i=0;i<dacl->AceCount;++i) {
   void* ptr=nullptr;
   if(!GetAce(dacl,i,&ptr))return false;
   auto* ace=reinterpret_cast<ACCESS_ALLOWED_ACE*>(ptr);
   if(ace->Header.AceType!=ACCESS_ALLOWED_ACE_TYPE||
      (ace->Header.AceFlags&INHERITED_ACE))return false;
   PSID allowed=&ace->SidStart;
   if(!EqualSid(allowed,sid)&&!EqualSid(allowed,system_sid_buf))return false;
 }
 return true;
}
bool IsRegularPrivate(const fs::path& path) {
 if(!PrivateAcl(path))return false;
 Handle h;
 h.h=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ, nullptr,
    OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h.h==INVALID_HANDLE_VALUE)return false;
 BY_HANDLE_FILE_INFORMATION info{};
 return GetFileInformationByHandle(h.h,&info) &&
    !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
}
bool EnsurePrivateDirectory(const fs::path& path) {
 if(!path.is_absolute()||!NTFS(path))return false;
 if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES) {
   if(!NoReparseComponents(path.parent_path()))return false;
   std::wstring sddl;Local sd;
   if(!SecureAttributes(&sddl,&sd))return false;
   SECURITY_ATTRIBUTES attrs{sizeof(attrs),sd.p,FALSE};
   if(!CreateDirectoryW(path.c_str(),&attrs))return false;
 }
 const DWORD attr=GetFileAttributesW(path.c_str());
 return attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY) &&
   !(attr&FILE_ATTRIBUTE_REPARSE_POINT) && PrivateAcl(path);
}
bool IsPrivateFile(const fs::path& path) { return IsRegularPrivate(path); }
bool CreatePrivateFile(const fs::path& path) {
 if(!EnsurePrivateDirectory(path.parent_path()))return false;
 std::wstring sddl;Local sd;
 if(!SecureAttributes(&sddl,&sd))return false;
 SECURITY_ATTRIBUTES attrs{sizeof(attrs),sd.p,FALSE};
 Handle h;
 h.h=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,&attrs,
     CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h.h==INVALID_HANDLE_VALUE)return false;
 return PrivateAcl(path);
}
PrivateFileLock::PrivateFileLock(HANDLE handle):handle_(handle){}
std::optional<PrivateFileLock> PrivateFileLock::Acquire(const fs::path& path) {
 if(!IsRegularPrivate(path))return std::nullopt;
 HANDLE h=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
     nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)return std::nullopt;
 if(!IsRegularPrivate(path)){CloseHandle(h);return std::nullopt;}
 PrivateFileLock lease(h);
 if(!LockFileEx(h,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&lease.overlap_))
     return std::nullopt;
 return lease;
}
PrivateFileLock::PrivateFileLock(PrivateFileLock&& other) noexcept:handle_(std::exchange(other.handle_,INVALID_HANDLE_VALUE)),overlap_(other.overlap_){}
PrivateFileLock& PrivateFileLock::operator=(PrivateFileLock&& other) noexcept {
 if(this!=&other){if(handle_!=INVALID_HANDLE_VALUE){UnlockFileEx(handle_,0,1,0,&overlap_);CloseHandle(handle_);}
 handle_=std::exchange(other.handle_,INVALID_HANDLE_VALUE);overlap_=other.overlap_;}
 return *this;
}
PrivateFileLock::~PrivateFileLock(){if(handle_!=INVALID_HANDLE_VALUE){UnlockFileEx(handle_,0,1,0,&overlap_);CloseHandle(handle_);}}
} // namespace playden::windows
