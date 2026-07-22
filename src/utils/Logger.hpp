// src/Logger.hpp
#pragma once
#include <fstream>
#include <string>
#include <mutex>

enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

class Logger {
    public:
        static Logger& getInstance() {
            static Logger logger;
            return logger;
        }

        void init(const std::string_view filepath);
        void log(LogLevel level, std::string_view message);

    private:
        Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        std::ofstream file_stream;
        std::mutex log_mutex;   
};


// macros for convenience 
#define LOG_INFO(msg) Logger::getInstance().log(LogLevel::INFO, msg)
#define LOG_WARN(msg) Logger::getInstance().log(LogLevel::WARNING, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)