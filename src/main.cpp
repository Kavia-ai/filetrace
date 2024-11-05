// System headers
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

// Standard library headers
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <mutex>

// Third party libraries
#include <cxxopts.hpp>

// Project headers
#include "path_utils.hpp"
#include "directory_tree.hpp"
#include "html_generator.hpp"
#include "logger.hpp"

// Version information
#define FILETRACE_VERSION "1.0.0"

// Enum to distinguish between processes and threads
enum class ProcessType {
    PROCESS,
    THREAD
};

// Structure to store thread information
struct ThreadInfo {
    pid_t thread_id;
    pid_t parent_pid;
    std::string name;
    bool active;
    ProcessType process_type;
    std::vector<pid_t> child_processes;
    std::vector<pid_t> child_threads;
    time_t creation_time;
    int exit_status;
};

// Structure to store file operation details
struct FileOperation {
    pid_t pid;
    std::string path;
    int sequence;
    pid_t thread_id;
    std::string thread_name;
    bool is_actual_open;  // Distinguish between actual opens and execve lookups
};

// Global thread tracking map and mutex for thread-safe access
std::map<pid_t, ThreadInfo> thread_map;
std::mutex thread_map_mutex;

// Function to get thread name
std::string get_thread_name(pid_t tid) {
    std::stringstream comm_path;
    comm_path << "/proc/" << tid << "/comm";
    std::ifstream comm_file(comm_path.str());
    std::string name;
    if (comm_file.good()) {
        std::getline(comm_file, name);
    } else {
        name = "unknown";
    }
    return name;
}

// Function to handle thread creation
void handle_thread_creation(pid_t parent_pid, pid_t thread_id, bool is_process = false) {
    // Enhanced thread/process creation handling
    std::lock_guard<std::mutex> lock(thread_map_mutex); // Protect thread_map access
    
    // Check if thread already exists in map
    auto existing_it = thread_map.find(thread_id);
    if (existing_it != thread_map.end()) {
        auto& existing = existing_it->second;
        // Update existing thread info if needed
        if (!existing.active) {
            existing.active = true;
            existing.exit_status = -1;
            existing.creation_time = time(nullptr);
            
            // Update parent relationship if changed
            if (existing.parent_pid != parent_pid) {
                // Remove from old parent's children list
                if (existing.parent_pid != 0) {
                    auto old_parent_it = thread_map.find(existing.parent_pid);
                    if (old_parent_it != thread_map.end()) {
                        auto& children = is_process ? 
                            old_parent_it->second.child_processes : 
                            old_parent_it->second.child_threads;
                        children.erase(
                            std::remove(children.begin(), children.end(), thread_id),
                            children.end()
                        );
                    }
                }
                existing.parent_pid = parent_pid;
            }
        }
        return;
    }

    ThreadInfo info;
    info.thread_id = thread_id;
    info.parent_pid = parent_pid;
    info.name = get_thread_name(thread_id);
    info.active = true;
    info.process_type = is_process ? ProcessType::PROCESS : ProcessType::THREAD;
    info.creation_time = time(nullptr);
    info.exit_status = -1;
    
    // Initialize empty vectors for child processes and threads
    info.child_processes = std::vector<pid_t>();
    info.child_threads = std::vector<pid_t>();
    
    // Add this thread/process as a child to its parent
    if (parent_pid != 0) {
        auto parent_it = thread_map.find(parent_pid);
        if (parent_it != thread_map.end()) {
            if (is_process) {
                parent_it->second.child_processes.push_back(thread_id);
            } else {
                parent_it->second.child_threads.push_back(thread_id);
            }
        } else {
            // Create parent entry if it doesn't exist
            ThreadInfo parent_info;
            parent_info.thread_id = parent_pid;
            parent_info.parent_pid = 0;
            parent_info.name = get_thread_name(parent_pid);
            parent_info.active = true;
            parent_info.process_type = ProcessType::PROCESS;
            parent_info.creation_time = time(nullptr);
            parent_info.exit_status = -1;
            parent_info.child_processes = is_process ? 
                std::vector<pid_t>{thread_id} : std::vector<pid_t>();
            parent_info.child_threads = is_process ? 
                std::vector<pid_t>() : std::vector<pid_t>{thread_id};
            thread_map[parent_pid] = parent_info;
        }
    }
    
    thread_map[thread_id] = info;
    
    Logger::debug("Created ", (is_process ? "process" : "thread"), 
                 " ", thread_id, " with parent ", parent_pid);
}

