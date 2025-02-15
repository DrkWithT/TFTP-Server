#pragma once

#include <string>
#include <variant>

namespace TftpServer::MyTftp {
    enum class Opcode : unsigned char {
        rrq = 1,
        wrq,
        data,
        ack,
        err,
        none,
        last = none
    };

    enum class DataMode : unsigned char {
        netascii,
        octet,
        mail,
        dud,
        last = dud
    };

    enum class ErrorCode : unsigned char {
        ok,
        not_defined,
        file_not_found,
        access_violation,
        storage_issue,
        bad_operation,
        unknown_tid,
        file_already_exists,
        no_user,
        last = no_user
    };

    struct DudPayload {};

    struct RWPayload {
        std::string filename;
        DataMode mode;
    };

    struct DataPayload {
        unsigned int block_n;
        std::u8string data;
    };

    struct AckPayload {
        unsigned short block_n;
    };

    struct ErrorPayload {
        ErrorCode error;
        std::string message;
    };

    struct Message {
        Opcode op;
        std::variant<DudPayload, RWPayload, DataPayload, AckPayload, ErrorPayload> payload;
    };
}