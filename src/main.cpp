/**
 * @file main.cpp
 * @author DrkWithT
 * @brief Implements driver logic for the TFTP server.
 * @date 2/05/2025
 */

// TODO: implement driver.

#include <array>
#include <string>
#include <atomic>
#include <thread>
#include <fstream>
#include <iostream>
#include "mybsock/netconfig.hpp"
#include "mybsock/buffers.hpp"
#include "mybsock/sockets.hpp"
#include "mytftp/types.hpp"
#include "mytftp/messaging.hpp"

static constexpr auto expected_argc = 2;

namespace TftpServer::Driver {
    static constexpr MyTftp::tftp_u16 dud_peer_tid = 0;
    static constexpr auto min_free_port = 1024;
    static constexpr auto tftp_chunk_size = 512UL;
    static constexpr auto io_buffer_size = 1024UL;

    static const std::array<std::string, static_cast<std::size_t>(MyTftp::ErrorCode::last) + 1> server_error_msgs = {
        "OK!",
        "Internal server error: reply corrupted / bad request args.",
        "File not found.",
        "File access violation.",
        "Disk full or allocation exceeded.",
        "Invalid operation.",
        "Unknown transfer ID.",
        "File already exists.",
        "No such user.",
    };

    [[nodiscard]] MyBSock::UDPServerSocket makeUDPSocket(const char* port_cstr);

    struct TransferContext {
        std::fstream fs;
        MyTftp::tftp_u16 block;
        bool done;
    };

    struct ReadResult {
        MyTftp::Message msg;
        MyBSock::IOResult io_data;
    };

    class MyServer {
    private:
        TransferContext m_ctx;  // stores transfer session state
        MyBSock::FixedBuffer<MyTftp::tftp_u8, io_buffer_size> m_buffer;
        MyBSock::UDPServerSocket m_socket;  // for underlying UDP for TFTP messaging
        MyTftp::tftp_u16 m_peer_tid;    // tracks current peer of current transfer
        std::atomic_flag m_persist;

        [[nodiscard]] std::u8string readNextFileChunk();
        [[nodiscard]] bool writeNextFileChunk(const std::u8string& blob);

        [[nodiscard]] ReadResult readMessage();
        void handleMessage(const ReadResult& read_result);
        void sendDataMessage(MyTftp::Opcode op, const MyTftp::Message& previous, const MyBSock::IOResult& prev_io);
        void sendAck(MyTftp::Opcode op, const MyTftp::Message& previous, const MyBSock::IOResult& prev_io);
        void sendError(MyTftp::ErrorCode error_code, const MyBSock::IOResult& prev_io);

    public:
        MyServer() = delete;
        MyServer(MyBSock::UDPServerSocket socket) noexcept;

        void runService();
    };


    MyBSock::UDPServerSocket makeUDPSocket(const char* port_cstr) {
        MyBSock::SocketGenerator sockgen {port_cstr};

        while (sockgen) {
            auto fd_optional = sockgen();

            if (fd_optional.has_value()) {
                return {fd_optional.value()};
            }
        }

        return {};
    }

    std::u8string MyServer::readNextFileChunk() {
        auto pending_rc = tftp_chunk_size;

        std::u8string result;

        while (pending_rc > 0 and not m_ctx.fs.eof()) {
            result += static_cast<MyTftp::tftp_u8>(m_ctx.fs.get());

            pending_rc--;
        }

        return result;
    }

    bool MyServer::writeNextFileChunk(const std::u8string& blob) {
        if (m_ctx.fs.bad()) {
            return false;
        }

        for (const auto octet : blob) {
            if (m_ctx.fs.put(octet).bad()) {
                return false;
            }
        }

        return true;
    }

    ReadResult MyServer::readMessage() {
        m_buffer.reset();

        auto io_result = m_socket.recieveFrom(m_buffer, io_buffer_size);

        return {
            MyTftp::parseMessage(m_buffer),
            io_result
        };
    }

