#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <iostream>
#include <type_traits>
#include <vector>
#include <vulkan.h>

#define VK_ASSERT(e)                                                                        \
    {                                                                                       \
        static_assert(std::is_same<decltype(e), VkResult>::value, "type must be VkResult"); \
        if (e) {                                                                            \
            std::cerr << "Vulkan error: " << e << std::endl;                                \
            exit(1);                                                                        \
        }                                                                                   \
    }

#endif

#define SIZE_OF_ARRAY(a) ((size_t)(sizeof(a) / sizeof(*(a))))

std::vector<char> read_bytes(const std::string& path);