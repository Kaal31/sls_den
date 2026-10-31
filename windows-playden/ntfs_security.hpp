// Windows NTFS private directory and file lease primitives.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <optional>
namespace playden::windows {
bool EnsurePrivateDirectory(const std::filesystem::path& path);
bool IsPrivateFile(const std::filesystem::path& path);
bool CreatePrivateFile(const std::filesystem::path& path);
class PrivateFileLock {
 public:
  static std::optional<PrivateFileLock> Acquire(const std::filesystem::path& path);
  PrivateFileLock(PrivateFileLock&&) noexcept;
  PrivateFileLock& operator=(PrivateFileLock&&) noexcept;
  PrivateFileLock(const PrivateFileLock&) = delete;
  PrivateFileLock& operator=(const PrivateFileLock&) = delete;
  ~PrivateFileLock();
 private:
  explicit PrivateFileLock(HANDLE handle);
  HANDLE handle_ = INVALID_HANDLE_VALUE;
  OVERLAPPED overlap_{};
};
} // namespace playden::windows
#endif
