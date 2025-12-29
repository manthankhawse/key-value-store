#include <cstdint>
#include <chrono>

uint64_t now_ns() {
    auto now = std::chrono::system_clock::now();
    auto nanoseconds_since_epoch = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    return static_cast<uint64_t>(nanoseconds_since_epoch);
}