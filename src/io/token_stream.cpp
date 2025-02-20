#include <cstdio>
#include <sstream>

#include "token_stream.h"

std::vector<std::string> split_string(const std::string& raw, char delimiter)
{
    std::vector<std::string> res;
    std::stringstream stream(raw);

    std::string token;
    while (getline(stream, token, delimiter)) {
        if (!token.empty())
            res.push_back(token);
    }

    return res;
}

const std::string& TokenStream::next()
{
    if (done())
        throw std::runtime_error("Token stream has no next token");
    return tokens[index++];
}

const std::string& TokenStream::current() const
{
    return tokens[index];
}

const void TokenStream::rollback(size_t amount)
{
    if (index < amount)
        throw std::runtime_error("Cannot roll further back than the start");
    index -= amount;
}

bool TokenStream::done() const
{
    return index >= tokens.size();
}

TokenStream::TokenStream(const std::string& raw, char delimiter)
{
    index = 0;
    tokens = split_string(raw, delimiter);
}
