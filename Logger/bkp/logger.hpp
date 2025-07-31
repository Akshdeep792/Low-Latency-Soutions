// logger.hpp
#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <vector>
#include <cstring>
#include <cassert>
#include <utility>
#include <iostream>
#include <chrono>
#include <functional>
#include <type_traits>

// Settings (tune as needed)
constexpr size_t LOGGER_RING_SIZE = 2048; // Log buffer per thread
constexpr size_t LOGGER_MAX_ARGS = 8;     // Maximum number of arguments per log
constexpr size_t LOGGER_MAX_THREADS = 64; // Upperbound on concurrent loggers/threads

// Helper for cache-line alignment
#define CACHE_ALIGN alignas(64)

// Timestamp: rdtsc for minimal overhead
inline uint64_t read_tsc()
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    return __rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t hi, lo;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
#else
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
#endif
}

// Argument pod wrapper
enum class LogArgType : uint8_t
{
    I64,
    U64,
    F64,
    CString,
    Pointer
};

struct LogArg
{
    LogArgType type;
    union
    {
        int64_t i64;
        uint64_t u64;
        double f64;
        const char *cstr; // user guarantees lifetime
        const void *ptr;
    };
    size_t cstr_len; // for strings only
};

struct CACHE_ALIGN LogMsg
{
    uint64_t timestamp;
    const char *fmt;
    LogArg args[LOGGER_MAX_ARGS];
    uint8_t arg_count;
    uint32_t thread_id;
    uint32_t msg_id;
};

class ThreadRingBuffer
{
public:
    ThreadRingBuffer(uint32_t tid) : _write_idx(0), _read_idx(0), _thread_id(tid), _data{}
    {
        static_assert((LOGGER_RING_SIZE & (LOGGER_RING_SIZE - 1)) == 0, "Ring size must be power of 2");
    }

    // Non-blocking, single-threaded producer
    template <typename... Args>
    void emplace(const char *fmt, Args &&...args)
    {
        static_assert(sizeof...(Args) <= LOGGER_MAX_ARGS, "Too many args for logger");
        uint64_t idx = _write_idx.fetch_add(1, std::memory_order_relaxed);
        LogMsg &msg = _data[idx & (LOGGER_RING_SIZE - 1)];
        msg.timestamp = read_tsc();
        msg.fmt = fmt;
        msg.arg_count = 0;
        msg.thread_id = _thread_id;
        msg.msg_id = static_cast<uint32_t>(idx);

        // POD/reference-only args enforced by user contract
        fill_args(msg, std::forward<Args>(args)...);
    }

    bool try_consume(LogMsg &out)
    {
        uint64_t read_idx = _read_idx.load(std::memory_order_relaxed);
        uint64_t write_idx = _write_idx.load(std::memory_order_acquire);
        if (read_idx == write_idx)
            return false;

        out = _data[read_idx & (LOGGER_RING_SIZE - 1)];
        _read_idx.fetch_add(1, std::memory_order_release);
        return true;
    }

private:
    template <typename T, typename... Rest>
    void fill_args(LogMsg &msg, T &&first, Rest &&...rest)
    {
        set_arg(msg, msg.arg_count++, std::forward<T>(first));
        fill_args(msg, std::forward<Rest>(rest)...);
    }
    void fill_args(LogMsg &) {}

    // Overloads for supported types (add as needed)
    void set_arg(LogMsg &msg, size_t idx, int64_t val)
    {
        msg.args[idx].type = LogArgType::I64;
        msg.args[idx].i64 = val;
    }
    void set_arg(LogMsg &msg, size_t idx, uint64_t val)
    {
        msg.args[idx].type = LogArgType::U64;
        msg.args[idx].u64 = val;
    }
    void set_arg(LogMsg &msg, size_t idx, double val)
    {
        msg.args[idx].type = LogArgType::F64;
        msg.args[idx].f64 = val;
    }
    void set_arg(LogMsg &msg, size_t idx, const char *val)
    {
        msg.args[idx].type = LogArgType::CString;
        msg.args[idx].cstr = val;
        msg.args[idx].cstr_len = std::strlen(val);
    }
    void set_arg(LogMsg &msg, size_t idx, void *val)
    {
        msg.args[idx].type = LogArgType::Pointer;
        msg.args[idx].ptr = val;
    }
    // Add more type support as needed
    template <typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, int64_t>::value && !std::is_same<T, uint64_t>::value, void>::type
    set_arg(LogMsg &msg, size_t idx, T val)
    {
        msg.args[idx].type = LogArgType::I64;
        msg.args[idx].i64 = static_cast<int64_t>(val);
    }
    std::atomic<uint64_t> _write_idx;
    std::atomic<uint64_t> _read_idx;
    uint32_t _thread_id;
    std::array<LogMsg, LOGGER_RING_SIZE> _data;
};

// Global registry to map threads to their local buffers
class Logger
{
public:
    static Logger &instance()
    {
        static Logger inst;
        return inst;
    }

    ThreadRingBuffer *get_buffer()
    {
        thread_local uint32_t tid = _register_thread();
        thread_local ThreadRingBuffer *buf = _get_or_create_buffer(tid);
        return buf;
    }

    void for_each_buffer(const std::function<void(ThreadRingBuffer *)> &f)
    {
        for (size_t i = 0; i < LOGGER_MAX_THREADS; ++i)
        {
            ThreadRingBuffer *buf = _buffers[i];
            if (buf)
                f(buf);
        }
    }

private:
    Logger() : _tid_counter(0), _buffers{} {}

    uint32_t _register_thread()
    {
        static thread_local uint32_t myid = _tid_counter.fetch_add(1, std::memory_order_relaxed);
        assert(myid < LOGGER_MAX_THREADS);
        return myid;
    }
    ThreadRingBuffer *_get_or_create_buffer(uint32_t tid)
    {
        if (!_buffers[tid])
            _buffers[tid] = new ThreadRingBuffer(tid);
        return _buffers[tid];
    }

    std::atomic<uint32_t> _tid_counter;
    ThreadRingBuffer *_buffers[LOGGER_MAX_THREADS];
};
// Public logging API: forwards to local ring buffer

template <typename... Args>
inline void log(const char *fmt, Args &&...args)
{
    Logger::instance().get_buffer()->emplace(fmt, std::forward<Args>(args)...);
}
