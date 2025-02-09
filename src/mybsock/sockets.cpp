#include <utility>
#include "mybsock/sockets.hpp"

namespace TftpServer::MyBSock {
    static constexpr auto dud_socket_fd = -1;

    bool UDPServerSocket::isUsable() const noexcept {
        return not m_closed and m_ready;
    }

    UDPServerSocket::UDPServerSocket() noexcept
    : m_fd {dud_socket_fd}, m_ready {false}, m_closed {true} {}

    UDPServerSocket::UDPServerSocket(int fd) noexcept
    : m_fd {fd}, m_ready {fd != dud_socket_fd}, m_closed {not m_ready} {}

    UDPServerSocket::~UDPServerSocket() {
        if (not m_ready or m_closed) {
            return;
        }

        close(m_fd);
        m_closed = true;
    }

    UDPServerSocket::UDPServerSocket(UDPServerSocket&& other) noexcept
    : m_fd {dud_socket_fd}, m_ready {false}, m_closed {true} {
        if (&other == this) {
            return;
        }

        if (m_fd != dud_socket_fd) {
            close(m_fd);
            m_ready = false;
            m_closed = true;
        }

        m_fd = std::exchange(other.m_fd, dud_socket_fd);
        m_ready = std::exchange(other.m_ready, false);
        m_closed = std::exchange(other.m_closed, true);
    }

    UDPServerSocket& UDPServerSocket::operator=(UDPServerSocket&& other) noexcept {
        if (&other == this) {
            return *this;
        }

        if (m_fd != dud_socket_fd) {
            close(m_fd);
            m_ready = false;
            m_closed = true;
        }

        m_fd = std::exchange(other.m_fd, dud_socket_fd);
        m_ready = std::exchange(other.m_ready, false);
        m_closed = std::exchange(other.m_closed, true);

        return *this;
    }
}