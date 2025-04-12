#pragma once
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <string>

class Dispatcher {
public:
    using Handler = std::function<std::vector<char>(const char* buffer, size_t& offset)>;

    void register_handler(uint32_t func_id, Handler handler) {
        handlers[func_id] = std::move(handler);
    }

    std::vector<char> dispatch(uint32_t func_id, const char* buffer, size_t& offset) const {
        auto it = handlers.find(func_id);
        if (it != handlers.end()) {
            return it->second(buffer, offset);
        } else {
            throw std::runtime_error("Unknown function ID: " + std::to_string(func_id));
        }
    }

private:
    std::unordered_map<uint32_t, Handler> handlers;
};
