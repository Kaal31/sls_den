#include "sls/steam_catalog_search.h"
#include <cassert>
#include <string>
int main() {
  using namespace playden::sls;
  const auto url=SteamStoreSearchUrl("Half Life");
  assert(url.find("term=Half+Life")!=std::string::npos);
  assert(SteamStoreSearchUrl("X").empty());
  assert(SteamStoreSearchUrl("Hello\nworld").empty());
  assert(SteamStoreSearchUrl("Half Life",0,101).empty());
  SteamCatalogPage page;
  assert(ParseSteamStoreSearchResponse(
    R"({"total_count":2,"results_html":"<a data-ds-appid=\"10\"><span class=\"title\">Example &amp; One</span></a><a data-ds-appid=\"20\"><span class=\"title\">Example Two</span></a>"})",
    &page));
  assert(page.total==2 && page.titles.size()==2);
  assert(page.titles[0].app_id==10 && page.titles[0].name=="Example & One");
  assert(page.titles[1].app_id==20 && page.titles[1].name=="Example Two");
  assert(!ParseSteamStoreSearchResponse("not-json",&page));
  assert(page.titles.size()==2);
  assert(ParseSteamStoreSearchResponse(
    R"({"total_count":3,"results_html":"<a data-ds-appid=\"10\"><span class=\"title\">A</span></a><a data-ds-appid=\"10\"><span class=\"title\">Duplicate</span></a>"})",&page));
  assert(page.titles.size()==1);
}
