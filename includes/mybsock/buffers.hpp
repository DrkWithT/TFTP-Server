#pragma once

#include <algorithm>
#include <array>
#include <string_view>
#include "meta/helpers.hpp"

namespace TftpServer::MyBSock {
    template <Meta::OctetKind T>
    class BufferView {
    private:
        const T* m_ptr;
        std::size_t m_length;

    public:
        BufferView() = delete;
        constexpr BufferView(const T* ptr, std::size_t length) noexcept
        : m_ptr {ptr}, m_length {length} {}

        [[nodiscard]] constexpr std::size_t getLength() const noexcept {
            return m_length;
        }

        explicit constexpr operator bool() const noexcept {
            return m_ptr != nullptr and m_length > 0;
        }

        [[nodiscard]] constexpr bool operator==(const BufferView& other) const noexcept {
            if (getLength() != other.m_length) {
                return false;
            }

            const auto* here_cursor = m_ptr;
            const auto* other_cursor = other.m_ptr;

            if (m_ptr == nullptr or m_ptr == nullptr) {
                return false;
            }

            constexpr auto common_length = m_length;

            return std::equal(here_cursor, here_cursor + common_length, other_cursor);
        }

        [[nodiscard]] constexpr bool operator==(const std::string_view ascii_view) const noexcept {
            if (getLength() != ascii_view.length() or m_ptr == nullptr) {
                return false;
            }

            if constexpr (not std::is_same_v<T, char>) {
                return false;
            }

            const auto* here_cursor = m_ptr;
            const auto* view_cursor = ascii_view.data();
            constexpr auto common_length = getLength();

            return std::equal(here_cursor, here_cursor + common_length, view_cursor);
        }
    };

    template <Meta::OctetKind T, std::size_t N> requires (N > 0UL)
    class FixedBuffer {
    private:
        std::array<T, N> m_data;
        std::size_t m_length;

    public:
        constexpr FixedBuffer() noexcept
        : m_data {}, m_length {0UL} {
            reset();
        }

        [[nodiscard]] T* viewPtr() noexcept {
            return m_data.data();
        }

        [[nodiscard]] constexpr std::size_t getLength() const noexcept {
            return m_length;
        }

        void markLength(std::size_t length) noexcept {
            m_length = length;
        }

        [[nodiscard]] constexpr std::size_t getSize() const noexcept {
            return N;
        }

        [[nodiscard]] constexpr bool isEmpty() const noexcept {
            return m_length == 0UL;
        }

        [[nodiscard]] constexpr bool isFull() const noexcept {
            return m_length >= N;
        }

        T& accessOctet(std::size_t index) {
            return m_data.at(index);
        }

        [[nodiscard]] bool appendOctet(T octet) {
            if (isFull()) {
                return false;
            }

            m_data[m_length++] = octet;

            return true;
        }

        constexpr void reset() noexcept {
            std::fill(m_data.begin(), m_data.end(), T {});
            m_length = 0UL;
        }

        friend constexpr BufferView<T> makeView(const FixedBuffer& buffer, std::size_t begin, std::size_t length) noexcept {
            if (begin + length >= buffer.getSize()) {
                return { nullptr, 0 };
            }

            return { buffer.viewPtr() + begin, length };
        }
    };
}