// Function to handle thread exit
void handle_thread_exit(pid_t thread_id, int exit_status = 0) {
    if (thread_map.find(thread_id) == thread_map.end()) {
        return;
    }

    auto& thread_info = thread_map[thread_id];
    
    // Only process if thread is still active
    if (!thread_info.active) {
        return;
    }

    thread_info.active = false;
    thread_info.exit_status = exit_status;
    
    // Recursively handle cleanup of child processes/threads
    for (pid_t child_pid : thread_info.child_processes) {
        if (thread_map.find(child_pid) != thread_map.end() && thread_map[child_pid].active) {
            // For processes, we need to ensure they're properly terminated
            if (thread_map[child_pid].process_type == ProcessType::PROCESS) {
                kill(child_pid, SIGTERM);
            }
            handle_thread_exit(child_pid, -1);
        }
    }
    
    // Handle child threads
    for (pid_t child_tid : thread_info.child_threads) {
        if (thread_map.find(child_tid) != thread_map.end() && thread_map[child_tid].active) {
            handle_thread_exit(child_tid, -1);
        }
    }

    // Clean up ptrace attachment if needed
    ptrace(PTRACE_DETACH, thread_id, nullptr, nullptr);
}

// Function to resolve file descriptor path
std::string resolve_fd_path(pid_t pid, int fd) {
    if (fd == AT_FDCWD) {
        return ".";
    }

    std::stringstream fd_path;
    fd_path << "/proc/" << pid << "/fd/" << fd;
    char buf[PATH_MAX];
    ssize_t len = readlink(fd_path.str().c_str(), buf, sizeof(buf) - 1);
    
    if (len == -1) {
        return "";
    }
    
    buf[len] = '\0';
    return std::string(buf);
}

// Function to resolve relative path for openat
std::string resolve_relative_path(const std::string& base_path, const std::string& relative_path) {
    if (relative_path.empty() || relative_path[0] == '/') {
        return relative_path;
    }
    
    std::string resolved = base_path;
    if (resolved != "/" && resolved.back() != '/') {
        resolved += "/";
    }
    resolved += relative_path;
    return path_utils::normalize_path(resolved);
}

// Function to safely read string from process memory
std::string read_process_string(pid_t pid, unsigned long addr) {
    char buffer[4096];
    size_t i = 0;
    union {
        long val;
        char chars[sizeof(long)];
    } data;

    while (i < sizeof(buffer) - sizeof(long)) {
        errno = 0;
        data.val = ptrace(PTRACE_PEEKDATA, pid, addr + i, nullptr);
        if (errno != 0) {
            break;
        }

        for (size_t j = 0; j < sizeof(long) && i + j < sizeof(buffer) - 1; j++) {
            buffer[i + j] = data.chars[j];
            if (data.chars[j] == '\0') {
                return std::string(buffer);
            }
        }
        i += sizeof(long);
    }
    buffer[i] = '\0';
    return std::string(buffer);
}

