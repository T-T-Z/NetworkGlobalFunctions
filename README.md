# NetworkGlobalFunctions
Server side function evaluation. Greatly simplifiying multi client syncronization and Multiplayer applications by executing functions on a shared server

Currently designed for Linux to Linux syncronization but easily ported to any OS

This was designed to simplify the process of multi client and multiplayer implementations. Managing packets and data types is often just an annoyance and takes time away from the real project.
This tool abstracts away a lot of the boilerplate code and annoyances allowing you to instead directly call a function that exists on a server and get the return data from it.
Making it significantly easier to handle server->client and client->server interactions as well as.

No addicitonal bloat is added to this tool. No security, no pre processing. So if you want to use this for security sensitive tasks make sure you preprocess your data before sending it.
This was primarily desinged for multiplayer apps where performance is the priority over security.

Format:
```
                  Function to call
                          ↓         ↙  ↙  Arguments that are to be passed to the function "add(int a, int b)"
int sum = conn.send<int>(FUNC_ADD, 5, 7);
                     ↑
Return type (int in this case)
```
Client Example:
```c++
#include "serverConnection.hpp"
#include "serverSideFunctions.hpp"

int main() {
    try {
        ServerConnection conn;

        // Int addition (and return int)
        int sum = conn.send<int>(FUNC_ADD, 5, 7);
        std::cout << "Sum: " << sum << "\n";

        // Float multiplication (and return float)
        float product = conn.send<float>(FUNC_MUL, 2.5f, 4.0f);
        std::cout << "Product: " << product << "\n";

        // Increase Global Counter (and return int)
        int counter = conn.send<int>(INCREASE_COUNTER);
        std::cout << "Counter: " << counter << "\n";

        // Vector add and return
        std::vector<int> v1 = {1, 2, 3};
        std::vector<int> v2 = {4, 5, 6};
        auto resultVec = conn.send<std::vector<int>>(FUNC_VECTOR_ADD, v1, v2);

        std::cout << "Vector result: ";
        for (auto x : resultVec) std::cout << x << " ";
        std::cout << "\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
    }

    return 0;
}
```

Server Example:

```c++
#include "server.hpp"
#include "serverSideFunctions.hpp"

int counter = 0;

int increaseCounter() {
    return ++counter;
}

int add(int a, int b) {
    return a + b;
}

float multiply(float x, float y) {
    return x * y;
}

std::vector<int> add_vectors(const std::vector<int>& v1, const std::vector<int>& v2) {
    size_t size = std::min(v1.size(), v2.size());
    std::vector<int> result(size);
    for (size_t i = 0; i < size; ++i) {
        result[i] = v1[i] + v2[i];
    }
    return result;
}

int main() {
    Server server(8080);

    // Automatically register handlers using function signatures
    server.register_typed_handler(FUNC_ADD, &add);
    server.register_typed_handler(FUNC_MUL, &multiply);
    server.register_typed_handler(FUNC_VECTOR_ADD, &add_vectors);
    server.register_typed_handler(INCREASE_COUNTER, &increaseCounter);

    server.start();
    return 0;
}
```

The Functions Enum (serverSideFunctions.hpp)
```c++
#pragma once

enum FuncID : uint32_t {
    FUNC_ADD = 1,
    FUNC_MUL = 2,
    FUNC_VECTOR_ADD = 3,
    INCREASE_COUNTER = 4
};

```
