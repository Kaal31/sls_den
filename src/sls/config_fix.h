#pragma once
// Bounded, reversible configuration-file fixes. Host must supply a verified
// per-game root and vetted replacement payload; no network or manifest execution.
#include <filesystem>
#include <string>
#include <string_view>
namespace playden::sls {
enum class FixResult { Applied, Restored, AlreadyApplied, NotApplied, UnsafePath,
                       TooLarge, Conflict, IoError };
FixResult ApplyConfigFix(const std::filesystem::path& game_root,
                         const std::filesystem::path& relative,
                         std::string_view replacement);
FixResult RestoreConfigFix(const std::filesystem::path& game_root,
                           const std::filesystem::path& relative);
}
