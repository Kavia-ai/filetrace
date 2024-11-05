#ifndef PATH_UTILS_HPP
#define PATH_UTILS_HPP

#include <string>
#include <filesystem>
#include <unistd.h>
#include <limits.h>

namespace path_utils {

// Global flag to control directory filtering
inline bool disable_directory_filtering = false;

// Normalize path to absolute path with resolved symlinks
inline std::string normalize_path(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    // First try to resolve using realpath
    char resolved_path[PATH_MAX];
    if (realpath(path.c_str(), resolved_path) != nullptr) {
        std::string result = std::string(resolved_path);
        std::cerr << "Resolved absolute path: " << path << " -> " << result << std::endl;
        return result;
    }
    
    // If realpath fails and path is relative, try to resolve relative to current directory
    if (path[0] != '/') {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::string base_dir = std::string(cwd);
            std::filesystem::path full_path = std::filesystem::path(base_dir) / path;
            
            // Try realpath again with the full path
            if (realpath(full_path.c_str(), resolved_path) != nullptr) {
                std::string result = std::string(resolved_path);
                std::cerr << "Resolved relative path: " << path << " -> " << result << std::endl;
                return result;
            }
            
            // If realpath still fails, use lexically normal path
            std::string result = full_path.lexically_normal().string();
            std::cerr << "Normalized relative path: " << path << " -> " << result << std::endl;
            return result;
        }
    }
    
    // If all else fails, return the original path normalized
    std::string result = std::filesystem::path(path).lexically_normal().string();
    std::cerr << "Normalized path: " << path << " -> " << result << std::endl;
    return result;
}

// Check if path is within or equal to base directory
inline bool is_within_directory(const std::string& path, const std::string& base_dir) {
    // If directory filtering is disabled, allow all paths
    if (disable_directory_filtering) {
        std::cerr << "Directory filtering disabled, allowing path: " << path << std::endl;
        return true;
    }

    // Always allow system paths
    if (path.find("/lib") == 0 || 
        path.find("/proc") == 0 || 
        path.find("/etc/ld.so.cache") == 0) {
        std::cerr << "Allowing system path: " << path << std::endl;
        return true;
    }

    // Normalize both paths
    std::string norm_path = normalize_path(path);
    std::string norm_base = normalize_path(base_dir);
    
    // Handle empty paths
    if (norm_path.empty() || norm_base.empty()) {
        std::cerr << "Empty path detected - path: " << path << ", base: " << base_dir << std::endl;
        return false;
    }
    
    // Handle exact matches
    if (norm_path == norm_base) {
        std::cerr << "Exact path match: " << norm_path << std::endl;
        return true;
    }

    // Ensure base directory ends with '/' for proper prefix matching
    if (norm_base.back() != '/') {
        norm_base += '/';
    }
    
    // Debug output
    std::cerr << "Path comparison:" << std::endl;
    std::cerr << "  Original path: " << path << std::endl;
    std::cerr << "  Original base: " << base_dir << std::endl;
    std::cerr << "  Normalized path: " << norm_path << std::endl;
    std::cerr << "  Normalized base: " << norm_base << std::endl;
    
    // Check if path is within base directory
    bool is_within = norm_path.find(norm_base) == 0;
    
    // Additional checks for relative paths
    if (!is_within && path[0] != '/') {
        // Try resolving relative to current directory
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::string full_path = normalize_path(std::string(cwd) + "/" + path);
            is_within = full_path.find(norm_base) == 0;
            std::cerr << "  Relative path check:" << std::endl;
            std::cerr << "    CWD: " << cwd << std::endl;
            std::cerr << "    Full path: " << full_path << std::endl;
            std::cerr << "    Result: " << (is_within ? "true" : "false") << std::endl;
        }
    }
    
    std::cerr << "  Final result: " << (is_within ? "true" : "false") << std::endl;
    return is_within;
}

// Get current working directory
inline std::string get_current_directory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        return std::string(cwd);
    }
    return "";
}

} // namespace path_utils

#endif // PATH_UTILS_HPP
