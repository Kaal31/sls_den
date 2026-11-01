#pragma once
#include <cstddef>
#include <string>
#include <string_view>
namespace playden::sls {
bool InflateZipRaw(std::string_view compressed, std::size_t expected, std::string* output);
}
