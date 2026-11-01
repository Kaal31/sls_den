#include "sls/zip_inflate.h"
#include <climits>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_SUPPORT_ZLIB
#define STBI_NO_STDIO
#define STBI_NO_JPEG
#define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_LINEAR
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb_image.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
namespace playden::sls {
bool InflateZipRaw(std::string_view compressed, size_t expected, std::string* out) {
  if(!out || expected==0 || expected>1024*1024 || compressed.empty() ||
     compressed.size()>INT_MAX || expected>INT_MAX) return false;
  std::string decoded(expected,'\0');
  const int written=stbi_zlib_decode_noheader_buffer(
    decoded.data(), static_cast<int>(decoded.size()),
    compressed.data(), static_cast<int>(compressed.size()));
  if(written<0 || static_cast<size_t>(written)!=expected) return false;
  *out=std::move(decoded);
  return true;
}
}
