#pragma once

#include "serverDispatcher.hpp"
#include <vector>
#include <thread>
#include <functional>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <tuple>
#include <type_traits>

// ========== Type Traits ==========

template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};

// ========== Buffer Utilities ==========

template<typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, T>
read_from_buffer(const char* buffer, size_t& offset) {
    T value;
    std::memcpy(reinterpret_cast<void*>(&value), buffer + offset, sizeof(T));
    offset += sizeof(T);
    return value;
}

template<typename T>
std::vector<T> read_vector_from_buffer(const char* buffer, size_t& offset) {
    size_t size = read_from_buffer<size_t>(buffer, offset);
    std::vector<T> vec(size);
    for (size_t i = 0; i < size; ++i) {
        vec[i] = read_from_buffer<T>(buffer, offset);
    }
    return vec;
}

template<typename T>
void write_to_buffer(std::vector<char>& buffer, const T& value) {
    const char* raw = reinterpret_cast<const char*>(&value);
    buffer.insert(buffer.end(), raw, raw + sizeof(T));
}

template<typename T>
void write_vector_to_buffer(std::vector<char>& buffer, const std::vector<T>& vec) {
    write_to_buffer(buffer, vec.size());
    for (const auto& val : vec) {
        write_to_buffer(buffer, val);
    }
}

// ========== Server Class ==========

class Server {
public:
    using Handler = Dispatcher::Handler;

    Server(uint16_t port = 8080)
        : port(port), client_id(0)
    {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) throw std::runtime_error("Failed to create socket");

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
            throw std::runtime_error("Failed to bind");

        if (listen(server_fd, SOMAXCONN) < 0)
            throw std::runtime_error("Failed to listen");
    }

    ~Server() {
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
        close(server_fd);
    }

    void register_handler(uint32_t func_id, Handler handler) {
        dispatcher.register_handler(func_id, std::move(handler));
    }

    template<typename Ret, typename... Args>
    void register_typed_handler(uint32_t func_id, Ret(*func)(Args...)) {
        register_handler(func_id, [=](const char* buf, size_t& offset) {
            auto args = read_args_from_buffer<Args...>(buf, offset);
            Ret result = call_with_tuple(func, args);
            return serialize_result(result);
        });
    }

    void start() {
        std::cout << "Server listening on port " << port << "...\n";
        socklen_t addrlen = sizeof(address);
        while (true) {
            int client_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
            if (client_socket >= 0) {
                threads.emplace_back([=]() {
                    handle_client(client_socket, client_id++);
                });
            } else {
                std::cerr << "Failed to accept client\n";
            }
        }
    }

private:
    int server_fd;
    uint16_t port;
    sockaddr_in address;
    Dispatcher dispatcher;
    std::vector<std::thread> threads;
    std::atomic<int> client_id;

    void handle_client(int client_socket, int client_id) {
        std::cout << "[Client " << client_id << "] Connected.\n";
        char buffer[1024];

        while (true) {
            ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                std::cout << "[Client " << client_id << "] Disconnected.\n";
                break;
            }

            size_t offset = 0;
            uint32_t func_id = read_from_buffer<uint32_t>(buffer, offset);

            try {
                std::vector<char> response = dispatcher.dispatch(func_id, buffer, offset);
                send(client_socket, response.data(), response.size(), 0);
            } catch (const std::exception& ex) {
                std::cerr << "[Client " << client_id << "] Error: " << ex.what() << "\n";
            }
        }

        close(client_socket);
    }

    // ======= Reflection Helpers =======

    template<typename T>
    using Decayed = std::decay_t<T>;

    template<typename... Args, size_t... Is>
    std::tuple<Decayed<Args>...> read_args_from_buffer_impl(const char* buf, size_t& offset, std::index_sequence<Is...>) {
        return std::make_tuple(read_single_arg<Decayed<Args>>(buf, offset)...);
    }

    template<typename... Args>
    std::tuple<Decayed<Args>...> read_args_from_buffer(const char* buf, size_t& offset) {
        return read_args_from_buffer_impl<Args...>(buf, offset, std::index_sequence_for<Args...>{});
    }

    template<typename T>
    std::enable_if_t<!is_vector<T>::value, T>
    read_single_arg(const char* buf, size_t& offset) {
        return read_from_buffer<T>(buf, offset);
    }

    template<typename T>
    std::enable_if_t<is_vector<T>::value, T>
    read_single_arg(const char* buf, size_t& offset) {
        using Elem = typename T::value_type;
        return read_vector_from_buffer<Elem>(buf, offset);
    }

    template<typename Func, typename Tuple, size_t... I>
    auto call_with_tuple_impl(Func&& func, Tuple&& t, std::index_sequence<I...>) {
        return func(std::get<I>(std::forward<Tuple>(t))...);
    }

    template<typename Func, typename Tuple>
    auto call_with_tuple(Func&& func, Tuple&& t) {
        constexpr size_t N = std::tuple_size<std::decay_t<Tuple>>::value;
        return call_with_tuple_impl(std::forward<Func>(func), std::forward<Tuple>(t), std::make_index_sequence<N>{});
    }

    template<typename T>
    std::vector<char> serialize_result(const T& value) {
        std::vector<char> out;
        write_to_buffer(out, value);
        return out;
    }

    template<typename T>
    std::vector<char> serialize_result(const std::vector<T>& vec) {
        std::vector<char> out;
        write_vector_to_buffer(out, vec);
        return out;
    }
};
