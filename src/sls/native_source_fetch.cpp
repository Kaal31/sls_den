#include "sls/native_source_fetch.h"
#include <string>

namespace playden::sls {
SourceFetch MakeNativeSourceFetch(steam::HttpClient& client,
                                 const std::atomic<bool>& cancel) {
  return [&client, &cancel](const std::string& url, std::string* payload, std::string* error) {
    if (!payload || !error || cancel.load()) return false;
    payload->clear();
    error->clear();
    // SourceUrl validates templates; still check the resolved URL at the transport boundary.
    if (url.compare(0, 8, "https://") != 0 || url.size() > 2048) {
      *error = "invalid_https_url";
      return false;
    }
    steam::HttpRequest request;
    request.method = steam::HttpMethod::kGet;
    request.url = url;
    request.timeout_ms = 10000;
    request.max_response_bytes = 1024 * 1024;
    request.allow_plain_http = false;
    auto response = client.Perform(request, cancel);
    if (!response.ok()) {
      *error = steam::HttpErrorName(response.error);
      return false;
    }
    if (response.status_code != 200) {
      *error = "unexpected_http_status";
      return false;
    }
    if (response.body.empty() || response.body.size() > request.max_response_bytes) {
      *error = "invalid_response_length";
      return false;
    }
    payload->assign(reinterpret_cast<const char*>(response.body.data()), response.body.size());
    return true;
  };
}
}
