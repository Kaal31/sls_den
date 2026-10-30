#include "sls/source_list.h"
#include <cassert>
int main() {
  using namespace playden::sls;
  std::vector<Source> providers = {
    {"primary", "Primary", "https://a.example/{appid}", true},
    {"backup", "Backup", "https://b.example/{appid}", true}
  };
  const auto serialized = SerializeSourceList(providers);
  std::vector<Source> parsed;
  assert(ParseSourceList(serialized, &parsed));
  assert(parsed.size() == 2);
  assert(parsed[0].id == "primary");
  assert(parsed[1].id == "backup");
  assert(!ParseSourceList("bad input", &parsed));
  assert(parsed.size() == 2);
  return 0;
}