// Function to handle system call entry
void handle_syscall_entry(pid_t pid, const user_regs_struct& regs, std::vector<FileOperation>& operations, const std::string& base_dir = "") {
    // Handle clone/fork syscalls for thread tracking
    if (regs.orig_rax == SYS_clone || regs.orig_rax == SYS_fork || regs.orig_rax == SYS_vfork) {
        // Wait for the clone syscall to complete
        if (ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr) == -1) {
            Logger::error("Failed to continue process for clone: ", strerror(errno));
            return;
        }
        
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            Logger::error("Failed to wait for clone: ", strerror(errno));
            return;
        }

        // Get the return value (new thread ID) from RAX register
        struct user_regs_struct post_regs;
        if (ptrace(PTRACE_GETREGS, pid, nullptr, &post_regs) == -1) {
            Logger::error("Failed to get registers after clone: ", strerror(errno));
        } else {
            long new_tid = post_regs.rax;
            if (new_tid > 0) {
                // Check if this is a fork/vfork or a thread creation
                bool is_process = (regs.orig_rax == SYS_fork || regs.orig_rax == SYS_vfork) || 
                                ((regs.orig_rax == SYS_clone) && (regs.rdi & CLONE_THREAD) == 0);
                handle_thread_creation(pid, new_tid, is_process);
            }
        }

        // Resume execution
        if (ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr) == -1) {
            Logger::error("Failed to resume after clone: ", strerror(errno));
        }
    }
    
    // Handle thread/process exit
    if (regs.orig_rax == SYS_exit || regs.orig_rax == SYS_exit_group) {
        int exit_status = regs.rdi;  // First argument contains the exit status
        handle_thread_exit(pid, exit_status);
    }
    
    // Handle file operations
    if (regs.orig_rax == SYS_open || regs.orig_rax == SYS_openat || regs.orig_rax == SYS_execve) {
        std::string filepath;
        
        try {
            if (regs.orig_rax == SYS_open) {
                filepath = read_process_string(pid, regs.rdi);
            } else if (regs.orig_rax == SYS_execve) {
                filepath = read_process_string(pid, regs.rdi);
            } else { // SYS_openat
                int dirfd = regs.rdi;
                filepath = read_process_string(pid, regs.rsi);
                
                // Handle relative paths in openat
                if (!filepath.empty() && filepath[0] != '/') {
                    std::string base_path = resolve_fd_path(pid, dirfd);
                    if (!base_path.empty()) {
                        filepath = resolve_relative_path(base_path, filepath);
                    }
                }
            }
        } catch (const std::exception& e) {
            Logger::error("Failed to read file path from process memory: ", e.what());
            return;
        }
        
        if (!filepath.empty()) {
            // Normalize the filepath
            std::string normalized_path = path_utils::normalize_path(filepath);
            
            // Skip if base_dir is specified and path is not within it
            if (!path_utils::disable_directory_filtering && !base_dir.empty()) {
                Logger::debug("Checking path: ", normalized_path, " against base: ", base_dir);
                if (!path_utils::is_within_directory(normalized_path, base_dir)) {
                    Logger::debug("Skipping file outside base directory: ", normalized_path);
                    return;
                }
            }
            
            // Only record actual file opens, not execve lookups
            if (regs.orig_rax != SYS_execve) {
                // Check if file exists before adding to tracking
                struct stat file_stat;
                if (stat(normalized_path.c_str(), &file_stat) == 0) {
                    FileOperation op;
                    op.pid = pid;
                    op.path = normalized_path;
                    op.sequence = operations.size() + 1;
                    op.thread_id = pid;
                    op.is_actual_open = true;  // This is an actual open operation
                    
                    // Get thread name from thread map
                    if (thread_map.find(pid) != thread_map.end()) {
                        op.thread_name = thread_map[pid].name;
                    } else {
                        // If thread not in map, create new entry
                        handle_thread_creation(pid, pid);
                        op.thread_name = thread_map[pid].name;
                    }
                    
                    Logger::debug("Adding file operation: ", op.path, " [", op.sequence, "]");
                    operations.push_back(op);
                } else {
                    Logger::debug("Skipping non-existent file: ", normalized_path);
                }
            }
        }
    }
}

// Function to generate HTML visualization
void generate_html_output(const std::vector<FileOperation>& operations, const std::string& output_file) {
    // Create directory tree
    DirectoryTree dir_tree;
    Logger::info("Generating HTML output with ", operations.size(), " operations:");
    for (const auto& op : operations) {
        Logger::debug("  - ", op.path, " [", op.sequence, "]");
        dir_tree.insert_file(op.path, op.sequence, op.thread_id, op.thread_name);
    }
    
    // Generate HTML using the HtmlGenerator
    if (!HtmlGenerator::generate_html_report(dir_tree, output_file)) {
        Logger::error("Failed to generate HTML report: ", HtmlGenerator::get_last_error());
    }
}

