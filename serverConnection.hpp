#pragma once

#include <vector>
#include <cstring>
#include <type_traits>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <stdexcept>

class ServerConnection {
public:
    ServerConnection(const std::string& ip = "127.0.0.1", uint16_t port = 8080) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

        if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            throw std::runtime_error("Failed to connect to server");
        }
    }

    ~ServerConnection() {
        if (sock >= 0) {
            close(sock);
        }
    }

    // Type trait to detect vectors
    template <typename T>
    struct is_std_vector : std::false_type {};

    template <typename T, typename A>
    struct is_std_vector<std::vector<T, A>> : std::true_type {};

    // Send data and return response
    template<typename Ret, typename... Args>
    Ret send(uint32_t func_id, Args&&... args) {
        char buffer[2048] = {0};
        size_t offset = 0;

        append_to_buffer(buffer, offset, func_id);
        (append_to_buffer(buffer, offset, std::forward<Args>(args)), ...); // Serialize args

        ::send(sock, buffer, offset, 0);

        if constexpr (is_std_vector<Ret>::value) {
            using ElemType = typename Ret::value_type;
            char recv_buf[2048] = {0};
            ssize_t msgLen = read(sock, recv_buf, sizeof(recv_buf));
            if (msgLen <= 0) return {};

            size_t read_offset = 0;
            size_t vec_size = read_from_buffer<size_t>(recv_buf, read_offset);

            Ret result(vec_size);
            for (size_t i = 0; i < vec_size; ++i) {
                result[i] = read_from_buffer<ElemType>(recv_buf, read_offset);
            }
            return result;
        } else {
            Ret result{};
            ssize_t msgLen = read(sock, &result, sizeof(result));
            return result;
        }
    }

private:
    int sock;

    // Append value (basic types)
    template<typename T>
    void append_to_buffer(char* buffer, size_t& offset, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        std::memcpy(buffer + offset, &value, sizeof(T));
        offset += sizeof(T);
    }

    // Append vector
    template<typename T>
    void append_to_buffer(char* buffer, size_t& offset, const std::vector<T>& vec) {
        size_t size = vec.size();
        append_to_buffer(buffer, offset, size); // Serialize size first
        for (const auto& elem : vec) {
            append_to_buffer(buffer, offset, elem); // Then serialize elements
        }
    }

    template<typename T>
    T read_from_buffer(const char* buffer, size_t& offset) const {
        T value;
        std::memcpy(&value, buffer + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }
};
