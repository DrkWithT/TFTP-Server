#pragma once

#include <utility>
#include <cstring>
#include <string>
#include "meta/helpers.hpp"
#include "mybsock/buffers.hpp"
#include "mytftp/types.hpp"

namespace TftpServer::MyTftp {
    using tftp_u8 = unsigned char;
    using tftp_u16 = unsigned short;

    struct RWOpt {};
    struct DataOpt {};
    struct AckOpt {};
    struct ErrOpt {};

    /// NOTE: denotes max payload length of `RRQ/WRQ`, assuming 512B for each filename and mode string.
    inline constexpr auto max_data_chunk_size = 512UL;
    inline constexpr auto max_payload_size = 514UL;
    inline constexpr auto dud_payload_num = 1024UL;

    inline const std::string mode_name_netascii = "netascii";
    inline const std::string mode_name_mail = "mail";
    inline const std::string mode_name_octet = "octet";

    [[nodiscard]] inline const std::string& toFileModeName(DataMode mode) noexcept {
        if (mode == DataMode::netascii) {
            return mode_name_netascii;
        } else if (mode == DataMode::mail) {
            return mode_name_mail;
        } else {
            return mode_name_octet;
        }
    }

    template <typename DataType>
    struct HelperResult {
        DataType data;
        std::size_t current_pos;
    };

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] HelperResult<tftp_u16> readU16(const MyBSock::FixedBuffer<T, N>& source, std::size_t begin) {
        tftp_u16 temp = 0;

        if (begin > source.getLength() - 2UL) {
            return {0, dud_payload_num};
        }

        std::memcpy(&temp, source.viewPtr() + begin, 2UL);

        temp = ntohs(temp);

        return {temp, begin + 2UL};
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] HelperResult<std::string> readText(const MyBSock::FixedBuffer<T, N>& source, std::size_t begin) {
        std::string temp;
        const auto source_len = source.getLength();

        if (begin >= source_len) {
            return {temp, dud_payload_num};
        }

        const auto* read_ptr = source.viewPtr();
        auto read_offset = begin;

        for (; read_offset < source_len; read_offset++) {
            const auto c = static_cast<char>(read_ptr[read_offset]);

            if (c == '\0') {
                break;
            }

            temp += c;
        }

        /// NOTE: +1 to the offset since 0 is a delimiter for TFTP strings within payloads, so I must skip it for the next data field.
        return {
            std::move(temp),
            read_offset + 1UL
        };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] HelperResult<std::u8string> readBlob(const MyBSock::FixedBuffer<T, N>& source, std::size_t begin, std::size_t blob_length) {
        std::u8string temp;
        const auto source_len = source.getLength();

        if (begin >= source_len) {
            return {temp, dud_payload_num};
        }

        const auto* read_ptr = source.viewPtr();
        auto read_offset = begin;
        auto pending_rc = blob_length;

        while (pending_rc > 0 and read_offset < source_len) {
            temp += static_cast<tftp_u8>(read_ptr[read_offset]);
            pending_rc--;
            read_offset++;
        }

        return {
            std::move(temp),
            read_offset
        };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] HelperResult<bool> writeU16(MyBSock::FixedBuffer<T, N>& target, std::size_t begin, tftp_u16 value) {
        const auto network_ord_value = htons(value);

        if (begin > target.getLength() - 2UL) {
            return { false, dud_payload_num };
        }

        auto* write_ptr = target.viewPtr();
        std::memcpy(write_ptr, &network_ord_value, 2UL);

        return { true, begin + 2UL };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] HelperResult<bool> writeText(MyBSock::FixedBuffer<T, N>& target, std::size_t begin, const std::string& value) {
        const auto value_len = value.size();
        const auto target_len = target.getLength();

        if (begin + value_len >= target_len) {
            return { false, dud_payload_num };
        }

        const auto* value_read_ptr = value.c_str();
        auto* write_ptr = target.viewPtr();
        auto write_offset = begin;
        auto pending_n = value_len;

        while (pending_n > 0 and write_offset < target_len) {
            write_ptr[write_offset] = static_cast<T>(*value_read_ptr);
            pending_n--;
            write_offset++;
            value_read_ptr++;
        }

        return { true, write_offset };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] HelperResult<bool> writeBlob(MyBSock::FixedBuffer<T, N>& target, std::size_t begin, const std::u8string& value) {
        const auto value_len = value.size();
        const auto target_len = target.getLength();

        if (begin + value_len >= target_len) {
            return { false, dud_payload_num };
        }

        const auto* value_read_ptr = value.c_str();
        auto* write_ptr = target.viewPtr();
        auto write_offset = begin;
        auto pending_n = value_len;

        while (pending_n > 0 and write_offset < target_len) {
            write_ptr[write_offset] = static_cast<T>(*value_read_ptr);
            pending_n--;
            write_offset++;
            value_read_ptr++;
        }

        return { true, write_offset };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] RWPayload parsePayload(const MyBSock::FixedBuffer<T, N>& source, [[maybe_unused]] RWOpt opt) {
        auto parse_pos = 2UL;

        auto [filename, pos_1] = readText(source, parse_pos);

        if (pos_1 == dud_payload_num) {
            return { "", DataMode::dud };
        }

        auto [filemode, pos_2] = readText(source, pos_1);

        if (pos_2 == dud_payload_num) {
            return { "", DataMode::dud };
        }

        DataMode temp_mode;

        if (filemode == "netascii") {
            temp_mode = DataMode::netascii;
        } else if (filemode == "mail") {
            temp_mode = DataMode::mail;
        } else {
            temp_mode = DataMode::octet;
        }

        return {
            std::move(filename),
            temp_mode
        };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] DataPayload parsePayload(const MyBSock::FixedBuffer<T, N>& source, [[maybe_unused]] DataOpt opt) {
        auto parse_pos = 2UL;

        auto [block_n, pos_1] = readU16(source, parse_pos);

        if (pos_1 == dud_payload_num) {
            return { 0, {} };
        }

        auto [data_blob, pos_2] = readBlob(source, pos_1, max_data_chunk_size);

        if (pos_2 == dud_payload_num) {
            return { 0, {}};
        }

        return { block_n, std::move(data_blob) };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] AckPayload parsePayload(const MyBSock::FixedBuffer<T, N>& source, [[maybe_unused]] AckOpt opt) {
        auto parse_pos = 2UL;

        auto [block_n, pos_1] = readU16(source, parse_pos);

        if (pos_1 == dud_payload_num) {
            return { 0 };
        }

        return { block_n };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] ErrorPayload parsePayload(const MyBSock::FixedBuffer<T, N>& source, [[maybe_unused]] ErrOpt opt) {
        auto parse_pos = 2UL;

        auto [raw_errcode, pos_1] = readU16(source, parse_pos);

        if (pos_1 == dud_payload_num) {
            return { ErrorCode::not_defined, "Message decoding failed!" };
        }

        auto [raw_msg, pos_2] = readText(source, pos_1);

        if (pos_2 == dud_payload_num) {
            return { ErrorCode::not_defined, "Message decoding failed!" };
        }
        
        return {
            static_cast<ErrorCode>(raw_errcode),
            std::move(raw_msg)
        };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] Message parseMessage(const MyBSock::FixedBuffer<T, N>& source) {
        auto parse_pos = 0UL;

        const auto [opcode, pos] = readU16(source, parse_pos);

        if (pos >= dud_payload_num) {
            return { Opcode::none, DudPayload {} };
        }

        const auto opcode_enum_v = static_cast<Opcode>(opcode);

        if (opcode_enum_v == Opcode::rrq or opcode_enum_v == Opcode::wrq) {
            return { opcode_enum_v, parsePayload(source, RWOpt {}) };
        } else if (opcode_enum_v == Opcode::data) {
            return { opcode_enum_v, parsePayload(source, DataOpt {}) };
        } else if (opcode_enum_v == Opcode::ack) {
            return { opcode_enum_v, parsePayload(source, AckOpt {}) };
        } else if (opcode_enum_v == Opcode::err) {
            return { opcode_enum_v, parsePayload(source, ErrOpt {}) };
        }

        return { Opcode::none, DudPayload {} };
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] bool serializePayload(MyBSock::FixedBuffer<T, N>& target, const RWPayload& payload) {
        auto [filename, filemode] = payload;
        
        auto [field_1_ok, pos_1] = writeText(target, 2UL, filename);

        if (not field_1_ok) {
            target.markLength(0);
            return false;
        }

        auto [field_2_ok, pos_2] = writeText(target, pos_1, toFileModeName(filemode));

        if (not field_2_ok or pos_2 == dud_payload_num) {
            target.markLength(0);
            return false;
        }

        target.markLength(pos_2);
        return true;
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] bool serializePayload(MyBSock::FixedBuffer<T, N>& target, const DataPayload& payload) {
        auto [block_n, data_blob] = payload;

        auto [field_1_ok, pos_1] = writeU16(target, 2UL, block_n);

        if (not field_1_ok) {
            target.markLength(0);
            return false;
        }

        auto [field_2_ok, pos_2] = writeBlob(target, pos_1, data_blob);

        if (not field_2_ok) {
            target.markLength(0);
            return false;
        }

        target.markLength(pos_2);
        return true;
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] bool serializePayload(MyBSock::FixedBuffer<T, N>& target, const AckPayload& payload) {
        auto [block_n] = payload;

        auto [field_1_ok, pos_1] = writeU16(target, 2UL, block_n);

        if (not field_1_ok or pos_1 == dud_payload_num) {
            target.markLength(0);
            return false;
        }

        target.markLength(pos_1);
        return true;
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] bool serializePayload(MyBSock::FixedBuffer<T, N>& target, const ErrorPayload& payload) {
        auto [errcode, msg] = payload;
        const auto errcode_n = static_cast<tftp_u16>(errcode);

        auto [field_1_ok, pos_1] = writeU16(target, 2UL, errcode_n);

        if (not field_1_ok) {
            target.markLength(0);
            return false;
        }

        auto [field_2_ok, pos_2] = writeText(target, pos_1, msg);

        if (not field_2_ok or pos_2 == dud_payload_num) {
            target.markLength(0);
            return false;
        }

        target.markLength(pos_2);
        return true;
    }

    template <Meta::OctetKind T, std::size_t N>
    [[nodiscard]] bool serializeMessage(MyBSock::FixedBuffer<T, N>& target, const Message& msg) {
        const auto msg_opcode = msg.op;
        const auto opcode_n = static_cast<tftp_u16>(msg_opcode);

        auto [field_0_ok, pos_0] = writeU16(target, 0UL, opcode_n);

        if (not field_0_ok or pos_0 == dud_payload_num) {
            target.markLength(0);
            return false;
        }

        if (msg_opcode == Opcode::rrq or msg_opcode == Opcode::wrq) {
            return serializePayload(target, std::get<RWPayload>(msg.payload));
        } else if (msg_opcode == Opcode::data) {
            return serializePayload(target, std::get<DataPayload>(msg.payload));
        } else if (msg_opcode == Opcode::ack) {
            return serializePayload(target, std::get<AckPayload>(msg.payload));
        } else if (msg_opcode == Opcode::err) {
            return serializePayload(target, std::get<ErrorPayload>(msg.payload));
        }

        return false;
    }
}