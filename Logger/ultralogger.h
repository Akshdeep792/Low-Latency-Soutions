#pragma once
#include "logger.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

class LoggerManager
{
public:
    static LoggerManager &instance();

    // Template function stays in header
    template <typename... Args>
    inline void log(uint32_t log_id, const char *fmt, Args &&...args)
    {
        logger.get_buffer()->emplace(log_id, fmt, std::forward<Args>(args)...);
    }

    int create_log_file(std::string filename, int file_id);
    void stop();

    Logger logger;
    std::thread consumer_thread;

private:
    LoggerManager();
    void run();

    int creating_log_file(std::string file, int file_id);
    void write_log(const LogMsg &msg);
    std::ofstream &get_file_stream(uint32_t log_id);
    void flush_all_files();

    std::unordered_map<uint32_t, std::ofstream *> _file_map;
    std::mutex _file_map_mutex;
    std::atomic<bool> _stop_flag{false};
};
