#include "sls/manifest_metadata.h"
#include <charconv>
#include <limits>
#include <map>
#include <utility>
namespace playden::sls {
namespace {
std::string_view trim(std::string_view v) {
  while (!v.empty() && (v.front() == ' ' || v.front() == '\t' || v.front() == '\r')) v.remove_prefix(1);
  while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r')) v.remove_suffix(1);
  return v;
}
bool number(std::string_view s, std::uint64_t* out) {
  s = trim(s);
  if (s.empty()) return false;
  auto r = std::from_chars(s.data(), s.data() + s.size(), *out);
  return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}
bool call(std::string_view s, std::string_view name, std::string_view* args) {
  if (s.substr(0, name.size()) != name) return false;
  s = trim(s.substr(name.size()));
  if (s.size() < 2 || s.front() != '(' || s.back() != ')') return false;
  *args = s.substr(1, s.size() - 2);
  return true;
}
}
bool ParseManifestMetadata(std::uint32_t app_id, std::string_view source,
                           ManifestMetadata* result, std::string* error) {
  auto fail = [&](const char* why) { if (error) *error = why; return false; };
  if (!result || !app_id || source.size() > 1024 * 1024) return fail("invalid input");
  ManifestMetadata parsed;
  parsed.app_id = app_id;
  std::map<std::uint32_t, DepotPin> depots;
  std::size_t pos = 0;
  for (std::size_t lines = 0; pos < source.size(); ++lines) {
    if (lines > 20000) return fail("too many lines");
    auto end = source.find('\n', pos);
    auto line = trim(source.substr(pos, end == std::string_view::npos ? source.size() - pos : end - pos));
    pos = end == std::string_view::npos ? source.size() : end + 1;
    bool disabled = line.substr(0, 2) == "--";
    if (disabled) line = trim(line.substr(2));
    std::string_view args;
    if (call(line, "addappid", &args)) {
      auto comma = args.find(',');
      std::uint64_t id = 0;
      if (!number(args.substr(0, comma), &id) || id == 0 || id > UINT32_MAX) return fail("invalid depot ID");
      if (!depots.count(static_cast<std::uint32_t>(id)) && depots.size() >= 4096) return fail("too many depots");
      auto& depot = depots[static_cast<std::uint32_t>(id)];
      depot.depot_id = static_cast<std::uint32_t>(id);
      if (!disabled) depot.enabled = true;
      else if (depot.manifest_gid == 0) depot.enabled = false;
    } else if (!disabled && call(line, "setManifestid", &args)) {
      auto comma = args.find(',');
      if (comma == std::string_view::npos) return fail("missing manifest argument");
      std::uint64_t id = 0, gid = 0;
      if (!number(args.substr(0, comma), &id) || id == 0 || id > UINT32_MAX) return fail("invalid depot ID");
      auto rhs = trim(args.substr(comma + 1));
      if (rhs.size() < 3 || (rhs.front() != '\'' && rhs.front() != '"')) return fail("expected quoted manifest GID");
      char quote = rhs.front();
      rhs.remove_prefix(1);
      auto close = rhs.find(quote);
      if (close == std::string_view::npos || !number(rhs.substr(0, close), &gid) || gid == 0)
        return fail("invalid manifest GID");
      auto it = depots.find(static_cast<std::uint32_t>(id));
      if (it == depots.end()) return fail("manifest has no declared depot");
      it->second.manifest_gid = gid;
    }
    // Other Lua commands are deliberately ignored, never executed.
  }
  for (const auto& [id, depot] : depots) parsed.depots.push_back(depot);
  *result = std::move(parsed);
  if (error) error->clear();
  return true;
}
}
