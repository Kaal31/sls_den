#include "platform/ps5/system.hpp"
#include <cstdint>
int main() {
  const std::int64_t before = hui::sys::monotonic_us();
  hui::sys::sleep_us(1500);
  const std::int64_t after = hui::sys::monotonic_us();
  if (after < before || after - before < 1000) return 1;
  hui::sys::log("SLS Den Windows platform services: smoke test OK");
  if (!hui::sys::hide_splash_screen()) return 2;
  return 0;
}
