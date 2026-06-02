#include <psqlxx/formatter.hpp>

#include <cxxopts.hpp>
#include <pqxx/pqxx>


namespace {

struct ColumnInfo {
    std::size_t width = 0;
    bool is_numeric = false;
};

inline auto &printSummary(std::ostream &out, const std::size_t size) {
    out << "(" << size << " row";
    if (size > 1) {
        out << 's';
    }
    return out << ")";
}

auto &printStrInCenter(std::ostream &out, const std::string_view a_field, const int width) {
    const int padding = width - a_field.size();
    const auto half_spaces = std::string(padding > 1 ? padding / 2 : 0, ' ');
    out << half_spaces << a_field << half_spaces;
    if (padding > 0 and padding % 2 != 0) {
        out << ' ';
    }

    return out;
}

auto &printStrLeft(std::ostream &out, const std::string_view a_field, const int width) {
    const int padding = width - a_field.size() - 1;
    const auto spaces = std::string(padding > 0 ? padding : 0, ' ');
    if (padding > 0) {
        out << ' ';
    }
    return out << a_field << spaces;
}

auto &printStrRight(std::ostream &out, const std::string_view a_field, const int width) {
    const int padding = width - a_field.size() - 1;
    const auto spaces = std::string(padding > 0 ? padding : 0, ' ');
    out << spaces << a_field;
    if (padding > 0) {
        out << ' ';
    }

    return out;
}

void printHeaders(std::ostream &out,
                  const pqxx::result &a_result,
                  const std::vector<ColumnInfo> &column_infos,
                  const std::string_view delimiter) {
    for (std::size_t i = 0; i < column_infos.size() - 1; ++i) {
        printStrInCenter(out, a_result.column_name(i), column_infos[i].width) << delimiter;
    }
    printStrInCenter(out, a_result.column_name(a_result.columns() - 1), column_infos.back().width)
        << '\n';
}

auto &printField(std::ostream &out,
                 const std::string_view a_field,
                 const std::string_view special_chars,
                 const ColumnInfo &column_info) {
    const auto do_quote = (not special_chars.empty()) and
                          (a_field.find_first_of(special_chars) != std::string_view::npos);
    if (do_quote) {
        out << '"';
    }

    if (column_info.is_numeric) {
        printStrRight(out, a_field, column_info.width);
    } else {
        printStrLeft(out, a_field, column_info.width);
    }

    if (do_quote) {
        out << '"';
    }

    return out;
}

inline auto &printFieldBar(std::ostream &out, const int width) {
    std::string bar(width, '-');
    return out << bar;
}

[[nodiscard]] auto isNumeric(const psqlxx::TypeMap &type_map, const pqxx::oid oid) {
    static const std::unordered_set<std::string_view> numeric_type_names {
        "int8", "int2", "int4", "oid", "tid", "xid", "cid", "float4", "float4"};

    const auto type_iter = type_map.find(oid);
    if (type_iter != type_map.cend()) {
        return numeric_type_names.find(type_iter->second) != numeric_type_names.cend();
    }

    return false;
}

[[nodiscard]] auto getColumnInfos(const pqxx::result &a_result,
                                  const psqlxx::TypeMap &type_map,
                                  const bool no_align) {
    std::vector<ColumnInfo> column_infos(a_result.columns());

    for (std::size_t i = 0; i < column_infos.size(); ++i) {
        column_infos[i].is_numeric = isNumeric(type_map, a_result.column_type(i));
    }

    if (no_align) {
        return column_infos;
    }

    for (std::size_t i = 0; i < column_infos.size(); ++i) {
        column_infos[i].width = std::strlen(a_result.column_name(i));
    }

    for (const auto &row : a_result) {
        for (std::size_t i = 0; i < column_infos.size(); ++i) {
            column_infos[i].width = std::max(column_infos[i].width, row[i].view().size());
        }
    }

    for (auto &info : column_infos) {
        info.width += 2;
    }

    return column_infos;
}

} // namespace


namespace psqlxx {

void AddFormatOptions(cxxopts::Options &options) {
    options.add_options("Formatter")("A,no-align",
                                     "unaligned table output mode",
                                     cxxopts::value<bool>()->default_value("false"))(
        "csv",
        "CSV (Comma-Separated Values) table output mode",
        cxxopts::value<bool>()->default_value("false"))

        ("F,field-separator",
         "field separator for unaligned output",
         cxxopts::value<std::string>()->default_value("|"))

            ("o,out-file",
             "send query results to file",
             cxxopts::value<std::string>()->default_value(""));
}

FormatterOptions HandleFormatOptions(const cxxopts::ParseResult &parsed_options) {
    FormatterOptions options {};

    options.out_file = parsed_options["out-file"].as<std::string>();

    if (parsed_options["csv"].as<bool>()) {
        options.delimiter = ",";
        options.special_chars = options.delimiter + "\n\r";
        options.show_title_and_summary = false;
        options.no_align = true;
    } else {
        options.delimiter = parsed_options["field-separator"].as<std::string>();
        options.no_align = parsed_options["no-align"].as<bool>();
    }

    return options;
}

void PrintResult(const pqxx::result &a_result,
                 const FormatterOptions &options,
                 const TypeMap &type_map,
                 std::ostream &out,
                 const std::string_view title) {
    if (a_result.columns() > 0) {
        const auto column_infos = getColumnInfos(a_result, type_map, options.no_align);

        if (options.show_title_and_summary and (not title.empty())) {
            const auto total_width = std::accumulate(column_infos.cbegin(),
                                                     column_infos.cend(),
                                                     0,
                                                     [](const auto init, const auto &info) {
                                                         return init + info.width;
                                                     });
            printStrInCenter(out, title, total_width) << '\n';
        }

        printHeaders(out, a_result, column_infos, options.delimiter);

        if (not options.no_align) {
            for (std::size_t i = 0; i < column_infos.size() - 1; ++i) {
                printFieldBar(out, column_infos[i].width) << '+';
            }
            printFieldBar(out, column_infos.back().width) << '\n';
        }

        for (const auto &row : a_result) {
            for (std::size_t i = 0; i < column_infos.size() - 1; ++i) {
                printField(out, row[i].view(), options.special_chars, column_infos[i])
                    << options.delimiter;
            }
            printField(out, row.back().view(), options.special_chars, column_infos.back())
                << '\n';
        }

        if (options.show_title_and_summary) {
            printSummary(out, a_result.size()) << std::endl;
        }
        if (not options.no_align) {
            out << std::endl;
        }
    }
}

} //namespace psqlxx
