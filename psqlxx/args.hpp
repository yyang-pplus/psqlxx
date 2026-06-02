#pragma once

#include <optional>

#include <cxxopts.hpp>


namespace psqlxx {

[[nodiscard]] cxxopts::Options CreateBaseOptions();

[[nodiscard]] std::optional<cxxopts::ParseResult>
ParseOptions(cxxopts::Options &options, int argc, char **argv) noexcept;

void HandleBaseOptions(const cxxopts::Options &options,
                       const cxxopts::ParseResult &parsed_options);

} //namespace psqlxx
