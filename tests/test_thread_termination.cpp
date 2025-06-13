#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>
#include <atomic>
#include "directory_tree.hpp"

class ThreadTerminationTest : public ::testing::Test {
protected:
    DirectoryTree tree;
    std::vector<std::thread> threads;
    std::atomic<bool> should_stop{false};
    
    void SetUp() override {
        tree = DirectoryTree();
        should_stop = false;
    }
    
    void TearDown() override {
        should_stop = true;
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    pid_t get_thread_id() {
        return static_cast<pid_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    }
};

TEST_F(ThreadTerminationTest, TestCleanupOnThreadExit) {
    // Create a thread that performs file operations and exits normally
    std::thread worker([this]() {
        pid_t tid = get_thread_id();
        tree.insert_file("/test/worker_file.txt", 1, tid, "worker");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
    
    // Wait for thread to complete
    worker.join();
    
    // Verify the file operation was recorded
    std::stringstream ss;
    tree.generate_html(ss);
    std::string output = ss.str();
    EXPECT_TRUE(output.find("worker_file.txt") != std::string::npos);
}

TEST_F(ThreadTerminationTest, TestMultipleThreadTermination) {
    const int NUM_THREADS = 5;
    std::vector<std::thread> workers;
    
    // Create multiple threads
    for (int i = 0; i < NUM_THREADS; i++) {
        workers.emplace_back([this, i]() {
            pid_t tid = get_thread_id();
            tree.insert_file("/test/thread_" + std::to_string(i) + ".txt", i + 1, tid, 
                           "worker_" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * i));
        });
    }
    
    // Wait for all threads to complete
    for (auto& worker : workers) {
        worker.join();
    }
    
    // Verify all file operations were recorded
    std::stringstream ss;
    tree.generate_html(ss);
    std::string output = ss.str();
    
    for (int i = 0; i < NUM_THREADS; i++) {
        EXPECT_TRUE(output.find("thread_" + std::to_string(i) + ".txt") != std::string::npos);
        EXPECT_TRUE(output.find("worker_" + std::to_string(i)) != std::string::npos);
    }
}

TEST_F(ThreadTerminationTest, TestNestedThreadHierarchy) {
    // Create a parent thread that spawns child threads
    std::thread parent([this]() {
        pid_t parent_tid = get_thread_id();
        tree.insert_file("/test/parent_thread.txt", 1, parent_tid, "parent");
        
        std::vector<std::thread> children;
        for (int i = 0; i < 3; i++) {
            children.emplace_back([this, i]() {
                pid_t child_tid = get_thread_id();
                tree.insert_file("/test/child_" + std::to_string(i) + ".txt", i + 2, 
                               child_tid, "child_" + std::to_string(i));
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
        }
        
        for (auto& child : children) {
            child.join();
        }
    });
    
    parent.join();
    
    // Verify parent and all child operations were recorded
    std::stringstream ss;
    tree.generate_html(ss);
    std::string output = ss.str();
    
    EXPECT_TRUE(output.find("parent_thread.txt") != std::string::npos);
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(output.find("child_" + std::to_string(i) + ".txt") != std::string::npos);
    }
}

TEST_F(ThreadTerminationTest, TestThreadStateTracking) {
    std::atomic<bool> thread_started{false};
    std::atomic<bool> thread_finished{false};
    
    // Create a thread that signals its state
    std::thread worker([this, &thread_started, &thread_finished]() {
        pid_t tid = get_thread_id();
        thread_started = true;
        
        tree.insert_file("/test/tracked_thread.txt", 1, tid, "tracked_worker");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        thread_finished = true;
    });
    
    // Wait for thread to start
    while (!thread_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Verify thread is active
    std::stringstream ss1;
    tree.generate_html(ss1);
    std::string output1 = ss1.str();
    EXPECT_TRUE(output1.find("tracked_thread.txt") != std::string::npos);
    
    worker.join();
    
    // Verify thread completed
    EXPECT_TRUE(thread_finished);
    
    // Verify final state
    std::stringstream ss2;
    tree.generate_html(ss2);
    std::string output2 = ss2.str();
    EXPECT_TRUE(output2.find("tracked_thread.txt") != std::string::npos);
}

TEST_F(ThreadTerminationTest, TestAbnormalThreadTermination) {
    bool exception_caught = false;
    
    // Create a thread that throws an exception
    std::thread worker([this, &exception_caught]() {
        try {
            pid_t tid = get_thread_id();
            tree.insert_file("/test/abnormal_thread.txt", 1, tid, "abnormal_worker");
            throw std::runtime_error("Simulated abnormal termination");
        } catch (const std::exception&) {
            exception_caught = true;
        }
    });
    
    worker.join();
    
    // Verify exception was caught
    EXPECT_TRUE(exception_caught);
    
    // Verify file operation was recorded before termination
    std::stringstream ss;
    tree.generate_html(ss);
    std::string output = ss.str();
    EXPECT_TRUE(output.find("abnormal_thread.txt") != std::string::npos);
}