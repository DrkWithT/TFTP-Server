#pragma once

#include <optional>
#include <netdb.h>

namespace TftpServer::MyBSock {
    class SocketGenerator {
    public:
        SocketGenerator() = delete;
        SocketGenerator(const char* port_cstr);
        ~SocketGenerator();

        explicit operator bool() const noexcept;
        std::optional<int> operator()();

    private:
        [[nodiscard]] bool hasNextOpt() const noexcept;

        addrinfo* m_head;
        addrinfo* m_cursor;
    };
}