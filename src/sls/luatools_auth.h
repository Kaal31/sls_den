#pragma once
// lua.tools bot-code sign-in contract, independent of Windows and PS5.
// The short-lived one-time code must only be sent to the official HTTPS redeem endpoint.
// Production hosts provide a TLS-verifying POST callback and protected session storage.
#include "steam/backend/json.h"
#include <functional>
#include <algorithm>
#include <utility>
#include <string>
#include <string_view>
namespace playden::sls {
inline constexpr std::string_view kLuatoolsRedeemUrl="https://lua.tools/api/auth/code/redeem";
struct LuatoolsSession {
  std::string access_token;
  std::string refresh_token;
  int expires_in=0;
};
using LuatoolsPost = std::function<bool(std::string_view url,std::string_view body,
                                         std::string* response)>;
inline bool ValidBotCode(std::string_view code) {
  if(code.empty() || code.size()>128)return false;
  for(unsigned char c:code) if(!(c>='A'&&c<='Z') && !(c>='a'&&c<='z') &&
                             !(c>='0'&&c<='9') && c!='-')return false;
  return true;
}
inline bool ParseLuatoolsSession(std::string_view response,LuatoolsSession* output) {
  if(!output || response.size()>64*1024)return false;
  auto parsed=steam::json::Parse(response);
  if(!parsed || !parsed->is_object())return false;
  const auto* session=parsed->Find("session");
  if(!session || !session->is_object())session=&*parsed;
  const auto* access=session->Find("access_token");
  if(!access || !access->is_string() || access->text().empty() ||
     access->text().size()>8192)return false;
  const auto* refresh=session->Find("refresh_token");
  if(refresh && (!refresh->is_string() || refresh->text().size()>8192))return false;
  LuatoolsSession value;
  value.access_token=access->text();
  if(refresh)value.refresh_token=refresh->text();
  const auto* expiry=session->Find("expires_in");
  if(expiry){auto n=expiry->AsUint64();if(n && *n<=86400)value.expires_in=int(*n);}
  *output=std::move(value);
  return true;
}
inline bool RedeemLuatoolsCode(std::string_view code,const LuatoolsPost& post,
                                LuatoolsSession* output) {
  if(!ValidBotCode(code) || !post || !output)return false;
  // String content is alphabetic/numeric/hyphen only, hence JSON-safe.
  std::string body=R"({"code":")"+std::string(code)+R"(","bot_code":")"+std::string(code)+R"("})";
  std::string response;
  const bool ok=post(kLuatoolsRedeemUrl,body,&response);
  std::fill(body.begin(),body.end(),char(0));
  if(!ok)return false;
  const bool parsed=ParseLuatoolsSession(response,output);
  std::fill(response.begin(),response.end(),char(0));
  return parsed;
}

inline constexpr std::string_view kLuatoolsRefreshUrl=
    "https://db.lua.tools/auth/v1/token?grant_type=refresh_token";
// Unlike bot codes, refresh tokens must be kept and rotated securely.
// Preserve the previous refresh token only if the server omits a replacement.
inline bool RefreshLuatoolsSession(const LuatoolsSession& prior,
                                   const LuatoolsPost& post, LuatoolsSession* result) {
  if(!result || !post || prior.refresh_token.empty() ||
     prior.refresh_token.size()>8192)return false;
  // Supabase refresh tokens are JSON strings; escape rather than interpolate raw input.
  std::string escaped;
  escaped.reserve(prior.refresh_token.size());
  for(unsigned char ch:prior.refresh_token){
    if(ch=='"' || ch=='\\')escaped.push_back('\\');
    if(ch<32 || ch>126)return false;
    escaped.push_back(static_cast<char>(ch));
  }
  std::string body=R"({"refresh_token":")"+escaped+R"("})";
  std::string response;
  const bool ok=post(kLuatoolsRefreshUrl,body,&response);
  std::fill(body.begin(),body.end(),char(0));
  std::fill(escaped.begin(),escaped.end(),char(0));
  if(!ok){std::fill(response.begin(),response.end(),char(0));return false;}
  LuatoolsSession renewed;
  const bool parsed=ParseLuatoolsSession(response,&renewed);
  std::fill(response.begin(),response.end(),char(0));
  if(!parsed)return false;
  if(renewed.refresh_token.empty())renewed.refresh_token=prior.refresh_token;
  *result=std::move(renewed);
  return true;
}
}
