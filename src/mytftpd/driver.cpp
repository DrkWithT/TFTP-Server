#include <iostream>
#include <thread>
#include "mytftp/types.hpp"
#include "mytftp/messaging.hpp"
#include "mytftpd/transaction.hpp"
#include "mytftpd/driver.hpp"

/// TODO: See RFC 1350, Section 6. Add the required, resending feature for the last DATA message!

namespace TftpServer::Driver {
    static constexpr std::array<std::string_view, static_cast<std::size_t>(MyTftp::ErrorCode::last) + 1> errcode_msgs = {
        "OK",
        "Server error",
        "File not found",
        "Access violation",
        "Memory / storage error",
        "Bad opcode",
        "Unknown TID",
        "File already exists",
        "Invalid user"
    };
    
    std::string_view toErrorMsg(MyTftp::ErrorCode errcode) noexcept {
        const auto errcode_n = static_cast<int>(errcode);
        return errcode_msgs[errcode_n];
    }

    /// @note Only use for unconditional state transitions.
    ServerState MyServer::transition(ServerState next) noexcept {
        return next;
    }

    /// @note Only use for conditional state transitions by `rw_count` and the current operation.
    ServerState MyServer::transition(MyBSock::IOStatus io_status, bool file_io_ok, std::size_t rw_count, MyTftp::Opcode opcode) noexcept {
        if (io_status != MyBSock::IOStatus::ok or not file_io_ok) {
            return ServerState::error;
        }

        if (opcode == MyTftp::Opcode::rrq and rw_count == Constants::tftp_chunk_size) {
            return ServerState::download;
        } else if (opcode == MyTftp::Opcode::rrq and rw_count < Constants::tftp_chunk_size) {
            return ServerState::done;
        } else if (opcode == MyTftp::Opcode::ack and rw_count == Constants::tftp_chunk_size) {
            return ServerState::download;
        } else if (opcode == MyTftp::Opcode::ack and rw_count < Constants::tftp_chunk_size) {
            return ServerState::done;
        } else if (opcode == MyTftp::Opcode::err) {
            return ServerState::error;
        } else {
            return ServerState::decode;
        }
    }

    StateResult<MyTftp::Message> MyServer::stateDecode() {
        std::print("tftpd: decoding some msg...\n"); // debug
        auto io_info = m_socket.recieveFrom(m_in_buffer, Constants::tftp_msg_size_limit);
        auto msg = MyTftp::parseMessage(m_in_buffer);

        m_state = transition(io_info.status, true, Constants::tftp_chunk_size, msg.op);

        return {
            io_info,
            msg
        };
    }

    StateResult<bool> MyServer::stateDownload(const MyBSock::IOResult& last_io, const MyTftp::Message& msg) {
        const auto& [msg_op, msg_body] = msg;
        const auto peer_tid = last_io.data.sin_port;

        if (m_context.tid != Constants::tid_dud and peer_tid != m_context.tid) {
            m_state = transition(ServerState::error);

            return { last_io, false };
        }

        if (msg_op == MyTftp::Opcode::rrq) {
            std::print("tftpd: Handle RRQ...\n"); // debug

            // RRQ case
            auto rrq_body = std::get<MyTftp::RWPayload>(msg_body);

            if (rrq_body.mode != MyTftp::DataMode::octet) {
                m_state = transition(ServerState::error);

                return { last_io, false };
            }

            m_context.fs = std::fstream {rrq_body.filename, std::fstream::binary | std::fstream::in};
            m_context.tid = (m_context.tid == Constants::tid_dud) ? peer_tid : m_context.tid;
            m_context.block = 1;
            m_context.mode = MyTftp::DataMode::octet;
        } else {
            // ACK case
            auto [ack_block] = std::get<MyTftp::AckPayload>(msg_body);
            std::print("tftpd: Handle ACK {}\n", ack_block); // debug

            if (ack_block != m_context.block) {
                m_state = transition(ServerState::error);
                return { last_io, false };
            }

            m_context.block++;
        }

        auto chunk_data = FileUtils::readOctetChunk(m_context.fs);
        const auto chunk_size = chunk_data.size();
        std::print("tftpd: sending over {}B...\n", chunk_size); // debug

        const auto serialize_ok = MyTftp::serializeMessage(m_out_buffer, {
            MyTftp::Opcode::data,
            MyTftp::DataPayload {
                m_context.block,
                std::move(chunk_data)
            }
        });

        if (serialize_ok) {
            std::print("tftpd: sending reply...\n"); // debug
            m_socket.sendTo(m_out_buffer, m_out_buffer.getLength(), last_io);
            m_state = transition(last_io.status, true, chunk_size, MyTftp::Opcode::none);
        } else {
            std::print("tftpd: failed to send reply...\n"); // debug
            m_state = transition(ServerState::error);
        }

        return { last_io, serialize_ok };
    }

