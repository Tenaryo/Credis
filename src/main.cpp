#include "cli/cli_parser.hpp"
#include "server/server_runner.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    AppConfig config;
    config.port = parse_port(argc, argv);
    config.server_config.replicaof = parse_replicaof(argc, argv);
    config.server_config.dir = parse_dir(argc, argv);
    config.server_config.dbfilename = parse_dbfilename(argc, argv);

    auto app = RedisApp::create(config);
    if (!app) {
        std::cerr << app.error() << '\n';
        return 1;
    }
    return app->run();
}
