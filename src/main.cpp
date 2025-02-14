/**
 * @file main.cpp
 * @author DrkWithT
 * @brief Implements driver logic for the TFTP server.
 * @date 2/05/2025
 */

// TODO: re-implement driver.

#include <iostream>
#include <print>
// #include <array>
// #include <string>
// #include <atomic>
// #include <thread>
// #include <fstream>
// #include "mybsock/netconfig.hpp"
// #include "mybsock/buffers.hpp"
// #include "mybsock/sockets.hpp"
// #include "mytftp/types.hpp"
// #include "mytftp/messaging.hpp"

static constexpr auto expected_argc = 2;


int main(int argc, char* argv[]) {
    if (argc != expected_argc) {
        std::cerr << "Invalid argc.\nusage: ./tftpd <port no.>\n";
        return 1;
    }
}