    void MyServer::handleMessage(const ReadResult& read_result) {
        m_buffer.reset();

        const auto& [msg, io_result] = read_result;

        const auto peer_tid = io_result.data.sin_port;

        /// NOTE: validate transfer ID of peer's sending port number... RFC 1350 states an invalid TID may denote an incorrectly sent message. 
        if (m_peer_tid == dud_peer_tid) {
            m_peer_tid = peer_tid;
        } else if (peer_tid != m_peer_tid) {
            sendError(MyTftp::ErrorCode::unknown_tid, io_result);
            return;
        }

        const auto opcode = msg.op;

        switch (opcode) {
        case MyTftp::Opcode::rrq:
            sendDataMessage(opcode, msg, io_result);
            break;
        case MyTftp::Opcode::wrq:
        case MyTftp::Opcode::data:
            sendAck(opcode, msg, io_result);
            break;
        case MyTftp::Opcode::ack:
            sendDataMessage(opcode, msg, io_result);
            break;
        case MyTftp::Opcode::err:
            break;
        default:
            sendError(MyTftp::ErrorCode::bad_operation, io_result);
            break;
        }

        /// NOTE: Prepare to service a new peer by TID after the transfer finishes. The peer could be the same or different.
        if (m_ctx.done) {
            m_peer_tid = dud_peer_tid;
        }
    }

    void MyServer::sendDataMessage(MyTftp::Opcode op, const MyTftp::Message& previous, const MyBSock::IOResult& prev_io) {
        if (op == MyTftp::Opcode::rrq) {
            const auto [filename, filemode] = std::get<MyTftp::RWPayload>(previous.payload);

            if (filemode != MyTftp::DataMode::octet) {
                sendError(MyTftp::ErrorCode::not_defined, prev_io);
                return;
            }

            std::fstream temp_fs {filename, std::ifstream::binary};

            if (not temp_fs.is_open()) {
                sendError(MyTftp::ErrorCode::file_not_found, prev_io);
                return;
            }

            m_ctx.fs = std::move(temp_fs),
            m_ctx.block = 1;
            m_ctx.done = false;

            auto chunk_blob = readNextFileChunk();
            const auto at_last_chunk = chunk_blob.size() < tftp_chunk_size;
            const auto data_msg_ok = MyTftp::serializeMessage(m_buffer, MyTftp::Message {
                MyTftp::Opcode::data,
                MyTftp::DataPayload {
                    .data = std::move(chunk_blob),
                    .block_n = m_ctx.block
                }
            });

            if (not data_msg_ok) {
                sendError(MyTftp::ErrorCode::not_defined, prev_io);
                return;
            }

            m_ctx.done = at_last_chunk;
            m_socket.sendTo(m_buffer, m_buffer.getLength(), prev_io);
        } else {
            // 2. for ACK
            if (m_ctx.done) {
                return;
            }

            const auto [prev_block_n] = std::get<MyTftp::AckPayload>(previous.payload);

            const auto next_block_n = prev_block_n + 1U;

            auto next_chunk_blob = readNextFileChunk();
            const auto at_ending_chunk = next_chunk_blob.size() < tftp_chunk_size;
            const auto next_data_ok = MyTftp::serializeMessage(m_buffer, MyTftp::Message {
                MyTftp::Opcode::data,
                MyTftp::DataPayload {
                    .data = std::move(next_chunk_blob),
                    .block_n = next_block_n
                }
            });

            if (not next_data_ok) {
                sendError(MyTftp::ErrorCode::not_defined, prev_io);
                return;
            }

            m_ctx.block = next_block_n;
            m_ctx.done = at_ending_chunk;
            m_socket.sendTo(m_buffer, m_buffer.getLength(), prev_io);
        }
    }

