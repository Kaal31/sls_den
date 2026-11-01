#include "sls/luatools_auth.h"
#include <cassert>
#include <string>
int main() {
  using namespace playden::sls;
  assert(ValidBotCode("abc-123"));
  assert(!ValidBotCode(""));
  assert(!ValidBotCode("abc\nsecret"));
  assert(!ValidBotCode(std::string(129,'X')));
  LuatoolsSession session;
  int called=0;
  const bool accepted=RedeemLuatoolsCode("ABCD-1234",
    [&](std::string_view url,std::string_view body,std::string* result) {
      ++called;
      assert(url==kLuatoolsRedeemUrl);
      assert(body.find("ABCD-1234")!=std::string_view::npos);
      *result=R"({"session":{"access_token":"test-access","refresh_token":"test-refresh","expires_in":3600}})";
      return true;
    },&session);
  assert(accepted && called==1);
  assert(session.access_token=="test-access" && session.refresh_token=="test-refresh");
  assert(session.expires_in==3600);
  assert(!RedeemLuatoolsCode("x\"y",
    [&](std::string_view,std::string_view,std::string*){++called;return true;},&session));
  assert(called==1);
  assert(!ParseLuatoolsSession(R"({"session":{"access_token":""}})",&session));
  assert(!ParseLuatoolsSession(R"({"error":"invalid"})",&session));
  assert(!ParseLuatoolsSession(R"({"session":{"access_token":"a","refresh_token":42}})",&session));
  assert(ParseLuatoolsSession(R"({"access_token":"a"})",&session));
  LuatoolsSession prior{"previous-access","previous-refresh",100};
  int refresh_calls=0;
  LuatoolsSession renewed;
  assert(RefreshLuatoolsSession(prior,
    [&](std::string_view url,std::string_view body,std::string* response) {
      ++refresh_calls;
      assert(url==kLuatoolsRefreshUrl);
      assert(body==R"({"refresh_token":"previous-refresh"})");
      *response=R"({"access_token":"next-access","refresh_token":"rotated-refresh","expires_in":3600})";
      return true;
    },&renewed));
  assert(refresh_calls==1 && renewed.access_token=="next-access");
  assert(renewed.refresh_token=="rotated-refresh" && renewed.expires_in==3600);
  assert(RefreshLuatoolsSession(prior,
    [&](std::string_view,std::string_view,std::string* response) {
      *response=R"({"access_token":"new-access"})";
      return true;
    },&renewed));
  assert(renewed.refresh_token=="previous-refresh");
  assert(!RefreshLuatoolsSession(LuatoolsSession{"access","",100},
    [&](std::string_view,std::string_view,std::string*){++refresh_calls;return true;},&renewed));
  assert(refresh_calls==1);
  assert(!RefreshLuatoolsSession(prior,
    [&](std::string_view,std::string_view,std::string* response) {
      *response=R"({"error":"expired refresh token"})";
      return true;
    },&renewed));
  assert(renewed.access_token=="new-access");

}
