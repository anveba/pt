#pragma once

#include <string>

// Reads the entire file as a string and returns it.
std::string str_from_file(const std::string& path);

// Removes the given characters from the string.
void filter_string(std::string& raw, const std::string& chars_to_remove);

// Gets the directory of a given file. Includes the final separator (/ or \)
// unless there is no directory given.
std::string directory_of(const std::string& file);