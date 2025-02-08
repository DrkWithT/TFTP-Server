#pragma once

#include <type_traits>
#include <concepts>

namespace TftpServer::Meta {
    template <typename T>
    concept OctetKind = std::is_same_v<T, char> or std::is_same_v<T, unsigned char>;
}