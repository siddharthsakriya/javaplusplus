// src/Logger.cpp
#include "Logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

void Logger::init(const std::string_view filepath) {
    std::lock_guard<std::mutex> lock(log_mutex);
    file_stream.open(std::string(filepath), std::ios::out | std::ios::app);
    if (!file_stream.is_open()) {
        std::cerr << "[Agent] Failed to open log file: " << filepath << "\n";
    }
}

std::string getLogLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, std::string_view message) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);

    std::string level_str = getLogLevelString(level);

    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&now_time_t), "%H:%M:%S") << "] "
        << "[" << level_str << "] " << message << "\n";

    std::string formatted = oss.str();

    std::cerr << formatted;

    if (file_stream.is_open()) {
        file_stream << formatted;
        file_stream.flush();
    }
}

