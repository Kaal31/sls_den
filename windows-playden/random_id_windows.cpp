#include "platform/random_id.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#pragma comment(lib, "bcrypt.lib")
namespace playden {
std::optional<std::string> RandomUuid() {
  unsigned char b[16]{};
  if (BCryptGenRandom(nullptr,b,sizeof(b),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return std::nullopt;
  b[6]=(b[6]&0x0f)|0x40; b[8]=(b[8]&0x3f)|0x80;
  char text[37];
  std::snprintf(text,sizeof(text),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
  return std::string(text);
}
}
