#include "platform/managed_directory.h"
#include "platform/managed_file_lock.h"
#include "ntfs_security.hpp"
#include <filesystem>
#include <fstream>
#include <io.h>
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
 const auto secure=root/"private";
 if(!playden::windows::EnsurePrivateDirectory(secure))return 9;
 auto private_dir=ManagedDirectory::Open(secure,DirectoryPolicy::kPrivate);
 if(!private_dir||!private_dir->Validate())return 10;
 int lock_fd=private_dir->CreateExclusiveFile("lease.lock",0600);
 if(lock_fd<0)return 11;
 _close(lock_fd);
 auto lease=ManagedFileLock::Acquire(*private_dir,"lease.lock",LockMode::kPublisher);
 if(!lease||!lease->Validate(*private_dir,"lease.lock"))return 12;
 if(ManagedFileLock::Acquire(*private_dir,"lease.lock",LockMode::kPublisher))return 13;
 lease.reset();
 if(!ManagedFileLock::Acquire(*private_dir,"lease.lock",LockMode::kReader))return 14;
 if(!private_dir->RemoveRegularFile("lease.lock"))return 15;
 if(private_dir->Sync())return 16;
 private_dir.reset();
 fs::remove_all(root,ec);
 std::cout<<"PASS: offline reads, protected NTFS writes and exclusive Win32 locks\n";
 return 0;
}
