#include "ntfs_security.hpp"
#include <windows.h>
#include <filesystem>
#include <iostream>
using namespace playden::windows;
int main(){
 namespace fs=std::filesystem;
 const auto parent=fs::temp_directory_path();
 const auto root=parent/(L"sls-den-ntfs-"+std::to_wstring(GetCurrentProcessId()));
 std::error_code ec; fs::remove_all(root,ec);
 if(!EnsurePrivateDirectory(root)){std::cerr<<"Could not establish protected NTFS owner ACL";return 1;}
 if(!CreatePrivateFile(root/L"lease.lock"))return 2;
 if(!IsPrivateFile(root/L"lease.lock"))return 3;
 auto first=PrivateFileLock::Acquire(root/L"lease.lock");
 if(!first)return 4;
 auto second=PrivateFileLock::Acquire(root/L"lease.lock");
 if(second)return 5;
 if(CreatePrivateFile(root/L"lease.lock"))return 6;
 if(EnsurePrivateDirectory(parent))return 7;
 first.reset();
 if(!PrivateFileLock::Acquire(root/L"lease.lock"))return 8;
 fs::remove_all(root,ec);
 std::cout<<"PASS NTFS private DACL and exclusive file lease\n";
 return 0;
}
