#include "util.h"

#include <fstream>

std::vector<char> read_bytes(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Failed to open path: " + path);

    size_t sz = (size_t)file.tellg();
    std::vector<char> buffer(sz);

    file.seekg(0);
    file.read(buffer.data(), sz);
    file.close();

    return buffer;
}