// Windows host implementation for pinned ps5-homebrew-ui system services.
// PS5 and macOS source files remain unchanged.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "platform/ps5/system.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace hui::sys {
std::int64_t monotonic_us() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}
void log(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
}
bool hide_splash_screen() { return true; }
void sleep_us(std::uint32_t microseconds) {
    std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}
[[noreturn]] void park() { std::exit(EXIT_FAILURE); }
[[noreturn]] void quit() { std::exit(EXIT_SUCCESS); }
} // namespace hui::sys
