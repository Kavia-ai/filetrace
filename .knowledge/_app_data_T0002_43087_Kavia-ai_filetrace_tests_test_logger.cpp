{"is_source_file": true, "format": "C++", "description": "Unit tests for the Logger class using Google Test framework.", "external_files": ["../src/logger.hpp", "<gtest/gtest.h>", "<gmock/gmock.h>", "<thread>", "<vector>", "<regex>", "<sstream>"], "external_methods": [], "published": [], "classes": [{"name": "LoggerTest", "description": "Test fixture class for Logger testing that sets up and tears down stdout and stderr redirection."}], "methods": [{"name": "getNormalizedOutput", "description": "Normalizes timestamps in the output string to a standard format."}], "calls": ["Logger::trace", "Logger::debug", "Logger::info", "Logger::warning", "Logger::error", "Logger::level_to_string"], "search-terms": ["LoggerTest", "message routing", "thread safety", "message formatting", "DEBUG_LOGGING macro"], "state": 2, "ctags": [], "filename": "/app/data/T0002/43087/Kavia-ai/filetrace/tests/test_logger.cpp", "hash": "7ed86355abaf639308aa6a72b72dcd05", "format-version": 3, "code-base-name": "b0134v6"}