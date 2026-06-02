#include <psqlxx/db.hpp>
#include <psqlxx/string_utils.hpp>

#include <unistd.h>

#include <iostream>
#include <unordered_map>

#include <cxxopts.hpp>
#include <pqxx/pqxx>


using namespace psqlxx;


namespace {

[[nodiscard]] inline std::string_view getTransactionName() {
    return "psqlxx";
}

[[nodiscard]] inline auto overridePasswordFromPrompt(std::string connection_string) {
    const auto password = getpass("Password: ");
    return internal::overridePassword(std::move(connection_string), password);
}

[[nodiscard]] inline auto concatenateKeyValue(std::string key, std::string value) {
    return Joiner {'='}(std::move(key), std::move(value));
}

[[nodiscard]] inline auto
doTransaction(const DbProxy &proxy, const char **words, const int word_count) {
    std::stringstream query;
    for (int i = 0; i < word_count; ++i) {
        query << words[i] << " ";
    }

    return ToCommandResult(proxy.DoTransaction(query.str()));
}

[[nodiscard]] inline std::string_view buildListDBsSql() {
    // psql -E -l
    return R"(
SELECT
  d.datname as "Name",
  pg_catalog.pg_get_userbyid(d.datdba) as "Owner",
  pg_catalog.pg_encoding_to_char(d.encoding) as "Encoding",
  CASE d.datlocprovider WHEN 'c' THEN 'libc' WHEN 'i' THEN 'icu' END AS "Locale Provider",
  d.datcollate as "Collate",
  d.datctype as "Ctype",
  d.daticulocale as "ICU Locale",
  d.daticurules as "ICU Rules",
  pg_catalog.array_to_string(d.datacl, E'\n') AS "Access privileges"
FROM pg_catalog.pg_database d
ORDER BY 1;
)";
}

[[nodiscard]] inline std::string_view buildListSchemasSql() {
    return R"(
SELECT n.nspname AS "Name",
  pg_catalog.pg_get_userbyid(n.nspowner) AS "Owner"
FROM pg_catalog.pg_namespace n
WHERE n.nspname !~ '^pg_' AND n.nspname <> 'information_schema'
ORDER BY 1;
)";
}

[[nodiscard]] bool listSchemas(const DbProxy &db_proxy) {
    const auto list_schemas_sql = buildListSchemasSql();
    return db_proxy.DoTransaction(list_schemas_sql, [&db_proxy](const auto &a_result) {
        db_proxy.PrintResult(a_result, "List of schemas");
    });
}

[[nodiscard]] inline std::string_view buildListRolesSql() {
    return R"(
SELECT r.rolname, r.rolsuper, r.rolinherit,
  r.rolcreaterole, r.rolcreatedb, r.rolcanlogin,
  r.rolconnlimit, r.rolvaliduntil,
  ARRAY(SELECT b.rolname
        FROM pg_catalog.pg_auth_members m
        JOIN pg_catalog.pg_roles b ON (m.roleid = b.oid)
        WHERE m.member = r.oid) as memberof,
  r.rolreplication,
  r.rolbypassrls
FROM pg_catalog.pg_roles r
WHERE r.rolname !~ '^pg_'
ORDER BY 1;
)";
}

[[nodiscard]] bool listRoles(const DbProxy &db_proxy) {
    const auto list_roles_sql = buildListRolesSql();
    return db_proxy.DoTransaction(list_roles_sql, [&db_proxy](const auto &a_result) {
        db_proxy.PrintResult(a_result, "List of roles");
    });
}

void addConnectionOptions(cxxopts::Options &options) {
    options.add_options("DB Connection")(
        "S,connection-string",
        "PQ connection string. Refer to the libpq connect call for a complete definition of what may go into the connect string. By default the client will try to connect to a server running on the local machine.",
        cxxopts::value<std::string>()->default_value(""))(
        "w,no-password",
        "never prompt for password",
        cxxopts::value<bool>()->default_value("false"));
}

[[nodiscard]] auto handleConnectionOptions(const cxxopts::ParseResult &parsed_options) {
    ConnectionOptions options {};

    options.base_connection_string = parsed_options["connection-string"].as<std::string>();
    options.prompt_for_password = not parsed_options["no-password"].as<bool>();

    return options;
}

} // namespace


