// Windows Storage backend. Private ACL, protected file writes and bounded reads.
// Journal durability is intentionally unavailable until separately validated.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/storage.h"
#include "ntfs_security.hpp"
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <system_error>
namespace playden {
namespace fs=std::filesystem;
namespace {
bool SafeRelative(const std::string& name) {
 if(name.empty() || name.find('\\')!=std::string::npos || name.find(':')!=std::string::npos ||
    name.find('\0')!=std::string::npos)return false;
 size_t start=0;
 while(start<name.size()) {
   auto slash=name.find('/',start);
   auto part=name.substr(start,slash==std::string::npos?slash:slash-start);
   if(part.empty()||part=="."||part=="..")return false;
   if(slash==std::string::npos)return true;
   start=slash+1;
 }
 return false;
}
bool PrivateParents(const fs::path& root,const fs::path& file) {
 auto current=root;
 if(!windows::EnsurePrivateDirectory(current))return false;
 const auto rel=file.parent_path().lexically_relative(root);
 if(rel.empty() || rel==".")return true;
 for(const auto& part:rel) {
   current/=part;
   if(!windows::EnsurePrivateDirectory(current))return false;
 }
 return true;
}
Result<void> UnsafeDurability() {
 return Result<void>::Failure(StorageError::kSyncFailed,
   "Windows journal durability has not passed NTFS recovery validation");
}
}
Storage::Storage(std::string root):root_(std::move(root)) {}
Result<std::string> Storage::Resolve(const std::string& relative) const {
 if(root_.empty()||!SafeRelative(relative))return Result<std::string>::Failure(
   StorageError::kInvalidPath,"invalid relative storage path");
 return Result<std::string>::Success((fs::u8path(root_)/fs::u8path(relative)).u8string());
}
Result<void> Storage::EnsureRootDirectory() const {
 if(root_.empty()||!windows::EnsurePrivateDirectory(fs::u8path(root_)))
   return Result<void>::Failure(StorageError::kPermissionFailed,
      "Windows private root requires owner-only NTFS ACL");
 return Result<void>::Success();
}
Result<Bytes> Storage::Read(const std::string& relative,std::size_t max_bytes) const {
 if(max_bytes==0||max_bytes>32*1024*1024)
   return Result<Bytes>::Failure(StorageError::kInvalidData,"invalid read size");
 auto file=Resolve(relative);
 if(!file.ok())return Result<Bytes>::Failure(file.error(),file.message());
 fs::path path=fs::u8path(file.value());
 if(!PrivateParents(fs::u8path(root_),path)||!windows::IsPrivateFile(path))
   return Result<Bytes>::Failure(StorageError::kPermissionFailed,"untrusted file or directory ACL");
 HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,
     OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)return Result<Bytes>::Failure(StorageError::kOpenFailed,"cannot open file");
 Bytes result;
 unsigned char buffer[4096];
 bool ok=true;
 for(;;){
   DWORD got=0;
   if(!ReadFile(h,buffer,sizeof(buffer),&got,nullptr)){ok=false;break;}
   if(got==0)break;
   if(got>max_bytes-result.size()){ok=false;break;}
   result.insert(result.end(),buffer,buffer+got);
 }
 CloseHandle(h);
 if(!ok)return Result<Bytes>::Failure(StorageError::kReadFailed,"bounded private file read failed");
 return Result<Bytes>::Success(std::move(result));
}
Result<void> Storage::WriteAtomic(const std::string& name,const Bytes& data) const {
 return WriteAtomicImpl(name,data,true);
}
Result<void> Storage::WriteAtomicPrivate(const std::string& name,const Bytes& data) const {
 return WriteAtomicImpl(name,data,true);
}
Result<void> Storage::WriteAtomicImpl(const std::string& relative,const Bytes& data,
                                      bool, bool* retry) const {
 if(retry)*retry=false;
 auto target=Resolve(relative);
 if(!target.ok())return Result<void>::Failure(target.error(),target.message());
 fs::path path=fs::u8path(target.value());
 const fs::path root=fs::u8path(root_);
 if(!PrivateParents(root,path))
   return Result<void>::Failure(StorageError::kPermissionFailed,"private directory ACL not verified");
 if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES && !windows::IsPrivateFile(path))
   return Result<void>::Failure(StorageError::kPermissionFailed,"destination is not private regular file");
 fs::path temp=path;
 temp+=L".tmp";
 if(GetFileAttributesW(temp.c_str())!=INVALID_FILE_ATTRIBUTES) {
   if(!windows::IsPrivateFile(temp)||!DeleteFileW(temp.c_str()))
     return Result<void>::Failure(StorageError::kPermissionFailed,"unsafe existing temp file");
 }
 if(!windows::CreatePrivateFile(temp))
   return Result<void>::Failure(StorageError::kOpenFailed,"could not create private temporary file");
 HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
     FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)
   return Result<void>::Failure(StorageError::kOpenFailed,"could not open private temporary file");
 size_t offset=0;
 bool ok=true;
 while(offset<data.size()) {
   DWORD len=static_cast<DWORD>(std::min<std::size_t>(data.size()-offset,1024*1024));
   DWORD count=0;
   if(!WriteFile(h,data.data()+offset,len,&count,nullptr)||!count){ok=false;break;}
   offset+=count;
 }
 if(ok)ok=FlushFileBuffers(h)!=0;
 CloseHandle(h);
 if(!ok)return Result<void>::Failure(StorageError::kWriteFailed,"private temp write failed");
 if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
   return Result<void>::Failure(StorageError::kRenameFailed,"private file replacement failed");
 if(!windows::IsPrivateFile(path))
   return Result<void>::Failure(StorageError::kPermissionFailed,"private ACL lost after replacement");
 return Result<void>::Success();
}
Result<void> Storage::WriteDurable(const std::string&,const Bytes&) const{return UnsafeDurability();}
Result<void> Storage::WriteDurableExclusive(const std::string&,const Bytes&) const{return UnsafeDurability();}
Result<Bytes> Storage::ReadDurable(const std::string&,std::size_t) const {
 return Result<Bytes>::Failure(StorageError::kSyncFailed,"durable journal unsupported on Windows preview");
}
Result<void> Storage::RemoveDurable(const std::string&) const{return UnsafeDurability();}
Result<std::vector<StoredFile>> Storage::ListFiles() const {
 const fs::path root=fs::u8path(root_);
 if(!windows::EnsurePrivateDirectory(root))
   return Result<std::vector<StoredFile>>::Failure(StorageError::kPermissionFailed,"untrusted root");
 std::vector<StoredFile> files;
 std::error_code error;
 for(auto it=fs::directory_iterator(root,error);!error&&it!=fs::directory_iterator();it.increment(error)) {
   if(!windows::IsPrivateFile(it->path()))continue;
   StoredFile item{};
   item.name=it->path().filename().u8string();
   files.push_back(std::move(item));
 }
 if(error)return Result<std::vector<StoredFile>>::Failure(StorageError::kReadFailed,"directory enumeration failed");
 return Result<std::vector<StoredFile>>::Success(std::move(files));
}
Result<void> Storage::Remove(const std::string& relative) const {
 auto result=Resolve(relative);
 if(!result.ok())return Result<void>::Failure(result.error(),result.message());
 const fs::path file=fs::u8path(result.value());
 if(GetFileAttributesW(file.c_str())==INVALID_FILE_ATTRIBUTES)return Result<void>::Success();
 if(!PrivateParents(fs::u8path(root_),file)||!windows::IsPrivateFile(file))
   return Result<void>::Failure(StorageError::kPermissionFailed,"unsafe file removal");
 if(!DeleteFileW(file.c_str()))
   return Result<void>::Failure(StorageError::kRemoveFailed,"file removal failed");
 return Result<void>::Success();
}
} // namespace playden
