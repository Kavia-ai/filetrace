{"is_source_file": true, "format": "C++", "description": "This header file defines the HtmlGenerator class, which provides a static method for generating an HTML report from a directory tree structure, including necessary styling and scripting for interactivity.", "external_files": ["directory_tree.hpp"], "external_methods": ["DirectoryTree::generate_html"], "published": ["HtmlGenerator::generate_html_report", "HtmlGenerator::get_last_error"], "classes": [{"name": "HtmlGenerator", "description": "A class that generates an HTML report visualization from a directory structure."}], "methods": [{"name": "generate_html_report", "description": "Generates an HTML report from the provided DirectoryTree object and writes it to the specified output file."}, {"name": "get_last_error", "description": "Returns the last error message reported by the HtmlGenerator."}], "calls": ["std::ofstream::is_open", "std::ofstream::operator<<", "DirectoryTree::generate_html"], "search-terms": ["HtmlGenerator", "generate_html_report", "directory tree visualization"], "state": 2, "ctags": [], "filename": "/app/data/T0002/43087/Kavia-ai/filetrace/src/html_generator.hpp", "hash": "4cdb08055d797d17232702cc77565421", "format-version": 3, "code-base-name": "b0134v6"}