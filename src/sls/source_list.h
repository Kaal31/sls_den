#pragma once
#include "sls/source_catalog.h"
#include <string>
#include <string_view>
#include <vector>

namespace playden::sls {
// Human-editable provider configuration, one TSV record per source:
// id TAB display name TAB HTTPS URL template TAB 0|1 enabled.
// Credentials deliberately are not stored in this format.
bool ParseSourceList(std::string_view input, std::vector<Source>* sources,
                     std::string* reason = nullptr);
std::string SerializeSourceList(const std::vector<Source>& sources);
}
