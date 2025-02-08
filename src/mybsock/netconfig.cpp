#include <stdexcept>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "mybsock/netconfig.hpp"

namespace TftpServer::MyBSock {
    static constexpr const char* default_port_cstr = "8080";
    static constexpr auto getaddrinfo_ok = 0;
    static constexpr auto socket_fd_dud = -1;

    SocketGenerator::SocketGenerator(const char* port_cstr)
    : m_head {nullptr}, m_cursor {nullptr} {
        const auto* checked_port_cstr = (port_cstr != nullptr) ? port_cstr : default_port_cstr;
 
        addrinfo udp_config;
        std::memset(&udp_config, 0, sizeof(udp_config));
        udp_config.ai_family = AF_INET;
        udp_config.ai_socktype = SOCK_DGRAM;
        udp_config.ai_flags = AI_PASSIVE;
    
        if (const auto result = getaddrinfo(nullptr, checked_port_cstr, &udp_config, &m_head); result != getaddrinfo_ok) {
            throw std::logic_error {gai_strerror(result)};
        }
    }

    SocketGenerator::~SocketGenerator() {
        if (m_head != nullptr) {
            freeaddrinfo(m_head);
            m_head = nullptr;
        }
    }

    SocketGenerator::operator bool() const noexcept {
        return m_cursor != nullptr;
    }

    std::optional<int> SocketGenerator::operator()() {
        if (m_cursor == nullptr) {
            return {};
        }

        const auto socket_fd = socket(m_cursor->ai_family, m_cursor->ai_socktype, m_cursor->ai_protocol);

        if (socket_fd == socket_fd_dud) {
            return {};
        }

        if (bind(socket_fd, m_cursor->ai_addr, m_cursor->ai_addrlen) == socket_fd_dud) {
            close(socket_fd);
            return {};
        }

        m_cursor = m_cursor->ai_next;

        return {socket_fd};
    }
}
