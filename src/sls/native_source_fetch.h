#pragma once
#include "sls/source_resolver.h"
#include "steam/backend/http_client.h"
#include <atomic>

namespace playden::sls {
// Supplies the source resolver with Playden's verified HTTPS transport.
// The caller owns the HttpClient and cancellation flag for the callback lifetime.
SourceFetch MakeNativeSourceFetch(steam::HttpClient& client,
                                 const std::atomic<bool>& cancel);
}
