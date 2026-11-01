#pragma once
// Public Steam title discovery. Search results are catalog entries, never ownership records.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace playden::sls {
struct SteamCatalogTitle { std::uint32_t app_id=0; std::string name; };
struct SteamCatalogPage { std::vector<SteamCatalogTitle> titles; std::uint32_t total=0; };
std::string SteamStoreSearchUrl(std::string_view query,std::uint32_t start=0,std::uint32_t count=30);
bool ParseSteamStoreSearchResponse(std::string_view body, SteamCatalogPage* page);
}
