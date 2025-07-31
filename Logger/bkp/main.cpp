// #include "logger.hpp"
#include "consumer.hpp"
// #include <cstdint>
#include <mach/mach_time.h>
// inline uint64_t rdtsc()
// {
//     unsigned int hi, lo;
//     asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
//     return ((uint64_t)hi << 32) | lo;
// }
mach_timebase_info_data_t info;

inline uint64_t latency(uint64_t cycles)
{
    // uint64_t nanoseconds = (uint64_t)cycles * 1e9 / 3e9;
    return (uint64_t)cycles * info.numer / info.denom;
}
void trader_thread_function()
{
    for (int i = 0; i < 1000000; ++i)
    {
        uint64_t start = mach_absolute_time();
        log("Trade fill: %d %s %.2f", i, "AAPL", 234.56);
        uint64_t end = mach_absolute_time();
        // ... other logic ...
        std::cout << "Latency: " << latency(end - start) << std::endl;
    }
}

int main()
{
    mach_timebase_info(&info);
    std::thread consumer(log_consumer_thread);

    // std::vector<std::thread> producers;
    // for (int i = 0; i < 4; ++i)
    //     producers.emplace_back(trader_thread_function);

    // for (auto &t : producers)
    //     t.join();
    trader_thread_function();
    // Signal consumer to exit as needed; omitted for brevity
    consumer.join();
}
