#include "client/client.h"
#include "client/cli_parser.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: kv_client <host> <port>" << std::endl;
        return 1;
    }

    KvClient client;
    if (!client.Connect(argv[1], static_cast<uint16_t>(std::stoi(argv[2])))) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }

    std::cout << "Connected. Type 'exit' to quit." << std::endl;

    CliParser parser;
    bool running = true;
    std::string line;
    while (running && std::cout << "> ", std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") {
            running = false;
            break;
        }

        auto cmd = parser.Parse(line);
        if (!cmd) {
            continue;
        }

        if (cmd->type == CommandType::UNKNOWN) {
            if (cmd->error == ParseError::UNKNOWN_COMMAND) {
                std::cout << "ERR unknown command" << std::endl;
            } else {
                std::cout << "ERR wrong number of arguments" << std::endl;
            }
            continue;
        }

        switch (cmd->type) {
            case CommandType::SET:
                std::cout << (client.Set(cmd->args[0], cmd->args[1]) ? "OK" : "ERR") << std::endl;
                break;

            case CommandType::GET: {
                auto val = client.Get(cmd->args[0]);
                if (val) {
                    std::cout << *val << std::endl;
                } else {
                    std::cout << "(nil)" << std::endl;
                }
                break;
            }

            case CommandType::DEL:
                std::cout << (client.Delete(cmd->args[0]) ? "1" : "0") << std::endl;
                break;

            case CommandType::EXISTS:
                std::cout << (client.Exists(cmd->args[0]) ? "exists" : "not found") << std::endl;
                break;

            case CommandType::KEYS:
                std::cout << "total keys: " << client.Keys() << std::endl;
                break;

            default:
                std::cout << "ERR unknown command" << std::endl;
                break;
        }
    }

    client.Disconnect();
    return 0;
}