    void MyServer::sendAck(MyTftp::Opcode op, const MyTftp::Message& previous, const MyBSock::IOResult& prev_io) {
        if (op == MyTftp::Opcode::wrq) {
            const auto [filename, filemode] = std::get<MyTftp::RWPayload>(previous.payload);

            if (filemode != MyTftp::DataMode::octet) {
                sendError(MyTftp::ErrorCode::not_defined, prev_io);
                return;
            }

            std::fstream temp_fs {filename, std::fstream::binary};

            if (not temp_fs.is_open()) {
                sendError(MyTftp::ErrorCode::access_violation, prev_io);
                return;
            }

            m_ctx.fs = std::move(temp_fs),
            m_ctx.block = 0;
            m_ctx.done = false;

            if (not MyTftp::serializeMessage(m_buffer, MyTftp::Message {
                MyTftp::Opcode::ack,
                MyTftp::AckPayload {
                    m_ctx.block
                }
            })) {
                sendError(MyTftp::ErrorCode::not_defined, prev_io);
                return;
            }

            m_socket.sendTo(m_buffer, m_buffer.getLength(), prev_io);
        } else {
            const auto [block_n, chunk] = std::get<MyTftp::DataPayload>(previous.payload);

            /// NOTE: validate block num. to check chunk ordering... a mis-ordered chunk would result in the wrong file contents!
            if (const auto prev_block_n = m_ctx.block; block_n - 1 != prev_block_n) {
                sendError(MyTftp::ErrorCode::not_defined, prev_io);
                return;
            }

            if (not writeNextFileChunk(chunk)) {
                sendError(MyTftp::ErrorCode::storage_issue, prev_io);
                return;
            }

            // 2. for DATA msg
            if (MyTftp::serializeMessage(m_buffer, MyTftp::Message {
                MyTftp::Opcode::ack,
                MyTftp::AckPayload {
                    block_n
                }
            })) {
                m_socket.sendTo(m_buffer, m_buffer.getLength(), prev_io);
                m_ctx.block++;
            }

            m_ctx.done = chunk.size() < tftp_chunk_size;
        }
    }

    void MyServer::sendError(MyTftp::ErrorCode error_code, const MyBSock::IOResult& prev_io) {
        const auto error_msg_id = static_cast<MyTftp::tftp_u16>(error_code);
        const auto& error_msg = server_error_msgs[error_msg_id];

        if (MyTftp::serializeMessage(m_buffer, MyTftp::Message {
            MyTftp::Opcode::err,
            MyTftp::ErrorPayload {
                .error = error_code,
                .message = error_msg
            }
        })) {
            m_socket.sendTo(m_buffer, m_buffer.getLength(), prev_io);
        }

        m_ctx.done = true;
    }

    MyServer::MyServer(MyBSock::UDPServerSocket socket) noexcept
    : m_ctx {}, m_buffer {}, m_socket {std::move(socket)}, m_peer_tid {dud_peer_tid}, m_persist {true} {}

    void MyServer::runService() {
        auto user_control_fn = [this]() {
            char stop_choice = 'n';

            do {
                std::cout << "Enter 'y' to stop.\n";
                std::cin >> stop_choice;
            } while (stop_choice != 'y');

            m_persist.clear();
            std::cout << "Please wait, as a final request must be served.\n";
        };

        std::thread control_thread {user_control_fn};

        while (m_persist.test() or not m_ctx.done) {
            const auto recieve_result = readMessage();
            handleMessage(recieve_result);
        }

        control_thread.join();
    }
}

int main(int argc, char* argv[]) {
    using namespace TftpServer;

    if (argc != expected_argc) {
        std::cerr << "Invalid argc.\nusage: ./tftpd <port no.>\n";
        return 1;
    }

    const auto checked_port_arg = std::atoi(argv[1]);

    if (checked_port_arg <= Driver::min_free_port) {
        std::cerr << "Invalid port number / transfer ID.\n";
        return 1;
    }

    Driver::MyServer app {Driver::makeUDPSocket(argv[1])};

    app.runService();
}