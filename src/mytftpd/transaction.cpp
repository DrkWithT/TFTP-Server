#include "mytftpd/transaction.hpp"
#include "mytftp/messaging.hpp"

namespace TftpServer::Driver::FileUtils {
    std::u8string readOctetChunk(std::fstream& fs) {
        std::u8string temp;
        auto pending_rc = Constants::tftp_chunk_size;

        while (not fs.eof() and pending_rc > 0) {
            temp += static_cast<MyTftp::tftp_u8>(fs.get());
            --pending_rc;
        }

        return temp;
    }

    bool writeOctetChunk(std::fstream& fs, const std::u8string& data) {
        auto pending_wc = data.size();
        const auto* data_ptr = data.data();

        while (not fs.eof() and not fs.bad() and pending_wc > 0) {
            fs.put(*data_ptr);
            --pending_wc;
        }

        return pending_wc == Constants::rw_done_n;
    }
}