    StateResult<void> MyServer::stateDone(const MyBSock::IOResult& last_io) {
        std::print("tftpd: Transaction done...\n"); // debug
        m_context = {
            {},
            Constants::tid_dud,
            0,
            MyTftp::DataMode::dud
        };
        m_state = transition(ServerState::decode);

        return { last_io };
    }

    StateResult<void> MyServer::stateError(const MyBSock::IOResult& last_io, bool had_sio_error, const MyTftp::Message& msg) {
        const auto msg_op = msg.op;
        MyTftp::ErrorCode errcode = MyTftp::ErrorCode::not_defined;

        std::print("Handle ERROR; had_sio_error={}, errcode={}\n", had_sio_error, static_cast<int>(errcode)); // debug
        
        if (had_sio_error) {
            errcode = MyTftp::ErrorCode::storage_issue;
        } else if (msg_op < MyTftp::Opcode::rrq or msg_op >= MyTftp::Opcode::last) {
            errcode = MyTftp::ErrorCode::bad_operation;
        }

        if (MyTftp::serializeMessage(m_out_buffer, {
            MyTftp::Opcode::err,
            MyTftp::ErrorPayload {
                errcode,
                toErrorMsg(errcode).data()
            }
        })) {
            m_socket.sendTo(m_out_buffer, m_out_buffer.getLength(), last_io);
        }

        m_state = transition(ServerState::stop);

        return { last_io };
    }

    MyServer::MyServer(MyBSock::UDPServerSocket socket)
    : m_in_buffer {}, m_out_buffer {}, m_context {}, m_socket {std::move(socket)}, m_state {ServerState::decode}, m_halt {false} {}

    bool MyServer::runService() {
        if (not m_socket.isUsable()) {
            return false;
        }

        auto prompt_async = [this]() {
            char choice;

            do {
                std::print("Enter 'y' to quit.\n");
                std::cin >> choice;
            } while (choice != 'y');

            std::print("tftpd: Stopping, please wait.\n"); // debug
            m_halt.clear();
        };

        std::thread prompt_thrd {prompt_async};

        MyTftp::Message temp_msg;
        MyBSock::IOResult temp_sio_info;
        auto server_io_ok = true;

        while (not m_halt.test() and m_state != ServerState::stop) {
            if (m_state == ServerState::decode) {
                auto [sio_info, msg] = stateDecode();
                temp_msg = std::move(msg);
                temp_sio_info = sio_info;
            } else if (m_state == ServerState::download) {
                auto [prev_sio, io_ok] = stateDownload(temp_sio_info, temp_msg);
                temp_sio_info = prev_sio;
                server_io_ok = io_ok;
            } else if (m_state == ServerState::done) {
                temp_sio_info = stateDone(temp_sio_info).io_info;
            } else if (m_state == ServerState::error) {
                temp_sio_info = stateError(temp_sio_info, server_io_ok, temp_msg).io_info;
            }
        }

        prompt_thrd.join();

        return true;
    }
}