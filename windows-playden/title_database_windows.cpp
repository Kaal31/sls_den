// A Windows host cannot inspect the PS5 system title database.
// Unknown is mandatory: "free" would permit unsafe title-id allocation.
#include "platform/ps5/title_database.h"
namespace playden::ps5 {
TitleDatabaseObservation ReadTitleDatabase(const std::string&, const std::string&) {
    return {};
}
TitleDatabaseObservation ReadNativeTitleDatabase(const std::string&) {
    return {};
}
} // namespace playden::ps5
