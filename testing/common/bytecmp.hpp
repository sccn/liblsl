#pragma once
#include <string>

/// convert a binary string to the equivalent C-escaped string, e.g. "Hello\xaf\xbcWorld\0"
std::string bytes_to_hexstr(const std::string &str);
/// compare two binary strings, printing the location around the first mismatch
void cmp_binstr(const std::string &a, const std::string b);
