#pragma once

#include <string>
#include <vector>

// Deals with tokenising a string and outputing the tokens
// one-by-one
class TokenStream
{
  public:
    // Returns current and increments
    const std::string& next();
    const std::string& current() const;
    const void rollback(size_t amount);
    bool done() const;
    size_t size() const { return tokens.size(); }
    size_t current_index() const { return index; }
    size_t remaining() const { return tokens.size() - index; }

    TokenStream(const std::string& raw, char delimiter);

  private:
    std::vector<std::string> tokens;
    size_t index;
};