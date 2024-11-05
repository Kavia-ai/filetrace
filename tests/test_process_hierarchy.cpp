#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <fcntl.h>
#include "directory_tree.hpp"

class ProcessHierarchyTest : public ::testing::Test {
protected:
    DirectoryTree tree;
    std::string test_file_path;
    
    void SetUp() override {
        tree = DirectoryTree();
        test_file_path = "/tmp/test_file.txt";
    }
    
    void TearDown() override {
        unlink(test_file_path.c_str());
    }
    
    bool file_exists(const std::string& path) {
        return access(path.c_str(), F_OK) != -1;
    }
};

TEST_F(ProcessHierarchyTest, TestForkProcessFileAccess) {
    pid_t child_pid = fork();
    
    if (child_pid == 0) {  // Child process
        // Simulate file access in child process
        std::stringstream ss;
        tree.insert_file("/test/child_file.txt", 1, getpid(), "child");
        tree.generate_html(ss);
        
        std::string output = ss.str();
        EXPECT_TRUE(output.find("child_file.txt") != std::string::npos);
        EXPECT_TRUE(output.find(std::to_string(getpid())) != std::string::npos);
        exit(0);
    } else {  // Parent process
        // Wait for child to complete
        int status;
        waitpid(child_pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
        
        // Verify parent process can still access tree
        std::stringstream ss;
        tree.insert_file("/test/parent_file.txt", 2, getpid(), "parent");
        tree.generate_html(ss);
        
        std::string output = ss.str();
        EXPECT_TRUE(output.find("parent_file.txt") != std::string::npos);
        EXPECT_TRUE(output.find(std::to_string(getpid())) != std::string::npos);
    }
}

TEST_F(ProcessHierarchyTest, TestVforkProcessFileAccess) {
    pid_t child_pid = vfork();
    
    if (child_pid == 0) {  // Child process
        // Minimal operations in vfork child
        _exit(0);
    } else {  // Parent process
        // Verify parent process can still access tree after vfork
        std::stringstream ss;
        tree.insert_file("/test/after_vfork.txt", 1, getpid(), "parent");
        tree.generate_html(ss);
        
        std::string output = ss.str();
        EXPECT_TRUE(output.find("after_vfork.txt") != std::string::npos);
        EXPECT_TRUE(output.find(std::to_string(getpid())) != std::string::npos);
    }
}

TEST_F(ProcessHierarchyTest, TestMixedForkAndThreads) {
    // Create a thread in parent process
    std::thread parent_thread([this]() {
        tree.insert_file("/test/parent_thread.txt", 1, 
            static_cast<pid_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())), 
            "parent_thread");
    });
    parent_thread.join();
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {  // Child process
        // Create thread in child process
        std::thread child_thread([this]() {
            tree.insert_file("/test/child_thread.txt", 2,
                static_cast<pid_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())),
                "child_thread");
        });
        child_thread.join();
        
        std::stringstream ss;
        tree.generate_html(ss);
        std::string output = ss.str();
        EXPECT_TRUE(output.find("child_thread.txt") != std::string::npos);
        exit(0);
    } else {  // Parent process
        int status;
        waitpid(child_pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
        
        std::stringstream ss;
        tree.generate_html(ss);
        std::string output = ss.str();
        EXPECT_TRUE(output.find("parent_thread.txt") != std::string::npos);
    }
}

