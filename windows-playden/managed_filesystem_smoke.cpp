#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace playden::platform;
int main() {
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/ "sls-win-managed-offline-test";
 std::error_code ec;
 fs::create_directories(root,ec);
 if(ec) return 1;
 {std::ofstream out(root/"safe.txt",std::ios::binary);out<<"safe"; }
 auto approved=ManagedDirectory::Open(root,DirectoryPolicy::kApprovedTree);
 if(!approved||!approved->Validate())return 2;
 auto fd=approved->OpenRegularFile("safe.txt");
 if(fd<0)return 3;
 _close(fd);
 if(approved->OpenRegularFile("../unsafe")>=0)return 4;
 if(ManagedDirectory::Open(root,DirectoryPolicy::kPrivate))return 5;
 if(ManagedFileLock::Acquire(*approved,"safe.txt",LockMode::kPublisher))return 6;
 if(approved->CreateExclusiveFile("creds.txt",0600)>=0)return 7;
 if(fs::exists(root/"creds.txt"))return 8;
 fs::remove_all(root,ec);
 std::cout<<"PASS: offline filesystem read-only with private writes locked\n";
 return 0;
}
