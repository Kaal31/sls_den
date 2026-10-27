#include "sls/source_catalog.h"
#include <cassert>
int main() {
  using namespace playden::sls;
  Source okay{"sample", "Sample", "https://example.org/apps/{appid}.lua"};
  assert(ValidateSource(okay));
  assert(SourceUrl(okay, 480) == "https://example.org/apps/480.lua");
  assert(!ValidateSource(Source{"bad", "Bad", "http://example.org/{appid}"}));
  assert(!ValidateSource(Source{"bad", "Bad", "https://example.org/{appid}/{appid}"}));
  auto list = ActiveSources({okay, okay, Source{"other","Other","https://example.org/{appid}", false}});
  assert(list.size() == 1);
}
