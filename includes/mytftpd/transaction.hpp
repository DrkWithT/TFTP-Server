#pragma once

#include <fstream>
#include <string>
#include "mytftp/messaging.hpp"
#include "mytftp/types.hpp"

namespace TftpServer::Driver {
    namespace Constants {
        inline constexpr MyTftp::tftp_u16 tid_dud = 0;
        inline constexpr auto rw_done_n = 0UL;
        inline constexpr MyTftp::tftp_u16 tftp_chunk_size = 512;
        inline constexpr auto tftp_opcode_size = 2UL;
        inline constexpr auto tftp_delim_size = 1UL;
        inline constexpr auto tftp_max_path_length = 64UL;
        inline constexpr auto tftp_max_mode_length = 16UL;
        inline constexpr auto tftp_msg_size_limit = tftp_opcode_size + tftp_max_path_length + tftp_delim_size + tftp_max_mode_length + tftp_delim_size + tftp_chunk_size;
    }

    namespace FileUtils {
        [[nodiscard]] std::u8string readOctetChunk(std::fstream& fs);
        [[nodiscard]] bool writeOctetChunk(std::fstream& fs, const std::u8string& data);
    }

    struct Transaction {
        std::fstream fs {};
        MyTftp::tftp_u16 tid {Constants::tid_dud};
        MyTftp::tftp_u16 block {0};
        MyTftp::DataMode mode {MyTftp::DataMode::dud};
    };

    [[nodiscard]] constexpr bool transactionReady(const Transaction& context) noexcept {
        return context.fs.is_open() and context.tid != Constants::tid_dud and context.mode != MyTftp::DataMode::dud;
    }
}