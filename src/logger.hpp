#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>

// Debug logging control
#ifndef DEBUG_LOGGING
#define DEBUG_LOGGING 1
#endif

class Logger {
public:
    enum class Level : uint8_t {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4
    };

    static const char* level_to_string(Level level) {
        switch (level) {
            case Level::TRACE: return "TRACE";
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO";
            case Level::WARNING: return "WARNING";
            case Level::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    template<typename... Args>
    static void debug(Args... args) {
        #if DEBUG_LOGGING
        log(Level::DEBUG, args...);
        #endif
    }

    template<typename... Args>
    static void trace(Args... args) {
        #if DEBUG_LOGGING
        log(Level::TRACE, args...);
        #endif
    }

    template<typename... Args>
    static void info(Args... args) {
        log(Level::INFO, args...);
    }

    template<typename... Args>
    static void warning(Args... args) {
        log(Level::WARNING, args...);
    }

    template<typename... Args>
    static void error(Args... args) {
        log(Level::ERROR, args...);
    }

private:
    static inline std::mutex log_mutex;

    template<typename T>
    static void append_to_stream(std::stringstream& ss, T&& arg) {
        ss << std::forward<T>(arg);
    }

    template<typename T, typename... Args>
    static void append_to_stream(std::stringstream& ss, T&& arg, Args&&... args) {
        ss << std::forward<T>(arg);
        append_to_stream(ss, std::forward<Args>(args)...);
    }

    static std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    template<typename... Args>
    static void log(Level level, Args&&... args) {
        std::stringstream ss;
        ss << "[" << get_timestamp() << "] "
           << "[" << level_to_string(level) << "] ";
        
        append_to_stream(ss, std::forward<Args>(args)...);
        std::string output = ss.str();

        std::lock_guard<std::mutex> lock(log_mutex);
        if (level == Level::ERROR || level == Level::WARNING) {
            std::cerr << output << std::endl;
        } else {
            std::cout << output << std::endl;
        }
    }
};
