#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace playden::sls {
struct Source {
  std::string id;
  std::string display_name;
  std::string url_template; // Must contain {appid}, substituted only with a decimal AppID.
  bool enabled = true;
  bool requires_api_key = false;
};
// Source URLs are provided by configuration, not hard-coded or silently trusted.
bool ValidateSource(const Source& source, std::string* why = nullptr);
std::string SourceUrl(const Source& source, std::uint32_t app_id);
std::vector<Source> ActiveSources(const std::vector<Source>& sources);
}
