#ifndef DIRECTORY_TREE_HPP
#define DIRECTORY_TREE_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <filesystem>
#include <sstream>
#include "path_utils.hpp"

// SVG icons for folder and file
const std::string folderSvg = "<svg class='svg-icon' viewBox='0 0 20 20'><path d='M2 4c0-1.1.9-2 2-2h4l2 2h6c1.1 0 2 .9 2 2v10c0 1.1-.9 2-2 2H4c-1.1 0-2-.9-2-2V4z'/></svg>";
const std::string fileSvg = "<svg class='svg-icon' viewBox='0 0 20 20'><path d='M13 2H6C4.9 2 4 2.9 4 4v12c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7l-3-5zM13 8V3.5L17.5 8H13z'/></svg>";

class DirectoryNode {
public:
    std::string name;
    std::string full_path;
    bool is_file;
    int sequence_number;
    pid_t thread_id;
    std::string thread_name;
    std::map<std::string, std::shared_ptr<DirectoryNode>> children;

    DirectoryNode(const std::string& n, const std::string& path, bool file = false)
        : name(n), full_path(path), is_file(file), sequence_number(-1) {}
};

class DirectoryTree {
public:
    DirectoryTree() : root(std::make_shared<DirectoryNode>("/", "/", false)) {}

    void insert_file(const std::string& path, int sequence, pid_t thread_id, const std::string& thread_name) {
        // Normalize the path first
        std::string normalized_path = path_utils::normalize_path(path);
        std::filesystem::path fs_path(normalized_path);
        std::shared_ptr<DirectoryNode> current = root;
        
        // Enhanced debug output
        std::cerr << "\nInserting file into directory tree:" << std::endl;
        std::cerr << "  Original path: " << path << std::endl;
        std::cerr << "  Normalized path: " << normalized_path << std::endl;
        std::cerr << "  Sequence number: " << sequence << std::endl;
        std::cerr << "  Thread ID: " << thread_id << std::endl;
        std::cerr << "  Thread name: " << thread_name << std::endl;
        std::cerr << "  Current root path: " << root->full_path << std::endl;
        
        // Create path components
        std::vector<std::string> components;
        for (const auto& component : fs_path) {
            std::string comp_str = component.string();
            if (comp_str == "/" || comp_str.empty()) continue;
            components.push_back(comp_str);
        }
        
        std::cerr << "Path components: ";
        for (const auto& comp : components) {
            std::cerr << comp << " / ";
        }
        std::cerr << std::endl;
        
        // Process each component
        std::string current_path = "/";
        for (size_t i = 0; i < components.size(); ++i) {
            const auto& comp_str = components[i];
            bool is_last = (i == components.size() - 1);
            
            current_path = (std::filesystem::path(current_path) / comp_str).string();
            std::cerr << "Processing component: " << comp_str 
                     << " (is_last: " << (is_last ? "true" : "false") << ")" << std::endl;
            
            if (current->children.find(comp_str) == current->children.end()) {
                current->children[comp_str] = std::make_shared<DirectoryNode>(
                    comp_str, current_path, is_last);
                std::cerr << "Created new node: " << comp_str 
                         << " with path: " << current_path << std::endl;
            }
            
            current = current->children[comp_str];
            
            // Update file status and metadata for the last component
            if (is_last) {
                current->is_file = true;
                current->sequence_number = sequence;
                current->thread_id = thread_id;
                current->thread_name = thread_name;
                std::cerr << "Updated file metadata for: " << comp_str 
                         << " [" << sequence << "]" << std::endl;
            }
        }
    }

    void generate_html(std::ostream& out) const {
        out << "<div class='directory-tree'>\n";
        generate_html_node(root, out, 0);
        out << "</div>\n";
    }

private:
    std::shared_ptr<DirectoryNode> root;

    void generate_html_node(const std::shared_ptr<DirectoryNode>& node, std::ostream& out, int depth) const {
        std::string indent(depth * 2, ' ');
        out << indent << "<div class='tree-node" << (node->is_file ? " file" : " directory") << "'>\n";
        
        // Output node content
        out << indent << "  <div class='node-content'>\n";
        if (!node->is_file) {
            out << indent << "    <span class='folder-icon' onclick='toggleDirectory(this)'>" << folderSvg << "</span>\n";
        } else {
            out << indent << "    <span class='file-icon'>" << fileSvg << "</span>\n";
        }
        out << indent << "    <span class='name'>" << node->name << "</span>\n";
        if (node->is_file) {
            if (node->sequence_number > 0) {
                out << indent << "    <span class='sequence'>[" << node->sequence_number << "]</span>\n";
            }
            if (!node->thread_name.empty()) {
                out << indent << "    <span class='thread-info'>(Thread: " << node->thread_id << " - " << node->thread_name << ")</span>\n";
            }
        }
        out << indent << "  </div>\n";

        // Output children
        if (!node->children.empty()) {
            out << indent << "  <div class='children'>\n";
            std::vector<std::shared_ptr<DirectoryNode>> sorted_children;
            for (const auto& child : node->children) {
                sorted_children.push_back(child.second);
            }
            
            // Sort children: directories first, then files, both alphabetically
            std::sort(sorted_children.begin(), sorted_children.end(),
                [](const auto& a, const auto& b) {
                    if (a->is_file != b->is_file) return !a->is_file;
                    return a->name < b->name;
                });

            for (const auto& child : sorted_children) {
                generate_html_node(child, out, depth + 2);
            }
            out << indent << "  </div>\n";
        }
        
        out << indent << "</div>\n";
    }
};

#endif // DIRECTORY_TREE_HPP
