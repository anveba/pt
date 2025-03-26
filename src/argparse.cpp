#include "argparse.h"
#include <cassert>
#include <sstream>
#include <stdexcept>

CommandLineParser::CommandLineParser()
{
    add_option("--help", "", OptionType::SWITCH, "Displays help.");
}

CommandLineParser::~CommandLineParser()
{
}

static bool is_letters_only(const std::string& str)
{
    return str.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos;
}

void CommandLineParser::add_option(
    const std::string& option_name,
    const std::string& short_option_name,
    OptionType type,
    const std::string& description)
{
    if (option_name.size() < 3 || option_name[0] != '-' || option_name[1] != '-')
        throw std::runtime_error("Option must start with two dashes followed by at least one character.");
    if ((short_option_name.size() != 2 || option_name[0] != '-') && !short_option_name.empty())
        throw std::runtime_error("Short name must start with one dashes followed by one character (or be empty).");
    if (!is_letters_only(option_name.substr(2)))
        throw std::runtime_error("Option name must only contain lowercase letters.");
    if (!short_option_name.empty() && !is_letters_only(short_option_name.substr(1)))
        throw std::runtime_error("Option short name must only contain lowercase letters (or be empty).");
    if (option_indices.count(option_name) > 0)
        throw std::runtime_error("Option " + option_name + " is already added.");

    option_indices[option_name] = options.size();
    if (!short_option_name.empty()) {
        if (short_option_indices.count(short_option_name) > 0)
            throw std::runtime_error("Short argument " + option_name + " is already added.");
        short_option_indices[short_option_name] = options.size();
    }

    Option a = { .name = option_name, .short_name = short_option_name, .option_type = type, .description = description };
    options.push_back(a);
}

void CommandLineParser::parse(int argc, char** argv, std::vector<std::string>& non_options)
{
    forget();
    bool options_ended = false;
    for (int i = 1; i < argc; i++) {
        std::string token(argv[i]);

        if (!options_ended && token.size() >= 3 && token[0] == '-' && token[1] == '-') {
            if (option_indices.count(token) == 0)
                throw std::runtime_error("Unknown option " + token + ".");

            Option& a = options[option_indices[token]];
            if (a.option_type == OptionType::SWITCH) {
                a.value = "true";
            } else {
                i++;
                if (i >= argc)
                    throw std::runtime_error("Option " + token + " has no value.");
                a.value = std::string(argv[i]);
            }
        } else if (!options_ended && token.size() >= 2 && token[0] == '-' && token[1] != '-') {
            if (short_option_indices.count(token.substr(0, 2)) == 0)
                throw std::runtime_error("Unknown option " + token.substr(0, 2) + ".");

            Option& a = options[short_option_indices[token.substr(0, 2)]];

            if (a.option_type == OptionType::SWITCH) {
                if (token.size() != 2)
                    throw std::runtime_error("Switch option " + token.substr(0, 2) + " has a value.");
                a.value = "true";
            } else if (token.size() == 2) {
                i++;
                if (i >= argc)
                    throw std::runtime_error("Option " + token + " has no value.");
                a.value = std::string(argv[i]);
            } else {
                a.value = token.substr(2);
            }
        } else if (token.size() == 2 && token[0] == '-' && token[1] == '-') {
            options_ended = true;
        } else {
            non_options.push_back(token);
        }
    }
}

void CommandLineParser::forget()
{
    for (auto& option : options)
        option.value = std::nullopt;
}

bool CommandLineParser::help_requested() const
{
    return get_arg_as_switch("--help");
}

std::optional<std::string> CommandLineParser::get_arg_as_string(const std::string& option_name) const
{
    if (option_indices.count(option_name) == 0)
        throw std::runtime_error("Option " + option_name + " was not added.");
    const Option& option = options[option_indices.at(option_name)];
    if (option.option_type != OptionType::STRING)
        throw std::runtime_error("Option type mismatch. Expected string.");
    return option.value;
}

bool CommandLineParser::get_arg_as_switch(const std::string& option_name) const
{
    if (option_indices.count(option_name) == 0)
        throw std::runtime_error("Option " + option_name + " was not added.");
    const Option& option = options[option_indices.at(option_name)];
    if (option.option_type != OptionType::SWITCH)
        throw std::runtime_error("Option type mismatch. Expected switch.");
    return option.value == "true";
}

std::optional<int> CommandLineParser::get_arg_as_integer(const std::string& option_name) const
{
    if (option_indices.count(option_name) == 0)
        throw std::runtime_error("Option " + option_name + " was not added.");
    const Option& option = options[option_indices.at(option_name)];
    if (option.option_type != OptionType::INTEGER)
        throw std::runtime_error("Option type mismatch. Expected integer.");

    if (option.value.has_value()) {
        try {
            return std::stoi(option.value.value());
        } catch (const std::exception&) {
            throw std::runtime_error("The value of option " + option_name + " was not an integer: " + option.value.value());
        }
    } else {
        return std::nullopt;
    }
}

std::optional<float> CommandLineParser::get_arg_as_decimal(const std::string& option_name) const
{
    if (option_indices.count(option_name) == 0)
        throw std::runtime_error("Option " + option_name + " was not added.");
    const Option& option = options[option_indices.at(option_name)];
    if (option.option_type != OptionType::DECIMAL)
        throw std::runtime_error("Option type mismatch. Expected decimal.");

    if (option.value.has_value()) {
        try {
            return std::stof(option.value.value());
        } catch (const std::exception&) {
            throw std::runtime_error("The value of option " + option_name + " was not a decimal number: " + option.value.value());
        }
    } else {
        return std::nullopt;
    }
}

std::string CommandLineParser::get_help() const
{
    std::stringstream result;
    result << "Options:";
    for (const auto& option : options) {
        result << "\n  ";
        result << option.name;
        if (!option.short_name.empty())
            result << ", " << option.short_name;
        result << "\t" << option.description;
    }
    return result.str();
}
