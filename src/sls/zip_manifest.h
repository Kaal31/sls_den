#pragma once
// Portable, bounded ZIP manifest reader. Does not extract files to disk.
// This initial implementation deliberately supports method 0 (stored) only.
#include <cstdint>
#include <string>
#include <string_view>
#include <set>

namespace playden::sls {
namespace zip_detail {
inline bool u16(std::string_view s, size_t p, uint16_t* v) {
  if (p > s.size() || s.size()-p < 2) return false;
  *v = uint16_t(uint8_t(s[p])) | (uint16_t(uint8_t(s[p+1])) << 8);
  return true;
}
inline bool u32(std::string_view s, size_t p, uint32_t* v) {
  uint16_t a,b;
  if (!u16(s,p,&a) || !u16(s,p+2,&b)) return false;
  *v = uint32_t(a) | (uint32_t(b)<<16);
  return true;
}
inline uint32_t crc32(std::string_view s) {
  uint32_t crc=0xffffffffu;
  for (unsigned char c : s) {
    crc ^= c;
    for(int i=0;i<8;++i) crc=(crc>>1) ^ (0xedb88320u & (0u-(crc&1u)));
  }
  return ~crc;
}
inline bool safe_name(std::string_view name) {
  if(name.empty() || name.size()>240 || name.front()=='/' || name.front()=='\\' ||
     name.find('\\')!=name.npos || name.find(':')!=name.npos ||
     name.find('\0')!=name.npos) return false;
  size_t i=0;
  while(i<name.size()) {
    auto end=name.find('/',i);
    if(end==name.npos) end=name.size();
    auto component=name.substr(i,end-i);
    if(component.empty() || component=="." || component=="..") return false;
    i=end+1;
  }
  return true;
}
inline bool target(std::string_view name,uint32_t appid) {
  auto slash=name.rfind('/');
  auto file=slash==name.npos ? name : name.substr(slash+1);
  return file==std::to_string(appid)+".lua";
}
}
inline bool DecodeStoredZipManifest(std::string_view zip, uint32_t appid,
                                    std::string* output, std::string* error) {
  auto fail=[&](const char* why){ if(error)*error=why; return false; };
  if(!output || !appid || zip.size()>1024*1024 || zip.size()<22)
    return fail("invalid archive size");
  // Locate EOCD within max ZIP comment length, and require no trailing bytes.
  size_t eocd=zip.npos;
  for(size_t p=zip.size()-22;;--p) {
    uint32_t sig; uint16_t comment;
    if(zip_detail::u32(zip,p,&sig) && sig==0x06054b50u &&
       zip_detail::u16(zip,p+20,&comment) && size_t(comment)==zip.size()-p-22) {
      eocd=p;break;
    }
    if(p==0 || zip.size()-p>65557)break;
  }
  if(eocd==zip.npos) return fail("missing central directory");
  uint16_t disk,cd_disk,on_disk,entries;
  uint32_t cd_size,cd_offset;
  if(!zip_detail::u16(zip,eocd+4,&disk)||!zip_detail::u16(zip,eocd+6,&cd_disk)||
     !zip_detail::u16(zip,eocd+8,&on_disk)||!zip_detail::u16(zip,eocd+10,&entries)||
     !zip_detail::u32(zip,eocd+12,&cd_size)||!zip_detail::u32(zip,eocd+16,&cd_offset)||
     disk || cd_disk || entries!=on_disk || entries==0 || entries>128 ||
     entries==65535 || cd_size==0 || cd_offset>eocd ||
     cd_size>eocd-cd_offset) return fail("invalid central directory");
  size_t p=cd_offset;
  std::set<std::string> names;
  bool found=false;
  std::string selected;
  for(unsigned n=0;n<entries;++n) {
    uint32_t sig,crc,compressed,uncompressed,local;
    uint16_t flags,method,name_len,extra_len,comment_len,disk_start;
    if(p>eocd || eocd-p<46 ||
       !zip_detail::u32(zip,p,&sig)||sig!=0x02014b50u ||
       !zip_detail::u16(zip,p+8,&flags)||!zip_detail::u16(zip,p+10,&method)||
       !zip_detail::u32(zip,p+16,&crc)||
       !zip_detail::u32(zip,p+20,&compressed)||!zip_detail::u32(zip,p+24,&uncompressed)||
       !zip_detail::u16(zip,p+28,&name_len)||!zip_detail::u16(zip,p+30,&extra_len)||
       !zip_detail::u16(zip,p+32,&comment_len)||!zip_detail::u16(zip,p+34,&disk_start)||
       !zip_detail::u32(zip,p+42,&local))return fail("malformed central entry");
    size_t length=size_t(46)+name_len+extra_len+comment_len;
    if(length>eocd-p || disk_start || (flags&1u) || (flags&8u) ||
       (flags&0x40u) || compressed>1024*1024 || uncompressed>1024*1024 ||
       !name_len) return fail("unsupported or oversized entry");
    auto name=zip.substr(p+46,name_len);
    if(!zip_detail::safe_name(name) || !names.insert(std::string(name)).second)
      return fail("unsafe or duplicate filename");
    if(zip_detail::target(name,appid)) {
      if(found || method!=0 || compressed!=uncompressed ||
         local>zip.size() || zip.size()-local<30)
        return fail("unsupported or duplicate manifest");
      uint32_t local_sig,local_crc,local_c,local_u;
      uint16_t local_flags,local_method,local_name,local_extra;
      if(!zip_detail::u32(zip,local,&local_sig)||local_sig!=0x04034b50u ||
         !zip_detail::u16(zip,local+6,&local_flags)||
         !zip_detail::u16(zip,local+8,&local_method)||
         !zip_detail::u32(zip,local+14,&local_crc)||
         !zip_detail::u32(zip,local+18,&local_c)||
         !zip_detail::u32(zip,local+22,&local_u)||
         !zip_detail::u16(zip,local+26,&local_name)||
         !zip_detail::u16(zip,local+28,&local_extra))
        return fail("malformed local header");
      size_t data= size_t(local)+30+local_name+local_extra;
      if(local_flags!=flags || local_method!=method || local_crc!=crc ||
         local_c!=compressed || local_u!=uncompressed ||
         local_name!=name_len || data>cd_offset ||
         compressed>cd_offset-data ||
         zip.substr(local+30,local_name)!=name)
        return fail("inconsistent local entry");
      auto bytes=zip.substr(data,compressed);
      if(zip_detail::crc32(bytes)!=crc) return fail("CRC mismatch");
      selected.assign(bytes.data(),bytes.size());
      found=true;
    }
    p+=length;
  }
  if(p!=size_t(cd_offset)+cd_size) return fail("central directory size mismatch");
  if(!found) return fail("manifest not found");
  *output=std::move(selected);
  if(error)error->clear();
  return true;
}
} // namespace playden::sls