namespace psqlxx {

namespace internal {

std::string overridePassword(std::string connection_string, std::string password) {
    const auto was_connection_str_empty = connection_string.empty();

    if (StartsWith(connection_string, "postgresql://") or
        StartsWith(connection_string, "postgres://")) {
        if (connection_string.rfind('?') == std::string::npos) {
            connection_string.push_back('?');
        } else {
            connection_string.push_back('&');
        }
    } else {
        if (not was_connection_str_empty) {
            connection_string.push_back(' ');
        }
    }

    return connection_string + ComposeDbParameter(DbParameterKey::password, std::move(password));
}

std::unique_ptr<pqxx::connection> makeConnection(const ConnectionOptions &options) {
    for (bool original_tried = false; true; original_tried = true) {
        try {
            if (original_tried and options.prompt_for_password) {
                const auto connection_string =
                    overridePasswordFromPrompt(options.base_connection_string);
                return std::make_unique<pqxx::connection>(connection_string);
            } else {
                return std::make_unique<pqxx::connection>(options.base_connection_string);
            }
        } catch (const pqxx::broken_connection &e) {
            if (original_tried or not strstr(e.what(), "no password supplied")) {
                std::cerr << e.what() << std::endl;
                break;
            }
        }
    }

    return {};
}

} //namespace internal

DbProxy::DbProxy(DbProxyOptions options) :
    m_options(std::move(options)), m_out(std::cout.rdbuf()) {

    connect();

    if (not m_options.format_options.out_file.empty()) {
        m_out_file.open(m_options.format_options.out_file, std::ofstream::out);
        if (m_out_file) {
            m_out.rdbuf(m_out_file.rdbuf());
        } else {
            std::cerr << "Failed to open out file '" << m_options.format_options.out_file
                      << "': " << strerror(errno) << std::endl;
        }
    }
}

void DbProxy::connect() {
    m_connection = internal::makeConnection(m_options.connection_options);
    if (m_connection) {
        initTypeMap();
    }
}

bool DbProxy::PrintConnectionInfo() const {
    if (not m_connection)
        return false;

    m_out << "You are connected to database \"" << m_connection->dbname() << "\" as user \""
          << m_connection->username() << "\" at port \""
          << m_connection->port_number().value_or(0) << "\"." << std::endl;

    return true;
}

std::string DbProxy::GetDbName() const {
    if (m_connection)
        return m_connection->dbname();
    return "";
}

void DbProxy::initTypeMap() {
    if (not DoTransaction("select typname, oid from pg_type;",
                          [this](const pqxx::result &a_result) {
                              for (const auto &row : a_result) {
                                  auto [type_name, oid] = row.as<std::string, int>();
                                  m_pg_type_map[oid] = std::move(type_name);
                              }
                          })) {
        std::cerr << "Failed to query pg_type from DB." << std::endl;
    }
}

void DbProxy::PrintResult(const pqxx::result &a_result, const std::string_view title) const {
    psqlxx::PrintResult(a_result, m_options.format_options, m_pg_type_map, m_out, title);
}

bool DbProxy::DoTransaction(const std::string_view sql_cmd, const ResultHandler handler) const {
    assert(*this);

    return pqxx::perform([this, sql_cmd, handler] {
        try {
            pqxx::work a_work(*(m_connection), getTransactionName());
            const auto a_result = a_work.exec(sql_cmd);

            if (handler) {
                handler(a_result);
            } else {
                PrintResult(a_result);
            }
            return true;

        } catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    });
}

void AddDbProxyOptions(cxxopts::Options &options) {
    addConnectionOptions(options);

    options.add_options("Command")("l,list-dbs",
                                   "list available databases, then exit",
                                   cxxopts::value<bool>()->default_value("false"))(
        "c,command",
        "run only single command (SQL or internal) and exit",
        cxxopts::value<std::vector<std::string>>(),
        "COMMAND")("f,command-file",
                   "execute commands from file, then exit",
                   cxxopts::value<std::string>()->default_value(""));

    AddFormatOptions(options);
}

DbProxyOptions HandleDbProxyOptions(const cxxopts::ParseResult &parsed_options) {
    DbProxyOptions options {handleConnectionOptions(parsed_options),
                            HandleFormatOptions(parsed_options)};

    options.list_DBs_and_exit = parsed_options["list-dbs"].as<bool>();

    if (parsed_options.count("command")) {
        options.commands = parsed_options["command"].as<std::vector<std::string>>();
    }

    options.command_file = parsed_options["command-file"].as<std::string>();

    return options;
}

std::string ComposeDbParameter(const DbParameterKey key_enum, std::string value) {
    const static std::unordered_map<DbParameterKey, std::string> DB_PARAMETER_KEY_MAP {
        {DbParameterKey::host, "host"},
        {DbParameterKey::port, "port"},
        {DbParameterKey::dbname, "dbname"},
        {DbParameterKey::user, "user"},
        {DbParameterKey::password, "password"},
    };

    return concatenateKeyValue(DB_PARAMETER_KEY_MAP.at(key_enum), std::move(value));
}

bool ListDbs(const DbProxy &db_proxy) {
    const auto list_dbs_sql = buildListDBsSql();
    return db_proxy.DoTransaction(list_dbs_sql, [&db_proxy](const auto &a_result) {
        db_proxy.PrintResult(a_result, "List of databases");
    });
}

CommandGroup CreatePsqlxxCommandGroup(const DbProxy &proxy) {
    CommandGroup group {"psqlxx", "psqlxx commands"};

    group.AddOptions()(
        {""},
        {VARIADIC_ARGUMENT},
        [&proxy](const auto words, const auto word_count) {
            return doTransaction(proxy, words, word_count);
        },
        "To execute query")(
        {"@l"},
        {},
        [&proxy](const auto, const auto) {
            return ToCommandResult(ListDbs(proxy));
        },
        "List databases")(
        {"@du"},
        {},
        [&proxy](const auto, const auto) {
            return ToCommandResult(listRoles(proxy));
        },
        "List roles")(
        {"@dn"},
        {},
        [&proxy](const auto, const auto) {
            return ToCommandResult(listSchemas(proxy));
        },
        "List schemas")(
        {"@conninfo"},
        {},
        [&proxy](const auto, const auto) {
            return ToCommandResult(proxy.PrintConnectionInfo());
        },
        "Display information about current connection");

    return group;
}

} //namespace psqlxx
