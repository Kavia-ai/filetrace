#include &lt;gtest/gtest.h&gt;
#include &lt;gmock/gmock.h&gt;
#include &lt;thread&gt;
#include &lt;vector&gt;
#include &lt;regex&gt;
#include &lt;sstream&gt;
#include "../src/logger.hpp"

// Test fixture for Logger tests
class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redirect stdout and stderr for testing
        old_cout = std::cout.rdbuf(cout_buffer.rdbuf());
        old_cerr = std::cerr.rdbuf(cerr_buffer.rdbuf());
    }

    void TearDown() override {
        // Restore stdout and stderr
        std::cout.rdbuf(old_cout);
        std::cerr.rdbuf(old_cerr);
    }

    // Helper function to get output with normalized timestamps
    std::string getNormalizedOutput(const std::string& output) {
        std::regex timestamp_regex(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\])");
        return std::regex_replace(output, timestamp_regex, "[TIMESTAMP]");
    }

    std::stringstream cout_buffer;
    std::stringstream cerr_buffer;
    std::streambuf* old_cout;
    std::streambuf* old_cerr;
};

// Test severity levels and message routing
TEST_F(LoggerTest, SeverityLevelsAndRouting) {
    Logger::trace("Trace message");
    Logger::debug("Debug message");
    Logger::info("Info message");
    Logger::warning("Warning message");
    Logger::error("Error message");

    std::string cout_output = getNormalizedOutput(cout_buffer.str());
    std::string cerr_output = getNormalizedOutput(cerr_buffer.str());

    // Check stdout messages
    EXPECT_THAT(cout_output, testing::HasSubstr("[TIMESTAMP] [TRACE] Trace message"));
    EXPECT_THAT(cout_output, testing::HasSubstr("[TIMESTAMP] [DEBUG] Debug message"));
    EXPECT_THAT(cout_output, testing::HasSubstr("[TIMESTAMP] [INFO] Info message"));

    // Check stderr messages
    EXPECT_THAT(cerr_output, testing::HasSubstr("[TIMESTAMP] [WARNING] Warning message"));
    EXPECT_THAT(cerr_output, testing::HasSubstr("[TIMESTAMP] [ERROR] Error message"));
}

// Test thread safety with concurrent logging
TEST_F(LoggerTest, ThreadSafety) {
    const int num_threads = 10;
    const int messages_per_thread = 100;
    std::vector&lt;std::thread&gt; threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                Logger::info("Thread ", i, " Message ", j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::string output = cout_buffer.str();
    int message_count = 0;
    std::string::size_type pos = 0;
    while ((pos = output.find("[INFO]", pos)) != std::string::npos) {
        message_count++;
        pos++;
    }

    EXPECT_EQ(message_count, num_threads * messages_per_thread);
}

// Test message formatting with variadic templates
TEST_F(LoggerTest, MessageFormatting) {
    Logger::info("Test ", 42, " ", 3.14, " ", true, " ", "string");
    std::string output = getNormalizedOutput(cout_buffer.str());
    EXPECT_THAT(output, testing::HasSubstr("[TIMESTAMP] [INFO] Test 42 3.14 1 string"));
}

// Test empty message handling
TEST_F(LoggerTest, EmptyMessage) {
    Logger::info("");
    std::string output = getNormalizedOutput(cout_buffer.str());
    EXPECT_THAT(output, testing::HasSubstr("[TIMESTAMP] [INFO] "));
}

// Test large message handling
TEST_F(LoggerTest, LargeMessage) {
    std::string large_message(8192, 'X');  // 8KB message
    Logger::info(large_message);
    std::string output = cout_buffer.str();
    EXPECT_EQ(output.find(large_message), std::string::npos ? false : true);
}

// Test timestamp format
TEST_F(LoggerTest, TimestampFormat) {
    Logger::info("Test message");
    std::string output = cout_buffer.str();
    std::regex timestamp_regex(R"(\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}\])");
    EXPECT_TRUE(std::regex_search(output, timestamp_regex));
}

// Test level to string conversion
TEST_F(LoggerTest, LevelToString) {
    EXPECT_STREQ(Logger::level_to_string(Logger::Level::TRACE), "TRACE");
    EXPECT_STREQ(Logger::level_to_string(Logger::Level::DEBUG), "DEBUG");
    EXPECT_STREQ(Logger::level_to_string(Logger::Level::INFO), "INFO");
    EXPECT_STREQ(Logger::level_to_string(Logger::Level::WARNING), "WARNING");
    EXPECT_STREQ(Logger::level_to_string(Logger::Level::ERROR), "ERROR");
}

// Test DEBUG_LOGGING macro functionality
TEST_F(LoggerTest, DebugLoggingMacro) {
    Logger::debug("Debug message");
    Logger::trace("Trace message");
    
    std::string output = cout_buffer.str();
    #if DEBUG_LOGGING
        EXPECT_THAT(output, testing::HasSubstr("Debug message"));
        EXPECT_THAT(output, testing::HasSubstr("Trace message"));
    #else
        EXPECT_THAT(output, testing::Not(testing::HasSubstr("Debug message")));
        EXPECT_THAT(output, testing::Not(testing::HasSubstr("Trace message")));
    #endif
}