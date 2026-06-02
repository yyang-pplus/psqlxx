#include <psqlxx/command.hpp>

#include <cassert>

#include <iomanip>
#include <iostream>
#include <regex>

#include <psqlxx/exception.hpp>
#include <psqlxx/string_utils.hpp>


using namespace std::string_literals;
using namespace psqlxx;


namespace {

[[nodiscard]] inline auto validName(const Command::NameType &name) {
    if (name.empty()) {
        return true;
    }

    static const std::regex name_pattern(R"(@?[a-zA-Z]+)");
    return std::regex_match(name.data(), name_pattern);
}

[[nodiscard]] inline auto validArgument(const Command::ArgumentType &argument) {
    if (argument.empty()) {
        return false;
    }

    if (argument == VARIADIC_ARGUMENT) {
        return true;
    }

    static const std::regex argument_pattern(R"(\[?[A-Z]+\]?)");
    return std::regex_match(argument.data(), argument_pattern);
}

} // namespace


namespace psqlxx {

namespace internal {

bool validCommand(const Command::NameArrayType &names,
                  const Command::ArgumentArrayType &arguments,
                  const Command::ActionType action) {
    if (not action) {
        return false;
    }

    for (const auto &a_name : names) {
        if (not validName(a_name)) {
            return false;
        }
    }

    for (const auto &an_argument : arguments) {
        if (not validArgument(an_argument)) {
            return false;
        }
    }

    return true;
}

} //namespace internal


Command::Command(NameArrayType n,
                 ArgumentArrayType arguments,
                 ActionType action,
                 DescriptionType description) :
    names(std::move(n)),
    m_arguments(std::move(arguments)), m_action(std::move(action)),
    m_description(std::move(description)) {
    for (const auto an_argument : m_arguments) {
        if (an_argument == VARIADIC_ARGUMENT) {
            m_variadic_argument = true;
            break;
        }
    }
}

const Command::DescriptionType Command::Description() const {
    return m_description;
}

CommandResult Command::operator()(const char **words, const int word_count) const {
    assert(m_action);
    assert(words);
    assert(word_count > 0);

    if (not m_variadic_argument) {
        const ArgumentArrayType::size_type number_arguments = word_count - 1;
        if (number_arguments > m_arguments.size()) {
            std::cerr << "Command (" << words[0] << ") failed: Too many arguments. Expected "
                      << m_arguments.size() << ", but " << number_arguments << " were given."
                      << std::endl;
            return CommandResult::failure;
        }
    }

    return m_action(words, word_count);
}


CommandGroup::CommandGroup(Command::NameType name, Command::DescriptionType description) :
    m_name(std::move(name)), m_description(std::move(description)) {
    assert(validName(m_name));
}

void CommandGroup::AddOneOption(Command::NameArrayType names,
                                Command::ArgumentArrayType arguments,
                                Command::ActionType action,
                                Command::DescriptionType description) {
    assert(internal::validCommand(names, arguments, action));

    if (names.empty()) {
        names.push_back("");
    }

    const auto command_ptr = std::make_shared<Command>(
        std::move(names), std::move(arguments), std::move(action), std::move(description));
    for (const auto &command_name : command_ptr->names) {
        if (command_name.empty()) {
            if (m_anonymous_command)
                throw CommandException {"Only one anonymous command is allowed within one group"};
            m_anonymous_command = command_ptr;
        } else {
            const auto insertion_happened =
                m_name_command_map.emplace(command_name, command_ptr).second;
            if (not insertion_happened) {
                throw CommandException {"Duplicate command name '" + std::string {command_name} +
                                        "' within one group"};
            }
        }
    }

    m_commands.push_back(command_ptr);
}

void CommandGroup::Help() const {
    std::cout << Name() << ":\n";
    for (const auto &[name, command] : m_name_command_map) {
        std::cout << "  " << name << '\t';
        if (name.size() < 6) {
            std::cout << '\t';
        }
        std::cout << command->Description() << '\n';
    }
    std::cout << std::endl;
}

void CommandGroup::Describe() const {
    std::cout << "  " << m_name << ":\t" << m_description << std::endl;
}

CommandResult CommandGroup::operator()(const char **words, const int word_count) const {
    assert(words);
    assert(word_count > 0);

    const auto iter = m_name_command_map.find(words[0]);
    if (iter != m_name_command_map.cend()) {
        return (*(iter->second))(words, word_count);
    }

    if (m_anonymous_command) {
        return (*m_anonymous_command)(words, word_count);
    }

    return CommandResult::unknown;
}

std::string_view CommandGroup::PrefixSearch(const std::string_view prefix) const {
    for (const auto &[name, _] : m_name_command_map) {
        if (StartsWith(name, prefix)) {
            return name;
        }
    }

    return {};
}


CommandResult HelpGroups(const std::vector<CommandGroup> &groups, const std::string_view name) {
    if (name.empty()) {
        const auto USAGE =
            "You are using psqlxx, yet another command-line interface to PostgreSQL.";
        std::cout << USAGE << '\n' << "Groups:\n";
        for (const auto &one_group : groups) {
            one_group.Describe();
        }
    } else {

        bool match_found = false;
        for (const auto &one_group : groups) {
            if (one_group.Name() == name) {
                one_group.Help();
                match_found = true;
                break;
            }
        }

        if (not match_found) {
            std::cerr << "Unknown command group " << std::quoted(name) << std::endl;
            return CommandResult::failure;
        }
    }

    return CommandResult::success;
}

} //namespace psqlxx
