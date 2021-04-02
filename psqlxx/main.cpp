#include <pqxx/pqxx>

#include <psqlxx/args.hpp>
#include <psqlxx/cli.hpp>
#include <psqlxx/db.hpp>


using namespace psqlxx;


namespace {

[[nodiscard]]
const auto buildOptions() {
    auto options = CreateBaseOptions();

    AddDbOptions(options);

    return options;
}

[[nodiscard]]
const auto handleOptions(cxxopts::Options &options, const int argc, char **argv) {
    const auto results = ParseOptions(options, argc, argv);
    if (not results) {
        exit(EXIT_FAILURE);
    }

    HandleBaseOptions(options, results.value());

    return HandleDbOptions(results.value());
}

constexpr auto toExitCode(const bool success) {
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

}


int main(int argc, char **argv) {
    auto options = buildOptions();

    const auto connection_options = handleOptions(options, argc, argv);

    const auto my_connection = MakeConnection(connection_options);
    if (not my_connection) {
        return EXIT_FAILURE;
    }

    if (connection_options.list_DBs_and_exit) {
        const auto list_dbs_sql = BuildListDBsSql();
        return toExitCode(DoTransaction(my_connection, list_dbs_sql.c_str()));
    }

    if (not connection_options.commands.empty()) {
        for (const auto &a_command : connection_options.commands) {
            if (not DoTransaction(my_connection, a_command.c_str())) {
                return EXIT_FAILURE;
            }
        }

        return EXIT_SUCCESS;
    }

    Cli my_cli{argv[0], CliOptions{}};
    my_cli.Config();
    my_cli.RegisterLineHandler([my_connection](const char *sql_cmd) {
        return DoTransaction(my_connection, sql_cmd);
    });
    my_cli.Run();

    return EXIT_SUCCESS;
}