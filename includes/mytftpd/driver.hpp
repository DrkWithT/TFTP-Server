#pragma once

#include <atomic>
// #include <thread>
#include <iostream>
#include <format>
#include <string_view>
// #include "mybsock/netconfig.hpp"
#include "mybsock/buffers.hpp"
#include "mybsock/sockets.hpp"
#include "mytftp/types.hpp"
#include "mytftpd/transaction.hpp"

namespace TftpServer::Driver {
    enum class ServerState {
        decode,
        download,
        done,
        error,
        stop
    };

    enum class LogLevel {
        info,
        warning,
        fatal
    };

    template <typename Other>
    struct StateResult {
        MyBSock::IOResult io_info;
        Other other;
    };

    template <>
    struct StateResult <void> {
        MyBSock::IOResult io_info;
    };

    [[nodiscard]] std::string_view toErrorMsg(MyTftp::ErrorCode) noexcept;

    class MyServer {
    private:
        MyBSock::FixedBuffer<MyTftp::tftp_u8, Constants::tftp_msg_size_limit> m_in_buffer;
        MyBSock::FixedBuffer<MyTftp::tftp_u8, Constants::tftp_msg_size_limit> m_out_buffer;
        Transaction m_context;
        MyBSock::UDPServerSocket m_socket;
        ServerState m_state;
        std::atomic_flag m_halt;

        template <LogLevel L, typename ... Args>
        void logMessage(const char* fmt_cstr, Args&& ... args) {
            if constexpr (L == LogLevel::info) {
                std::cout << "tftpd [INFO]: " << std::format(fmt_cstr, args...);
            } else if constexpr (L == LogLevel::warning) {
                std::cout << "tftpd [WARNING]: " << std::format(fmt_cstr, args...);
            } else if constexpr (L == LogLevel::fatal) {
                std::cout << "tftpd [FATAL]: " << std::format(fmt_cstr, args...);
            } else {
                std::cout << "tftpd [LOG]: ...\n";
            }
        }

        [[nodiscard]] ServerState transition(ServerState next) noexcept;
        [[nodiscard]] ServerState transition(MyBSock::IOStatus io_status, bool file_io_ok, std::size_t rw_count, MyTftp::Opcode opcode) noexcept;

        StateResult<MyTftp::Message> stateDecode();
        StateResult<bool> stateDownload(const MyBSock::IOResult& last_io, const MyTftp::Message& msg);
        StateResult<void> stateDone(const MyBSock::IOResult& last_io);
        StateResult<void> stateError(const MyBSock::IOResult& last_io, bool had_sio_error, const MyTftp::Message& msg);
        /** @note No stateStop exists since ServerState::stop is a placeholder state to signal a service halt on an error. */

    public:
        MyServer() = delete;
        MyServer(MyBSock::UDPServerSocket socket);

        [[nodiscard]] bool runService();
    };
}