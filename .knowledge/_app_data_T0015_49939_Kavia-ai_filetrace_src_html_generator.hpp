{"is_source_file": true, "format": "C++ Header File", "description": "This file defines the HtmlGenerator class, which is responsible for generating an HTML report based on a directory tree structure.", "external_files": ["directory_tree.hpp"], "external_methods": ["DirectoryTree::generate_html"], "published": ["HtmlGenerator::generate_html_report", "HtmlGenerator::get_last_error"], "classes": [{"name": "HtmlGenerator", "description": "A class that provides static methods for generating an HTML report from a DirectoryTree."}], "methods": [{"name": "generate_html_report", "description": "Generates an HTML report based on the provided DirectoryTree and saves it to the specified output file."}, {"name": "get_last_error", "description": "Returns the last error message encountered during HTML report generation."}], "calls": ["std::ofstream::is_open", "std::ofstream::close", "std::string::operator+", "DirectoryTree::generate_html"], "search-terms": ["HtmlGenerator", "generate_html_report", "get_last_error"], "state": 2, "ctags": [], "filename": "/app/data/T0015/49939/Kavia-ai/filetrace/src/html_generator.hpp", "hash": "4cdb08055d797d17232702cc77565421", "format-version": 3, "code-base-name": "b0246qj"}