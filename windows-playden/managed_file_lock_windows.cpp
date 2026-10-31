// Windows offline preview: private publishing locks are intentionally disabled
// until NTFS ACL and handle-identity requirements can be enforced.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/managed_file_lock.h"
#include <cerrno>
#include <utility>
namespace playden::platform {
std::optional<ManagedFileLock> ManagedFileLock::Acquire(const ManagedDirectory&,const std::string&,LockMode) {
 errno=EACCES;return std::nullopt;
}
ManagedFileLock::ManagedFileLock(ManagedFileLock&& other) noexcept
 : descriptor_(std::exchange(other.descriptor_,-1)) {}
ManagedFileLock& ManagedFileLock::operator=(ManagedFileLock&& other) noexcept {
 if(this!=&other)descriptor_=std::exchange(other.descriptor_,-1);
 return *this;
}
bool ManagedFileLock::Validate(const ManagedDirectory&,const std::string&) const{return false;}
bool ManagedFileLock::Holds(int,const std::filesystem::path&){return false;}
ManagedFileLock::~ManagedFileLock()=default;
}
