// consumer.hpp
#include "logger.hpp"
// Implement your sink here; e.g., to terminal, disk, or remote.
void log_consumer_thread()
{
    std::vector<LogMsg> batch(LOGGER_RING_SIZE * LOGGER_MAX_THREADS);

    while (true)
    {
        // Optionally: sleep or spin based on config.
        Logger::instance().for_each_buffer([&](ThreadRingBuffer *buf)
                                           {
            LogMsg msg;
            while (buf->try_consume(msg)) {
                // Example: format data using msg.fmt and msg.args
                // This is where you do real formatting; no heap/locking on producer.
                // std::cout << "[" << msg.timestamp << "][T" << msg.thread_id << "] ";
                // // You could use fmtlib, snprintf, or custom logic here.
                // // Example: just print argument info:
                // for (size_t i = 0; i < msg.arg_count; ++i) {
                //     switch (msg.args[i].type) {
                //         case LogArgType::I64:     std::cout << msg.args[i].i64 << " "; break;
                //         case LogArgType::U64:     std::cout << msg.args[i].u64 << " "; break;
                //         case LogArgType::F64:     std::cout << msg.args[i].f64 << " "; break;
                //         case LogArgType::CString: std::cout << msg.args[i].cstr << " "; break;
                //         case LogArgType::Pointer: std::cout << msg.args[i].ptr << " "; break;
                //     }
                // }
                // std::cout << std::endl;
            } });
        // Tune: yield/sleep to balance CPU and catch-up latency.
        // std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}