TEST_F(ProcessHierarchyTest, TestProcessCleanup) {
    pid_t child_pid = fork();
    
    if (child_pid == 0) {  // Child process
        tree.insert_file("/test/temp_file.txt", 1, getpid(), "child");
        exit(0);
    } else {  // Parent process
        // Wait for child to exit
        int status;
        waitpid(child_pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        
        // Verify parent can still operate normally
        std::stringstream ss;
        tree.insert_file("/test/cleanup_test.txt", 2, getpid(), "parent");
        tree.generate_html(ss);
        
        std::string output = ss.str();
        EXPECT_TRUE(output.find("cleanup_test.txt") != std::string::npos);
        EXPECT_TRUE(output.find(std::to_string(getpid())) != std::string::npos);
    }
}

TEST_F(ProcessHierarchyTest, TestMultiLevelFork) {
    pid_t first_child = fork();
    
    if (first_child == 0) {  // First child
        pid_t second_child = fork();
        
        if (second_child == 0) {  // Second child
            tree.insert_file("/test/second_child.txt", 1, getpid(), "second_child");
            std::stringstream ss;
            tree.generate_html(ss);
            std::string output = ss.str();
            EXPECT_TRUE(output.find("second_child.txt") != std::string::npos);
            exit(0);
        } else {  // First child after forking
            int status;
            waitpid(second_child, &status, 0);
            EXPECT_TRUE(WIFEXITED(status));
            
            tree.insert_file("/test/first_child.txt", 2, getpid(), "first_child");
            exit(0);
        }
    } else {  // Parent process
        int status;
        waitpid(first_child, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        
        tree.insert_file("/test/parent.txt", 3, getpid(), "parent");
        std::stringstream ss;
        tree.generate_html(ss);
        std::string output = ss.str();
        EXPECT_TRUE(output.find("parent.txt") != std::string::npos);
    }
}

TEST_F(ProcessHierarchyTest, TestFileOperationsInChildProcess) {
    int fd = open(test_file_path.c_str(), O_CREAT | O_WRONLY, 0644);
    ASSERT_NE(fd, -1) << "Failed to create test file";
    close(fd);
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {  // Child process
        // Attempt file operations in child
        fd = open(test_file_path.c_str(), O_RDWR);
        EXPECT_NE(fd, -1) << "Child process failed to open file";
        
        const char* test_data = "test data from child";
        write(fd, test_data, strlen(test_data));
        close(fd);
        
        tree.insert_file(test_file_path, 1, getpid(), "child");
        exit(0);
    } else {  // Parent process
        int status;
        waitpid(child_pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        
        // Verify file exists and was modified
        EXPECT_TRUE(file_exists(test_file_path));
        
        char buffer[100] = {0};
        fd = open(test_file_path.c_str(), O_RDONLY);
        ASSERT_NE(fd, -1) << "Parent failed to open file";
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer)-1);
        close(fd);
        
        EXPECT_GT(bytes_read, 0);
        EXPECT_STREQ(buffer, "test data from child");
    }
}

TEST_F(ProcessHierarchyTest, TestConcurrentFileAccess) {
    std::atomic<int> ready_threads{0};
    std::vector<std::thread> threads;
    const int num_threads = 4;
    
    pid_t child_pid = fork();
    
    if (child_pid == 0) {  // Child process
        // Create multiple threads in child process
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([this, i, &ready_threads]() {
                ready_threads++;
                while (ready_threads < num_threads) {
                    std::this_thread::yield();
                }
                
                std::string filename = "/test/child_thread_" + std::to_string(i) + ".txt";
                tree.insert_file(filename, i+1,
                    static_cast<pid_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())),
                    "child_thread_" + std::to_string(i));
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::stringstream ss;
        tree.generate_html(ss);
        std::string output = ss.str();
        
        for (int i = 0; i < num_threads; i++) {
            EXPECT_TRUE(output.find("child_thread_" + std::to_string(i)) != std::string::npos);
        }
        
        exit(0);
    } else {  // Parent process
        int status;
        waitpid(child_pid, &status, 0);
        EXPECT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
        
        // Create threads in parent process
        ready_threads = 0;
        threads.clear();
        
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([this, i, &ready_threads]() {
                ready_threads++;
                while (ready_threads < num_threads) {
                    std::this_thread::yield();
                }
                
                std::string filename = "/test/parent_thread_" + std::to_string(i) + ".txt";
                tree.insert_file(filename, i+1,
                    static_cast<pid_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())),
                    "parent_thread_" + std::to_string(i));
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::stringstream ss;
        tree.generate_html(ss);
        std::string output = ss.str();
        
        for (int i = 0; i < num_threads; i++) {
            EXPECT_TRUE(output.find("parent_thread_" + std::to_string(i)) != std::string::npos);
        }
    }
}
