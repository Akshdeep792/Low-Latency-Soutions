#include "ultralogger.h"

LoggerManager &LoggerManager::instance()
{
    static LoggerManager inst;
    return inst;
}

LoggerManager::LoggerManager()
{
    consumer_thread = std::thread([this]()
                                  { run(); });
}

int LoggerManager::create_log_file(std::string filename, int file_id)
{
    return creating_log_file(filename, file_id);
}

void LoggerManager::stop()
{
    _stop_flag = true;
}

void LoggerManager::run()
{
    while (!_stop_flag)
    {
        // Poll every thread buffer for logs
        logger.for_each_buffer([this](ThreadRingBuffer *buf)
                               {
            LogMsg msg;
            while (buf->try_consume(msg)) {
                write_log(msg);
            } });

        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    flush_all_files();
}

int LoggerManager::creating_log_file(std::string file, int file_id)
{
    auto ofs = new std::ofstream();
    std::ostringstream filename;
    filename << "log_" << file << ".log";
    ofs->open(filename.str(), std::ios::out | std::ios::app);
    ofs->rdbuf()->pubsetbuf(nullptr, 0); // Unbuffered
    _file_map[file_id] = ofs;
    return file_id;
}

void LoggerManager::write_log(const LogMsg &msg)
{
    std::ofstream &ofs = get_file_stream(msg.log_id);
    ofs << "[" << msg.timestamp << "][T" << msg.thread_id << "] ";
    for (size_t i = 0; i < msg.arg_count; ++i)
    {
        const LogArg &a = msg.args[i];
        switch (a.type)
        {
        case LogArgType::I64:
            ofs << a.i64 << " ";
            break;
        case LogArgType::U64:
            ofs << a.u64 << " ";
            break;
        case LogArgType::F64:
            ofs << std::setprecision(15) << a.f64 << " ";
            break;
        case LogArgType::CString:
            ofs.write(a.cstr, a.cstr_len);
            ofs << " ";
            break;
        case LogArgType::Pointer:
            ofs << a.ptr << " ";
            break;
        }
    }
    ofs << "\n";
}

std::ofstream &LoggerManager::get_file_stream(uint32_t log_id)
{
    std::lock_guard<std::mutex> lock(_file_map_mutex);
    auto it = _file_map.find(log_id);
    return *(it->second);
}

void LoggerManager::flush_all_files()
{
    std::lock_guard<std::mutex> lock(_file_map_mutex);
    for (auto &kv : _file_map)
    {
        kv.second->flush();
        kv.second->close();
        delete kv.second;
    }
    _file_map.clear();
    consumer_thread.join();
}
