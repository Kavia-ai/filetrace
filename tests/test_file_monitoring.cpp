#include <gtest/gtest.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include "../src/path_utils.hpp"

class FileMonitoringTest : public ::testing::Test {
protected:
    struct stat stat_file(const std::filesystem::path& path) {
        struct stat file_stat;
        EXPECT_EQ(stat(path.c_str(), &file_stat), 0) 
            << "Failed to stat file: " << path;
        return file_stat;
    }
    void SetUp() override {
        // Create test files and directories
        test_dir = std::filesystem::temp_directory_path() / "filetrace_test";
        std::filesystem::create_directory(test_dir);
        
        // Create existing file
        existing_file = test_dir / "existing.txt";
        std::ofstream(existing_file) << "test content";
        
        // Create symlink
        symlink_file = test_dir / "symlink.txt";
        std::filesystem::create_symlink(existing_file, symlink_file);
    }

    void TearDown() override {
        // Cleanup test files and directories
        std::filesystem::remove_all(test_dir);
    }

    std::filesystem::path test_dir;
    std::filesystem::path existing_file;
    std::filesystem::path symlink_file;
    std::filesystem::path nonexistent_file;
};

// Test tracking of existing files
TEST_F(FileMonitoringTest, ExistingFileValidation) {
    struct stat file_stat;
    ASSERT_EQ(stat(existing_file.c_str(), &file_stat), 0) 
        << "Existing file should be accessible via stat()";
    ASSERT_TRUE(S_ISREG(file_stat.st_mode)) 
        << "Existing file should be a regular file";
}

// Test filtering of non-existent files
TEST_F(FileMonitoringTest, NonExistentFileValidation) {
    nonexistent_file = test_dir / "nonexistent.txt";
    struct stat file_stat;
    ASSERT_NE(stat(nonexistent_file.c_str(), &file_stat), 0) 
        << "Non-existent file should not be accessible via stat()";
    ASSERT_EQ(errno, ENOENT) 
        << "Errno should indicate file not found";
}

// Test handling of symbolic links
TEST_F(FileMonitoringTest, SymbolicLinkValidation) {
    struct stat file_stat;
    struct stat link_stat;
    
    // Test that symlink exists and points to the right file
    ASSERT_EQ(lstat(symlink_file.c_str(), &link_stat), 0) 
        << "Symlink should be accessible via lstat()";
    ASSERT_TRUE(S_ISLNK(link_stat.st_mode)) 
        << "File should be a symbolic link";
    
    // Test that we can follow the symlink
    ASSERT_EQ(stat(symlink_file.c_str(), &file_stat), 0) 
        << "Should be able to stat through symlink";
    ASSERT_TRUE(S_ISREG(file_stat.st_mode)) 
        << "Target of symlink should be a regular file";
    
    // Verify symlink points to the right file
    ASSERT_EQ(file_stat.st_ino, stat_file(existing_file).st_ino) 
        << "Symlink should point to the existing file";
}

// Test concurrent file operations
TEST_F(FileMonitoringTest, ConcurrentFileOperations) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    const int num_threads = 10;
    const int ops_per_thread = 100;
    
    // Create multiple threads that perform file operations
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops_per_thread; j++) {
                struct stat file_stat;
                if (stat(existing_file.c_str(), &file_stat) == 0) {
                    success_count++;
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all operations were successful
    ASSERT_EQ(success_count, num_threads * ops_per_thread) 
        << "All concurrent file stat operations should succeed";
}

