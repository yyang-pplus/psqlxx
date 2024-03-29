#pragma once

#include <iostream>
#include <unordered_map>


namespace cxxopts {

class Options;
class ParseResult;

}

namespace pqxx {

class result;

}


namespace psqlxx {

using TypeMap = std::unordered_map<int, std::string>;

struct FormatterOptions {
    std::string out_file;

    std::string delimiter;
    std::string special_chars;

    bool show_title_and_summary = true;
    bool no_align = false;
};

void AddFormatOptions(cxxopts::Options &options);

[[nodiscard]]
FormatterOptions HandleFormatOptions(const cxxopts::ParseResult &parsed_options);


void PrintResult(const pqxx::result &a_result, const FormatterOptions &options,
                 const TypeMap &type_map, std::ostream &out,
                 const std::string_view title);

}//namespace psqlxx
