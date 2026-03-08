#pragma once

#include <cstdint>
#include <string_view>

namespace mini1 {

struct CsvParse {
    static std::string_view trim_sv(std::string_view sv);
    static bool try_parse_int32_sv(std::string_view sv, int32_t& value);
    static bool try_parse_double_sv(std::string_view sv, double& value);
    static std::string_view next_field(const char*& pos, const char* end);
    static std::string_view strip_quotes(std::string_view sv);
};

} // namespace mini1
