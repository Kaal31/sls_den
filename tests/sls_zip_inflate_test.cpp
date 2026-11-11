// Portable ZIP/DEFLATE provider fixture. No network, secrets, or game files.
#include "sls/zip_manifest.h"
#include "sls/zip_inflate.h"
#include "sls/provider_discovery.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace playden::sls;
static void Check(bool ok,const char* why) {
  if(!ok){std::cerr<<"FAIL: "<<why<<"\n";std::exit(1);}
}
static void Put16(std::string& s,std::uint32_t v){
  s.push_back(char(v));s.push_back(char(v>>8));
}
static void Put32(std::string& s,std::uint32_t v){
  Put16(s,v);Put16(s,v>>16);
}
static std::string Hex(std::string_view hex) {
  auto digit=[](char c)->unsigned {
    return c>='0'&&c<='9'?unsigned(c-'0'):unsigned(c-'a'+10);
  };
  std::string out;
  for(std::size_t i=0;i+1<hex.size();i+=2)
    out.push_back(char((digit(hex[i])<<4)|digit(hex[i+1])));
  return out;
}
static std::string Archive(std::string name,std::string_view data,
                            std::string_view compressed,bool bad_crc=false) {
  const auto crc=zip_detail::crc32(data)+(bad_crc?1:0);
  std::string z,cd;
  Put32(z,0x04034b50);Put16(z,20);Put16(z,0);Put16(z,8);
  Put16(z,0);Put16(z,0);Put32(z,crc);
  Put32(z,compressed.size());Put32(z,data.size());
  Put16(z,name.size());Put16(z,0);
  z+=name;z.append(compressed);
  Put32(cd,0x02014b50);Put16(cd,20);Put16(cd,20);
  Put16(cd,0);Put16(cd,8);Put16(cd,0);Put16(cd,0);
  Put32(cd,crc);Put32(cd,compressed.size());Put32(cd,data.size());
  Put16(cd,name.size());Put16(cd,0);Put16(cd,0);Put16(cd,0);
  Put16(cd,0);Put32(cd,0);Put32(cd,0);cd+=name;
  auto start=z.size();z+=cd;
  Put32(z,0x06054b50);Put16(z,0);Put16(z,0);
  Put16(z,1);Put16(z,1);Put32(z,cd.size());
  Put32(z,start);Put16(z,0);
  return z;
}
int main() {
  constexpr std::uint32_t app=480;
  const std::string lua="addappid(480)\nsetManifestid(480, \"123456789\")\n";
  // Raw DEFLATE bytes generated with zlib level=9, wbits=-15.
  const auto compressed=Hex("4b4c49492c28c84cd130b130d0e42a4e2df14dcccb4c4b2d2e8108e92828191a199b989a995b582a69720100");
  std::string manifest,error;
  const auto valid=Archive("480.lua",lua,compressed);
  Check(!DecodeStoredZipManifest(valid,app,&manifest,&error),
        "stored-only compatibility API rejects deflate");
  Check(DecodeZipManifest(valid,app,&manifest,&error,InflateZipRaw)&&manifest==lua,
        "raw DEFLATE ZIP validates and yields matching Lua");
  Check(!DecodeZipManifest(Archive("480.lua",lua,compressed,true),app,
                           &manifest,&error,InflateZipRaw),
        "CRC mismatch rejected even when inflate succeeds");
  Check(!DecodeZipManifest(Archive("../480.lua",lua,compressed),app,
                           &manifest,&error,InflateZipRaw),
        "traversal filename rejected");
  Check(!DecodeZipManifest(Archive("481.lua",lua,compressed),app,
                           &manifest,&error,InflateZipRaw),
        "wrong AppID archive rejected");
  Check(!DecodeZipManifest(valid.substr(0,valid.size()-2),app,
                           &manifest,&error,InflateZipRaw),
        "truncated ZIP rejected");
  Check(!DecodeZipManifest(valid,app,&manifest,&error,
        [](std::string_view,std::size_t,std::string* out){
          *out="incorrect size";return true;
        }),"inflater must return exact declared byte count");
  ProviderSelection selected;
  const auto result=DiscoverManifestWithCredentials(app,selected,{},
    [&](const std::string& provider,const std::string&,std::uint32_t,
        std::string_view,std::string* payload,std::string*) {
      if(provider!="sushi")return false;
      *payload=valid;return true;
    },
    [](std::string_view zip,std::uint32_t id,std::string* out,std::string* why) {
      return DecodeZipManifest(zip,id,out,why,InflateZipRaw);
    });
  Check(result.ok&&result.source_id=="sushi"&&result.metadata.depots.size()==1&&
        result.metadata.depots[0].manifest_gid==123456789,
        "SLS fallback accepts real DEFLATE manifest and exposes depot metadata");
  std::cout<<"PASS: portable SLS ZIP DEFLATE, integrity checks and source fallback\n";
}
