#include "sls/source_catalog.h"
#include <algorithm>
#include <cctype>
#include <set>
namespace playden::sls {
bool ValidateSource(const Source& s, std::string* why) {
  auto fail = [&](const char* why_text) { if (why) *why = why_text; return false; };
  if (s.id.empty() || s.id.size() > 64 || s.display_name.empty() || s.display_name.size() > 128)
    return fail("invalid source identity");
  if (!std::all_of(s.id.begin(), s.id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_';
      })) return fail("invalid source ID");
  if (s.url_template.size() > 2048 || s.url_template.compare(0, 8, "https://") != 0)
    return fail("HTTPS required");
  if (s.url_template.find('#') != std::string::npos || s.url_template.find('@') != std::string::npos)
    return fail("URL fragments and userinfo are not supported");
  auto at = s.url_template.find("{appid}");
  if (at == std::string::npos || s.url_template.find("{appid}", at + 7) != std::string::npos)
    return fail("exactly one {appid} required");
  if (why) why->clear();
  return true;
}
std::string SourceUrl(const Source& s, std::uint32_t app_id) {
  if (!app_id || !ValidateSource(s)) return {};
  std::string url = s.url_template;
  auto at = url.find("{appid}");
  url.replace(at, 7, std::to_string(app_id));
  return url;
}
std::vector<Source> ActiveSources(const std::vector<Source>& sources) {
  std::vector<Source> result;
  std::set<std::string> seen;
  for (const auto& s : sources)
    if (s.enabled && !s.requires_api_key && ValidateSource(s) && seen.insert(s.id).second)
      result.push_back(s);
  return result;
}
}