// Function to validate output file path
bool validate_output_file(const std::string& path) {
    try {
        // Check if path is empty
        if (path.empty()) {
            Logger::error("Output file path cannot be empty");
            return false;
        }

        // Create filesystem path and extract parent directory
        std::filesystem::path fs_path(path);
        std::filesystem::path parent_path = fs_path.parent_path();
        
        // Handle empty parent path (current directory)
        if (parent_path.empty()) {
            parent_path = std::filesystem::current_path();
            Logger::debug("Using current directory as parent path");
        }

        // Check if parent directory exists
        if (!std::filesystem::exists(parent_path)) {
            Logger::error("Parent directory does not exist: ", parent_path.string());
            return false;
        }

        // Verify parent path is a directory
        if (!std::filesystem::is_directory(parent_path)) {
            Logger::error("Parent path is not a directory: ", parent_path.string());
            return false;
        }

        // Check directory permissions using filesystem status
        try {
            auto perms = std::filesystem::status(parent_path).permissions();
            if ((perms & std::filesystem::perms::owner_write) == std::filesystem::perms::none &&
                (perms & std::filesystem::perms::group_write) == std::filesystem::perms::none &&
                (perms & std::filesystem::perms::others_write) == std::filesystem::perms::none) {
                Logger::error("Parent directory is not writable: ", parent_path.string());
                return false;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::error("Failed to check directory permissions: ", e.what());
            return false;
        }
        
        Logger::debug("Output file validation successful: ", path);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Filesystem error validating output file: ", e.what());
        return false;
    } catch (const std::exception& e) {
        Logger::error("Error validating output file: ", e.what());
        return false;
    } catch (...) {
        Logger::error("Unknown error validating output file");
        return false;
    }
}

// Function to validate executable command
bool validate_command(const std::string& command) {
    try {
        // Check if command is empty
        if (command.empty()) {
            Logger::error("Command cannot be empty");
            return false;
        }

        // If command contains a path separator, check directly
        if (command.find('/') != std::string::npos) {
            if (access(command.c_str(), X_OK) == 0) {
                Logger::debug("Command validation successful (direct path): ", command);
                return true;
            }
            Logger::error("Command not executable: ", command);
            return false;
        }

        // Search in PATH
        char* path_env = getenv("PATH");
        if (!path_env) {
            Logger::error("PATH environment variable not set");
            return false;
        }

        std::istringstream path_stream(path_env);
        std::string path;
        while (std::getline(path_stream, path, ':')) {
            if (path.empty()) continue;
            
            std::string full_path = path + "/" + command;
            if (access(full_path.c_str(), X_OK) == 0) {
                Logger::debug("Command validation successful (PATH): ", full_path);
                return true;
            }
        }

        Logger::error("Command not found in PATH: ", command);
        return false;
    } catch (const std::exception& e) {
        Logger::error("Error validating command: ", e.what());
        return false;
    } catch (...) {
        Logger::error("Unknown error validating command");
        return false;
    }
}

// Function to display help message
void display_help(const cxxopts::Options& options) {
    std::cout << options.help() << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  filetrace ls -l                                  # Basic usage" << std::endl;
    std::cout << "  filetrace --output-html trace.html gcc -c file.c # Custom output file" << std::endl;
    std::cout << "  filetrace -a make                               # Show all files" << std::endl;
    std::cout << "  filetrace -d /path/to/dir ls                    # Filter files in directory" << std::endl;
    std::cout << "  filetrace -- ./script.sh                        # Trace a script" << std::endl;
}

int main(int argc, char* argv[]) try {
        cxxopts::Options options("filetrace", "Thread-aware File Access Visualizer");
        
        // Enhanced command line options
        options.add_options()
            ("o,output-html", "Specify output HTML file (default: filetrace_output.html)", 
             cxxopts::value<std::string>()->default_value("filetrace_output.html"))
            ("a,all", "Show all files (disable directory filtering)")
            ("d,directory", "Base directory for file filtering (default: current directory)",
             cxxopts::value<std::string>())
            ("h,help", "Display this help message")
            ("v,version", "Display version information")
            ("command", "Command to execute", cxxopts::value<std::vector<std::string>>())
            ;

        options.positional_help("<command> [args...]");
        options.parse_positional({"command"});
        options.allow_unrecognised_options();

        try {
            // Parse command line options
            auto result = options.parse(argc, argv);

            // Handle help and version requests first
            if (result.count("help")) {
                display_help(options);
                return 0;
            }

            if (result.count("version")) {
                std::cout << "FileTrace version " << FILETRACE_VERSION << std::endl;
                return 0;
            }

            // Validate command presence
            if (!result.count("command")) {
                Logger::error("Error: No command specified");
                display_help(options);
                return 1;
            }

            // Process output file option
            std::string output_file = result["output-html"].as<std::string>();
            if (!validate_output_file(output_file)) {
                Logger::error("Error: Cannot write to output file: ", output_file);
                Logger::error("Please ensure the directory exists and you have write permissions");
                return 1;
            }

            // Process directory filtering options
            path_utils::disable_directory_filtering = result.count("all") > 0;
            std::string base_dir;
            if (result.count("directory")) {
                base_dir = result["directory"].as<std::string>();
                if (!std::filesystem::exists(base_dir)) {
                    Logger::error("Error: Base directory does not exist: ", base_dir);
                    return 1;
                }
                base_dir = std::filesystem::canonical(base_dir).string();
            } else {
                base_dir = path_utils::get_current_directory();
            }

            // Process command and arguments
            std::vector<std::string> command = result["command"].as<std::vector<std::string>>();
            if (command.empty()) {
                Logger::error("Error: Empty command specified");
                display_help(options);
                return 1;
            }

            // Validate command exists and is executable
            if (!validate_command(command[0])) {
                return 1;
            }

            Logger::info("Starting file trace with options:");
            Logger::info("  Output file: ", output_file);
            Logger::info("  Base directory: ", base_dir);
            Logger::info("  Directory filtering: ", (path_utils::disable_directory_filtering ? "disabled" : "enabled"));
            Logger::info("  Command: ", command[0]);
            std::vector<FileOperation> operations;

            Logger::info("Output will be saved to: ", output_file);
            Logger::info("Monitoring file operations...");
            pid_t child = fork();

            if (child == 0) {
            // Child process
            ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
            // Stop to let parent set options
            raise(SIGSTOP);
            
            // Convert command vector to char* array for execvp
            std::vector<char*> args;
            args.reserve(command.size() + 1);
            for (auto& arg : command) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);
            
            execvp(args[0], args.data());
            Logger::error("Failed to execute ", command[0], ": ", strerror(errno));
            exit(1);
            } else if (child > 0) {
        // Create initial process entry in thread map
        handle_thread_creation(0, child, true);
        // Parent process
        int status;
        user_regs_struct regs;
        bool in_syscall = false;

        // Wait for child to stop (after SIGSTOP)
        waitpid(child, &status, 0);
        if (WIFSTOPPED(status)) {
            // Set ptrace options for following forks
            if (ptrace(PTRACE_SETOPTIONS, child, 0,
                      PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE |
                      PTRACE_O_TRACEEXIT | PTRACE_O_TRACEEXEC) == -1) {
                Logger::error("Failed to set ptrace options: ", strerror(errno));
            }
            // Resume the child
            ptrace(PTRACE_SYSCALL, child, nullptr, nullptr);
        }

        while (true) {
            pid_t waited_pid = waitpid(-1, &status, __WALL);
            if (waited_pid == -1) {
                if (errno == ECHILD) {
                    // Enhanced ECHILD error handling
                    Logger::warning("No child processes found (ECHILD). Checking thread states...");
                    bool any_active = false;
                    std::vector<pid_t> threads_to_cleanup;
                    
                    // First pass: identify threads that need cleanup
                    for (const auto& thread : thread_map) {
                        if (thread.second.active) {
                            if (kill(thread.first, 0) == -1) {
                                if (errno == ESRCH) {
                                    threads_to_cleanup.push_back(thread.first);
                                } else {
                                    Logger::warning("Error checking thread ", thread.first, 
                                                  ": ", strerror(errno));
                                }
                            } else {
                                any_active = true;
                            }
                        }
                    }
                    
                    // Second pass: cleanup terminated threads
                    for (pid_t tid : threads_to_cleanup) {
                        Logger::debug("Cleaning up terminated thread ", tid);
                        handle_thread_exit(tid, -1);
                        
                        // Attempt to detach if still attached
                        if (ptrace(PTRACE_DETACH, tid, nullptr, nullptr) == -1 && errno != ESRCH) {
                            Logger::warning("Failed to detach from thread ", tid, 
                                          ": ", strerror(errno));
                        }
                    }
                    
                    if (!any_active) {
                        Logger::info("All threads have terminated. Exiting...");
                        break;
                    }
                    
                    // Some threads might still be running but not traced
                    usleep(10000); // Increased sleep to reduce CPU usage
                    continue;
                }
                
                Logger::error("waitpid failed: ", strerror(errno));
                // Try to cleanup remaining threads before breaking
                for (const auto& thread : thread_map) {
                    if (thread.second.active) {
                        handle_thread_exit(thread.first, -1);
                    }
                }
                break;
            }
            
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // Handle thread/process termination
                int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                handle_thread_exit(waited_pid, exit_code);
                
                // Check if this was the main process
                if (waited_pid == child && (WIFEXITED(status) || WIFSIGNALED(status))) {
                    // Main process exited, cleanup remaining threads
                    for (auto& thread : thread_map) {
                        if (thread.second.active && thread.first != child) {
                            if (ptrace(PTRACE_DETACH, thread.first, nullptr, nullptr) != -1) {
                                handle_thread_exit(thread.first, -1);
                            }
                        }
                    }
                }
                continue;
            }

            if (WIFSTOPPED(status)) {
                // Check for fork/clone/vfork events
                if (status >> 8 == (SIGTRAP | (PTRACE_EVENT_FORK << 8)) ||
                    status >> 8 == (SIGTRAP | (PTRACE_EVENT_VFORK << 8)) ||
                    status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8))) {
                    
                    unsigned long new_pid;
                    if (ptrace(PTRACE_GETEVENTMSG, waited_pid, nullptr, &new_pid) != -1) {
                        bool is_process = (status >> 8 == (SIGTRAP | (PTRACE_EVENT_FORK << 8)) ||
                                        status >> 8 == (SIGTRAP | (PTRACE_EVENT_VFORK << 8)));
                        
                        // Enhanced fork/clone event handling
                        handle_thread_creation(waited_pid, new_pid, is_process);
                        
                        // Wait for the new process/thread to stop
                        int new_status;
                        if (waitpid(new_pid, &new_status, __WALL) != -1) {
                            if (WIFSTOPPED(new_status)) {
                                // Set options for the new process/thread
                                if (ptrace(PTRACE_SETOPTIONS, new_pid, 0,
                                        PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE |
                                        PTRACE_O_TRACEEXIT | PTRACE_O_TRACEEXEC) == -1) {
                                    Logger::error("Failed to set ptrace options for new process/thread ", 
                                                new_pid, ": ", strerror(errno));
                                }
                                
                                // Resume the new process/thread
                                if (ptrace(PTRACE_SYSCALL, new_pid, nullptr, nullptr) == -1) {
                                    Logger::error("Failed to resume new process/thread ", 
                                                new_pid, ": ", strerror(errno));
                                }
                            } else {
                                Logger::warning("New process/thread ", new_pid, 
                                              " not stopped as expected");
                            }
                        } else {
                            Logger::error("Failed to wait for new process/thread ", 
                                        new_pid, ": ", strerror(errno));
                        }
                    } else {
                        Logger::error("Failed to get event message for process/thread creation: ", 
                                    strerror(errno));
                    }
                    
                    // Resume the parent process
                    if (ptrace(PTRACE_SYSCALL, waited_pid, nullptr, nullptr) == -1) {
                        Logger::error("Failed to resume parent process ", 
                                    waited_pid, ": ", strerror(errno));
                    }
                    continue;
                }

                // Validate thread state before continuing
                auto thread_it = thread_map.find(waited_pid);
                if (thread_it == thread_map.end()) {
                    // New thread detected, create entry
                    handle_thread_creation(child, waited_pid);
                    thread_it = thread_map.find(waited_pid);
                }
                
                if (!thread_it->second.active) {
                    // Thread marked as inactive, detach from it
                    if (ptrace(PTRACE_DETACH, waited_pid, nullptr, nullptr) == -1) {
                        Logger::error("Failed to detach from terminated thread ", waited_pid, 
                                    ": ", strerror(errno));
                    }
                    continue;
                }
                // Validate thread state before accessing registers
                if (kill(waited_pid, 0) == -1) {
                    if (errno == ESRCH) {
                        // Thread no longer exists
                        handle_thread_exit(waited_pid, -1);
                        continue;
                    }
                }

                // Enhanced register access error recovery
                int retry_count = 0;
                const int max_retries = 5;  // Increased retry limit
                bool registers_obtained = false;
                
                while (retry_count < max_retries) {
                    if (ptrace(PTRACE_GETREGS, waited_pid, nullptr, &regs) != -1) {
                        registers_obtained = true;
                        break;
                    }
                    
                    if (errno == ESRCH) {
                        Logger::debug("Thread ", waited_pid, " terminated during register access");
                        handle_thread_exit(waited_pid, -1);
                        break;
                    } else if (errno == EINVAL) {
                        Logger::warning("Invalid thread state for ", waited_pid, 
                                      ". Attempt ", (retry_count + 1), "/", max_retries);
                        
                        // Check if thread is still alive
                        if (kill(waited_pid, 0) == -1 && errno == ESRCH) {
                            Logger::debug("Thread ", waited_pid, " terminated during recovery");
                            handle_thread_exit(waited_pid, -1);
                            break;
                        }
                        
                        // Exponential backoff for retries
                        usleep(1000 * (1 << retry_count));
                        retry_count++;
                        continue;
                    }
                    
                    Logger::error("Failed to get registers for thread ", waited_pid, 
                                ": ", strerror(errno));
                    break;
                }

                if (!registers_obtained) {
                    Logger::error("Failed to recover thread ", waited_pid, " state after ", 
                                retry_count, " attempts");
                    handle_thread_exit(waited_pid, -1);
                    continue;
                }
            
                if (!in_syscall) {
                    handle_syscall_entry(waited_pid, regs, operations, base_dir);
                }
                in_syscall = !in_syscall;
            
                // Validate thread state before continuing
                if (kill(waited_pid, 0) == -1) {
                    if (errno == ESRCH) {
                        handle_thread_exit(waited_pid, -1);
                        continue;
                    }
                }
                
                // Enhanced ptrace continuation error recovery
                int continue_retry = 0;
                const int max_continue_retries = 3;
                bool continuation_successful = false;
                
                while (continue_retry < max_continue_retries) {
                    if (ptrace(PTRACE_SYSCALL, waited_pid, nullptr, nullptr) != -1) {
                        continuation_successful = true;
                        break;
                    }
                    
                    if (errno == ESRCH) {
                        Logger::debug("Thread ", waited_pid, " terminated during continuation");
                        handle_thread_exit(waited_pid, -1);
                        break;
                    } else if (errno == EINVAL || errno == EIO) {
                        Logger::warning("Failed to continue thread ", waited_pid, 
                                      " (attempt ", (continue_retry + 1), "/", max_continue_retries,
                                      "): ", strerror(errno));
                        
                        // Verify thread state
                        if (kill(waited_pid, 0) == -1) {
                            if (errno == ESRCH) {
                                Logger::debug("Thread ", waited_pid, " terminated during retry");
                                handle_thread_exit(waited_pid, -1);
                                break;
                            }
                        }
                        
                        usleep(1000 * (1 << continue_retry));
                        continue_retry++;
                        continue;
                    }
                    
                    Logger::error("Failed to continue thread ", waited_pid, 
                                ": ", strerror(errno));
                    break;
                }
                
                if (!continuation_successful) {
                    Logger::error("Failed to continue thread ", waited_pid, 
                                " after ", continue_retry, " attempts");
                    // Attempt to cleanup the thread
                    handle_thread_exit(waited_pid, -1);
                    continue;
                }
            }
        }

        generate_html_output(operations, output_file);
        Logger::info("Created visualization at ", output_file);
            } else {
                Logger::error("Fork failed: ", strerror(errno));
                return 1;
            }

            return 0;
        } catch (const std::exception& e) {
            Logger::error("Command line parsing error: ", e.what());
            return 1;
        }
    } catch (const std::exception& e) {
        Logger::error("Unhandled exception: ", e.what());
        return 1;
    } catch (...) {
        Logger::error("Unknown error occurred");
        return 1;
    }
