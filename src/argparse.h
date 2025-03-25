#ifndef ARGPARSE_H_INCLUDED
#define ARGPARSE_H_INCLUDED

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class OptionType
{
    STRING = 1 << 0,
    SWITCH = 1 << 1,
    INTEGER = 1 << 2,
    DECIMAL = 1 << 3,
};

struct Option
{
    std::string name, short_name;
    OptionType option_type;
    std::string description;
    std::optional<std::string> value;
};

class CommandLineParser
{
  public:
    CommandLineParser();
    ~CommandLineParser();

    void add_option(const std::string& option_name, const std::string& short_option_name, OptionType type, const std::string& description);

    void parse(int argc, char** argv, std::vector<std::string>& non_options);
    void forget();

    bool help_requested() const;

    std::optional<std::string> get_arg_as_string(const std::string& option_name) const;
    bool get_arg_as_switch(const std::string& option_name) const;
    std::optional<int> get_arg_as_integer(const std::string& option_name) const;
    std::optional<float> get_arg_as_decimal(const std::string& option_name) const;

    std::string get_help() const;

  private:
    std::unordered_map<std::string, size_t> option_indices;
    std::unordered_map<std::string, size_t> short_option_indices;
    std::vector<Option> options;
};

#endif