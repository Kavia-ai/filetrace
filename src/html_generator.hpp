#ifndef HTML_GENERATOR_HPP
#define HTML_GENERATOR_HPP

#include <string>
#include <fstream>
#include <sstream>
#include "directory_tree.hpp"

class HtmlGenerator {
public:
    static bool generate_html_report(const DirectoryTree& tree, const std::string& output_file) {
        std::ofstream out(output_file);
        if (!out.is_open()) {
            last_error = "Failed to open output file: " + output_file;
            return false;
        }

        // Write HTML header with enhanced styling
        out << "<!DOCTYPE html>\n"
            << "<html>\n"
            << "<head>\n"
            << "<title>File Access Visualization</title>\n"
            << "<style>\n"
            << ":root { --spacing-unit: 0.5rem; --primary-color: #0066cc; --border-color: #ddd; --text-color: #333; --bg-color: #fff; }\n"
            << "@media (prefers-color-scheme: dark) {\n"
            << "  :root { --primary-color: #4d94ff; --border-color: #444; --text-color: #eee; --bg-color: #222; }\n"
            << "}\n"
            << "* { box-sizing: border-box; margin: 0; padding: 0; }\n"
            << "body { font-family: system-ui, -apple-system, sans-serif; background: var(--bg-color); color: var(--text-color); line-height: 1.5; }\n"
            << ".svg-icon { width: 16px; height: 16px; fill: currentColor; vertical-align: middle; }\n"
            << ".container { display: grid; grid-template-columns: minmax(250px, 1fr) 3fr; gap: var(--spacing-unit); padding: var(--spacing-unit); max-width: 1600px; margin: 0 auto; }\n"
            << "@media (max-width: 768px) { .container { grid-template-columns: 1fr; } }\n"
            << "h1 { color: var(--text-color); font-size: 1.5rem; margin-bottom: var(--spacing-unit); grid-column: 1 / -1; }\n"
            << ".search-container { position: sticky; top: 0; background: var(--bg-color); padding: var(--spacing-unit); z-index: 100; grid-column: 1 / -1; }\n"
            << "#search-box { width: 100%; padding: calc(var(--spacing-unit) * 0.75); font-size: 1rem; border: 2px solid var(--border-color); border-radius: 4px; background: var(--bg-color); color: var(--text-color); }\n"
            << "#search-box:focus { outline: none; border-color: var(--primary-color); box-shadow: 0 0 0 2px rgba(0,102,204,0.2); }\n"
            << ".directory-tree { font-family: 'SF Mono', Consolas, monospace; font-size: 0.9rem; }\n"
            << ".tree-node { display: flex; flex-direction: column; margin: calc(var(--spacing-unit) * 0.25) 0; }\n"
            << ".node-content { display: flex; align-items: center; padding: calc(var(--spacing-unit) * 0.5); border-radius: 4px; transition: background-color 0.2s; }\n"
            << ".node-content:hover { background-color: rgba(0,102,204,0.1); }\n"
            << ".file { color: var(--text-color); }\n"
            << ".file .name { font-weight: normal; }\n"
            << ".directory { color: var(--primary-color); cursor: pointer; }\n"
            << ".directory .name { font-weight: 600; }\n"
            << ".sequence { color: var(--primary-color); margin-left: var(--spacing-unit); font-weight: 600; opacity: 0.8; }\n"
            << ".thread-info { color: var(--text-color); margin-left: var(--spacing-unit); opacity: 0.7; }\n"
            << ".debug-info { color: var(--text-color); margin-left: var(--spacing-unit); opacity: 0.7; transition: all 0.3s ease; }\n"
            << ".debug-info.collapsed { max-height: 0; overflow: hidden; opacity: 0; }\n"
            << ".debug-info-header { cursor: pointer; display: flex; align-items: center; }\n"
            << ".debug-info-header:hover { color: var(--primary-color); }\n"
            << ".debug-info-content { max-height: 500px; overflow: auto; transition: max-height 0.3s ease; }\n"
            << ".folder-icon { margin-right: calc(var(--spacing-unit) * 0.5); transition: transform 0.2s; display: inline-flex; align-items: center; }\n"
            << ".file-icon { margin-right: calc(var(--spacing-unit) * 0.5); display: inline-flex; align-items: center; }\n"
            << ".children { margin-left: calc(var(--spacing-unit) * 2); border-left: 1px solid var(--border-color); padding-left: var(--spacing-unit); transition: all 0.3s ease-out; }\n"
            << ".collapsed .children { display: none; }\n"
            << ".collapsed .folder-icon { transform: rotate(-90deg); }\n"
            << ".hidden { display: none; }\n"
            << ".search-match { background-color: rgba(255, 215, 0, 0.3); box-shadow: 0 0 0 2px rgba(255, 215, 0, 0.5); border-radius: 2px; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); }\n"
            << ".search-match-enter { animation: highlight-fade-in 0.3s cubic-bezier(0.4, 0, 0.2, 1); }\n"
            << "@keyframes highlight-fade-in { from { background-color: transparent; } to { background-color: rgba(255, 215, 0, 0.3); } }\n"
            << ".children { margin-left: calc(var(--spacing-unit) * 2); border-left: 1px solid var(--border-color); padding-left: var(--spacing-unit); transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); transform-origin: top; }\n"
            << ".collapsed .children { transform: scaleY(0); opacity: 0; }\n"
            << ".tree-node { transform-origin: top; transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1), opacity 0.3s cubic-bezier(0.4, 0, 0.2, 1); }\n"
            << ".tree-node.hidden { transform: scaleY(0); opacity: 0; }\n"
            << "</style>\n"
            << "<script>\n"
            << "const folderSvg = `<svg class='svg-icon' viewBox='0 0 20 20'><path d='M2 4c0-1.1.9-2 2-2h4l2 2h6c1.1 0 2 .9 2 2v10c0 1.1-.9 2-2 2H4c-1.1 0-2-.9-2-2V4z'/></svg>`;\n"
            << "const fileSvg = `<svg class='svg-icon' viewBox='0 0 20 20'><path d='M13 2H6C4.9 2 4 2.9 4 4v12c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7l-3-5zM13 8V3.5L17.5 8H13z'/></svg>`;\n"
            << "</script>\n"
            << "</head>\n"
            << "<body>\n"
            << "<div class='container'>\n"
            << "<h1>File Access Visualization</h1>\n"
            << "<div class='search-container'>\n"
            << "<input type='text' id='search-box' placeholder='Search files and processes...' onkeyup='filterFiles()'>\n"
            << "</div>\n"
            << "<div class='directory-tree'>\n"
            << "<script>\n"
            << "function toggleDirectory(element) {\n"
            << "    const node = element.closest('.tree-node');\n"
            << "    node.classList.toggle('collapsed');\n"
            << "}\n\n"
            << "function filterFiles() {\n"
            << "    const searchText = document.getElementById('search-box').value.toLowerCase();\n"
            << "    const nodes = document.querySelectorAll('.tree-node');\n"
            << "    \n"
            << "    // Remove existing highlights\n"
            << "    document.querySelectorAll('.search-match').forEach(el => {\n"
            << "        el.classList.remove('search-match', 'search-match-enter');\n"
            << "    });\n"
            << "    \n"
            << "    // First pass: Mark all nodes as hidden\n"
            << "    nodes.forEach(node => {\n"
            << "        node.classList.add('hidden');\n"
            << "    });\n\n"
            << "    // Second pass: Show matching nodes and their parent directories\n"
            << "    nodes.forEach(node => {\n"
            << "        const nameElement = node.querySelector('.name');\n"
            << "        const name = nameElement.textContent.toLowerCase();\n"
            << "        const isDirectory = node.querySelector('.directory') !== null;\n"
            << "        \n"
            << "        if (name.includes(searchText) && searchText !== '') {\n"
            << "            // Show and highlight this node\n"
            << "            node.classList.remove('hidden');\n"
            << "            nameElement.classList.add('search-match', 'search-match-enter');\n"
            << "            \n"
            << "            // Show all parent directories\n"
            << "            let parent = node.parentElement;\n"
            << "            while (parent) {\n"
            << "                if (parent.classList.contains('children')) {\n"
            << "                    parent.classList.remove('hidden');\n"
            << "                    const parentNode = parent.closest('.tree-node');\n"
            << "                    if (parentNode) {\n"
            << "                        parentNode.classList.remove('hidden');\n"
            << "                        parentNode.classList.remove('collapsed'); // Expand matching directories\n"
            << "                    }\n"
            << "                }\n"
            << "                parent = parent.parentElement;\n"
            << "            }\n\n"
            << "            // If this is a directory, show all its children\n"
            << "            if (isDirectory) {\n"
            << "                const children = node.querySelectorAll('.tree-node');\n"
            << "                children.forEach(child => {\n"
            << "                    child.classList.remove('hidden');\n"
            << "                });\n"
            << "                node.classList.remove('collapsed'); // Expand matching directories\n"
            << "            }\n"
            << "        }\n"
            << "    });\n\n"
            << "    // If search is empty, show everything\n"
            << "    if (searchText === '') {\n"
            << "        nodes.forEach(node => {\n"
            << "            node.classList.remove('hidden');\n"
            << "        });\n"
            << "    }\n"
            << "}\n\n"
            << "// Add click handlers to all directory nodes\n"
            << "document.addEventListener('DOMContentLoaded', function() {\n"
            << "    document.querySelectorAll('.directory').forEach(dir => {\n"
            << "        dir.addEventListener('click', function(e) {\n"
            << "            if (e.target.closest('.node-content')) {\n"
            << "                toggleDirectory(e.target);\n"
            << "            }\n"
            << "        });\n"
            << "    });\n"
            << "});\n"
            << "</script>\n";

        // Generate tree content
        tree.generate_html(out);

        // Close directory tree div
        out << "</div>\n";

        // Write HTML footer with debug information
        out << "<div class='debug-info' style='grid-column: 1 / -1; margin-top: var(--spacing-unit); padding: var(--spacing-unit); background: var(--bg-color); border: 1px solid var(--border-color); border-radius: 4px;'>\n"
            << "<div class='debug-info-header' onclick='this.parentElement.classList.toggle(\"collapsed\")'>\n"
            << "<h2 style='font-size: 1.2rem; margin-bottom: var(--spacing-unit);'>Debug Information</h2>\n"
            << "</div>\n"
            << "<div class='debug-info-content'>\n"
            << "<pre id='debug-info' style='font-family: \"SF Mono\", Consolas, monospace; font-size: 0.9rem; overflow-x: auto;'>\n"
            << "Output file: " << output_file << "\n"
            << "</pre>\n"
            << "</div>\n"
            << "</div>\n"
            << "</div>\n" // Close container
            << "</body>\n"
            << "</html>\n";

        out.close();
        return true;
    }

    static std::string get_last_error() {
        return last_error;
    }

private:
    static std::string last_error;
};

std::string HtmlGenerator::last_error;

#endif // HTML_GENERATOR_HPP
