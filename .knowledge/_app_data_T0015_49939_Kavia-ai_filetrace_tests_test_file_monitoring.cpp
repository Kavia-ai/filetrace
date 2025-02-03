{"is_source_file": true, "format": "C++", "description": "This file contains unit tests for monitoring file operations such as accessing existing files, validating non-existent files, handling symbolic links, and performing concurrent file operations using the Google Test framework.", "external_files": ["../src/path_utils.hpp"], "external_methods": [], "published": [], "classes": [{"name": "FileMonitoringTest", "description": "Unit test class for monitoring file operations, inheriting from Google Test's testing framework."}], "methods": [{"name": "SetUp", "description": "Sets up the test environment by creating test directories and files."}, {"name": "TearDown", "description": "Cleans up the test environment by removing test directories and files."}, {"name": "ExistingFileValidation", "description": "Tests that the existing file can be accessed and is a regular file."}, {"name": "NonExistentFileValidation", "description": "Tests that non-existent files cannot be accessed and the correct error is returned."}, {"name": "SymbolicLinkValidation", "description": "Tests that symbolic links can be accessed and correctly point to their target files."}, {"name": "ConcurrentFileOperations", "description": "Tests the ability to perform concurrent file operations and checks for successful execution."}], "calls": ["stat", "lstat", "std::filesystem::create_directory", "std::ofstream", "std::filesystem::create_symlink", "std::filesystem::remove_all"], "search-terms": ["FileMonitoringTest", "ExistingFileValidation", "NonExistentFileValidation", "SymbolicLinkValidation", "ConcurrentFileOperations"], "state": 2, "ctags": [], "filename": "/app/data/T0015/49939/Kavia-ai/filetrace/tests/test_file_monitoring.cpp", "hash": "33b9b7b5253f49c8ff4e2b532a183828", "format-version": 3, "code-base-name": "b0246qj"}