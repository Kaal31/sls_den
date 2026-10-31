#include "platform/ps5/title_allow_list.h"
#include <mutex>
namespace playden::ps5 {
namespace { std::recursive_mutex& Lock(){static std::recursive_mutex m;return m;} }
bool WithRoot(const std::function<bool()>&) {return false;} // Windows cannot elevate as PS5 root.
std::shared_ptr<void> HoldEffectiveUid() {
 return std::make_shared<std::unique_lock<std::recursive_mutex>>(Lock());
}
std::shared_ptr<void> TryHoldEffectiveUid() {
 auto held=std::make_shared<std::unique_lock<std::recursive_mutex>>(Lock(),std::try_to_lock);
 return held->owns_lock()?held:nullptr;
}
}
