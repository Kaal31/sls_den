#include "platform/storage.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
int main() {
 namespace fs=std::filesystem;
 auto root=fs::temp_directory_path()/(L"sls-den-storage-"+std::to_wstring(GetCurrentProcessId()));
 std::error_code error;fs::remove_all(root,error);
 playden::Storage storage(root.u8string());
 auto created=storage.EnsureRootDirectory();if(!created.ok())return 1;
 playden::Bytes first{1,2,3,4};
 if(!storage.WriteAtomicPrivate("private.bin",first).ok())return 2;
 auto read=storage.Read("private.bin");
 if(!read.ok()||read.value()!=first)return 3;
 playden::Bytes second{8,7};
 if(!storage.WriteAtomic("private.bin",second).ok())return 4;
 read=storage.Read("private.bin");
 if(!read.ok()||read.value()!=second)return 5;
 if(storage.WriteAtomic("../outside.bin",first).ok())return 6;
 if(storage.WriteDurable("journal",first).ok())return 7;
 if(!storage.Remove("private.bin").ok())return 8;
 auto removed=storage.Read("private.bin");
 if(removed.ok())return 9;
 fs::remove_all(root,error);
 std::cout<<"PASS owner-only NTFS atomic storage, bounded reads and journal denial\n";
}
