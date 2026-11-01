#pragma once
#include "sls/luatools_auth.h"
#include "steam/backend/http_client.h"
#include <atomic>
namespace playden::sls {
// Real HTTPS adapter, shared by Windows and PS5 builds. The host owns HttpClient.
bool RedeemLuatoolsViaHttp(steam::HttpClient& http,const std::atomic<bool>& cancel,
                            std::string_view bot_code,LuatoolsSession* session);
}
