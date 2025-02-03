{"is_source_file": true, "format": "C++", "description": "Header file for a Logger class that provides logging functionality with different levels of severity.", "external_files": [], "external_methods": [], "published": [], "classes": [{"name": "Logger", "description": "A class that provides methods for logging messages at various levels (TRACE, DEBUG, INFO, WARNING, ERROR) with thread-safe access."}], "methods": [{"name": "level_to_string", "description": "Converts the logging level enum to a string representation."}, {"name": "debug", "description": "Logs a message at the DEBUG level."}, {"name": "trace", "description": "Logs a message at the TRACE level."}, {"name": "info", "description": "Logs a message at the INFO level."}, {"name": "warning", "description": "Logs a message at the WARNING level."}, {"name": "error", "description": "Logs a message at the ERROR level."}, {"name": "get_timestamp", "description": "Generates a timestamp string for logging."}, {"name": "log", "description": "Logs a message at the specified log level with the provided arguments."}], "calls": ["std::lock_guard<std::mutex>", "std::localtime", "std::chrono::system_clock::now", "std::chrono::system_clock::to_time_t", "std::chrono::duration_cast", "std::ostream::operator<<"], "search-terms": ["Logger", "Level", "debug logging", "log levels"], "state": 2, "ctags": [], "filename": "/app/data/T0002/43087/Kavia-ai/filetrace/src/logger.hpp", "hash": "43a91df4dc9ad3afbe5d572c204e429c", "format-version": 3, "code-base-name": "b0134v6"}