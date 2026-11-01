#include "sls/luatools_http.h"
#include <algorithm>
namespace playden::sls {
namespace {
// Public Supabase anonymous key from lua.tools client (not an account secret).
constexpr const char* kSupabaseAnon=
  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
  "eyJpYXQiOjE3NzYwMzkzNzYsImV4cCI6MTg5MzQ1NjAwMCwicm9sZSI6ImFub24iLCJpc3MiOiJzdXBhYmFzZSJ9."
  "f_-K38u3odjltP-g_67FVmG32Vg-_-k-lNBvIaVUVBM";
LuatoolsPost MakeAuthenticatedPost(steam::HttpClient& http,const std::atomic<bool>& cancel) {
  return [&](std::string_view url,std::string_view body,std::string* response) {
      if(cancel.load() || !response)return false;
      steam::HttpRequest request;
      request.method=steam::HttpMethod::kPost;
      request.url=std::string(url);
      request.timeout_ms=30000;
      request.max_response_bytes=64*1024;
      request.allow_plain_http=false;
      request.headers={{"Content-Type","application/json"},
                       {"Accept","application/json"},
                       {"apikey",kSupabaseAnon},
                       {"User-Agent","LuaDen/1.0"}};
      request.body.assign(body.begin(),body.end());
      auto result=http.Perform(request,cancel);
      std::fill(request.body.begin(),request.body.end(),0);
      if(!result.ok() || (result.status_code!=200 && result.status_code!=201) ||
         result.body.empty() || result.body.size()>request.max_response_bytes)
        return false;
      response->assign(reinterpret_cast<const char*>(result.body.data()),result.body.size());
      std::fill(result.body.begin(),result.body.end(),0);
      return true;
    };
}
bool RedeemLuatoolsViaHttp(steam::HttpClient& http,const std::atomic<bool>& cancel,
                            std::string_view bot_code,LuatoolsSession* session) {
  if(cancel.load() || !session)return false;
  return RedeemLuatoolsCode(bot_code,MakeAuthenticatedPost(http,cancel),session);
}
bool RefreshLuatoolsViaHttp(steam::HttpClient& http,const std::atomic<bool>& cancel,
                           const LuatoolsSession& prior,LuatoolsSession* renewed) {
  if(cancel.load() || !renewed)return false;
  return RefreshLuatoolsSession(prior,MakeAuthenticatedPost(http,cancel),renewed);
}
}
