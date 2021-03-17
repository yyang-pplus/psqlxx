#pragma once

#include <memory>


namespace cxxopts {

class Options;
class ParseResult;

}

namespace pqxx {

class connection;

}


namespace postgres_client {

void DoTransaction(const std::shared_ptr<pqxx::connection> connection_ptr,
                   const char *sql_cmd);

void CreatePqOptions(cxxopts::Options &options);

const std::string HandlePqOptions(const cxxopts::ParseResult &parsed_options);

}//namespace postgres_client
