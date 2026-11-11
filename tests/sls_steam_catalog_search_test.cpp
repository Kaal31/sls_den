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
  // Search results are public catalog entries, not proof of ownership.
  // Validate pagination and URL encoding before any shared UI uses the query.
  const auto paged=SteamStoreSearchUrl("A&B / C",30,25);
  assert(paged.find("term=A%26B+%2F+C")!=std::string::npos);
  assert(paged.find("start=30&count=25")!=std::string::npos);
  assert(SteamStoreSearchUrl("Valid",10001,1).empty());
  assert(SteamStoreSearchUrl("Valid",0,0).empty());
  assert(SteamStoreSearchUrl("Valid",0,100).size()>0);
  assert(SteamStoreSearchUrl(std::string(101,'x')).empty());

  // Malformed and out-of-range app IDs cannot become installable catalog
  // entries. A failed parse must preserve the last successfully loaded page.
  assert(ParseSteamStoreSearchResponse(
    R"({"total_count":999999999,"results_html":"<a data-ds-appid=\"0\"><span class=\"title\">Zero</span></a><a data-ds-appid=\"4294967296\"><span class=\"title\">Overflow</span></a><a data-ds-appid=\"42\"><span class=\"title\">Valid &amp; Safe</span></a>"})",
    &page));
  assert(page.total==1000000 && page.titles.size()==1);
  assert(page.titles[0].app_id==42 && page.titles[0].name=="Valid & Safe");
  assert(!ParseSteamStoreSearchResponse(R"({"total_count":1})",&page));
  assert(page.total==1000000 && page.titles.size()==1);
  assert(!ParseSteamStoreSearchResponse("{}",nullptr));
}
