/**
 * @file main.cpp
 * @author DrkWithT
 * @brief Implements driver logic for the TFTP server.
 * @date 2/05/2025
 */

#include <iostream>
#include <print>
#include <optional>
#include "mybsock/netconfig.hpp"
#include "mybsock/sockets.hpp"
#include "mytftpd/driver.hpp"

static constexpr auto expected_argc = 2;
static constexpr auto min_allowed_port = 1025;
static constexpr auto max_allowed_port = 65535;

int main(int argc, char* argv[]) {
    using namespace TftpServer;

    if (argc != expected_argc) {
        std::cerr << "Invalid argc.\nusage: ./tftpd <port no.>\n";
        return 1;
    }

    const auto checked_port_n = std::stoi(argv[1]);

    if (checked_port_n < min_allowed_port or checked_port_n > max_allowed_port) {
        std::print(std::cerr, "Invalid port arg: {}\n", checked_port_n);
        return 1;
    }

    auto make_socket = [] [[nodiscard]] (const char* port_cstr) -> MyBSock::UDPServerSocket {
        MyBSock::SocketGenerator generator {port_cstr};

        while (generator) {
            auto fd_opt = generator();

            if (fd_opt.has_value()) {
                return fd_opt.value();
            }
        }

        return {};
    };

    Driver::MyServer app {make_socket(argv[1])};
    
    if (not app.runService()) {
        std::print(std::cerr, "Setup failed: local address options unavailable.\n");
        return 1;
    }
}