#pragma once

#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "mybsock/buffers.hpp"

namespace TftpServer::MyBSock {
    enum class IOStatus {
        ok,
        invalid_args,
        pipe_closed
    };

    struct IOResult {
        sockaddr_in data;
        IOStatus status;
    };

    class UDPServerSocket {
    private:
        int m_fd;
        bool m_ready;
        bool m_closed;

        [[nodiscard]] bool isUsable() const noexcept;

    public:
        UDPServerSocket() noexcept;
        UDPServerSocket(int fd) noexcept;
        ~UDPServerSocket();

        UDPServerSocket(const UDPServerSocket& other) = delete;
        UDPServerSocket& operator=(const UDPServerSocket& other) = delete;

        UDPServerSocket(UDPServerSocket&& other) noexcept;
        UDPServerSocket& operator=(UDPServerSocket&& other) noexcept;

        template <typename BufferT, std::size_t BufferN>
        [[nodiscard]] IOResult recieveFrom(FixedBuffer<BufferT, BufferN>& buffer, std::size_t n) {
            if (m_closed) {
                return { {}, IOStatus::pipe_closed };
            }

            if (buffer.isFull() or n >= buffer.getSize()) {
                return { {}, IOStatus::invalid_args };
            }

            buffer.reset();

            IOResult temp = {};

            auto* read_ptr = buffer.getPtr();
            socklen_t sa_size = sizeof(sockaddr_in);
            const auto count = recvfrom(m_fd, read_ptr, n, 0, reinterpret_cast<sockaddr*>(&temp.data), &sa_size);

            if (count > 0) {
                buffer.markLength(count);
                temp.status= IOStatus::ok;
            } else {
                temp.status = IOStatus::pipe_closed;
            }

            return temp;
        }

        template <typename BufferT, std::size_t BufferN>
        IOResult sendTo(const FixedBuffer<BufferT, BufferN>& buffer, std::size_t n, const IOResult& prev) {
            if (m_closed) {
                return { {}, IOStatus::pipe_closed };
            }

            if (buffer.isEmpty() or n >= buffer.getLength()) {
                return { {}, IOStatus::invalid_args };
            }

            IOResult temp = prev;

            const auto* read_ptr = buffer.getPtr();
            socklen_t sa_size = sizeof(sockaddr_in);
            const auto count = sendto(m_fd, read_ptr, n, 0, reinterpret_cast<sockaddr*>(&temp.data), sa_size);

            if (count > 0) {
                temp.status= IOStatus::ok;
            } else {
                temp.status = IOStatus::pipe_closed;
            }

            return temp;
        }
    };
}