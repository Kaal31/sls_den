#include "sls/source_list.h"
#include <set>
namespace playden::sls {
bool ParseSourceList(std::string_view input, std::vector<Source>* sources, std::string* reason) {
  auto fail = [&](const char* message) { if (reason) *reason = message; return false; };
  if (!sources || input.size() > 64 * 1024) return fail("invalid source list");
  std::vector<Source> result;
  std::set<std::string> ids;
  std::size_t pos = 0;
  while (pos < input.size()) {
    const auto end = input.find('\n', pos);
    auto line = input.substr(pos, end == std::string_view::npos ? input.size()-pos : end-pos);
    pos = end == std::string_view::npos ? input.size() : end + 1;
    if (line.empty()) continue;
    if (result.size() >= 16) return fail("too many sources");
    const auto a = line.find('\t');
    const auto b = a == std::string_view::npos ? a : line.find('\t', a+1);
    const auto c = b == std::string_view::npos ? b : line.find('\t', b+1);
    if (a == std::string_view::npos || b == std::string_view::npos ||
        c == std::string_view::npos || line.find('\t', c+1) != std::string_view::npos)
      return fail("expected four tab-separated fields");
    Source source;
    source.id = std::string(line.substr(0,a));
    source.display_name = std::string(line.substr(a+1,b-a-1));
    source.url_template = std::string(line.substr(b+1,c-b-1));
    const auto state = line.substr(c+1);
    if (state != "0" && state != "1") return fail("invalid enabled flag");
    source.enabled = state == "1";
    if (!ValidateSource(source)) return fail("invalid provider");
    if (!ids.insert(source.id).second) return fail("duplicate provider ID");
    result.push_back(std::move(source));
  }
  *sources = std::move(result);
  if (reason) reason->clear();
  return true;
}
std::string SerializeSourceList(const std::vector<Source>& sources) {
  if (sources.size() > 16) return {};
  std::string output;
  std::set<std::string> seen;
  for (const auto& source : sources) {
    if (!ValidateSource(source) || source.requires_api_key || !seen.insert(source.id).second ||
        source.display_name.find_first_of("\t\r\n") != std::string::npos ||
        source.url_template.find_first_of("\t\r\n") != std::string::npos) return {};
    output += source.id + "\t" + source.display_name + "\t" + source.url_template +
              "\t" + (source.enabled ? "1" : "0") + "\n";
  }
  return output;
}
}
