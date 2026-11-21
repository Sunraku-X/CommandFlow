#include "validator.h"
#include <regex>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& value)
{
    const std::string whitespace = " \t\n\r";
    const size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) return "";
    const size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool validateCommands(const std::string& commands)
{
    static const std::regex movePattern(R"(^(FORWARD|BACKWARD)\s*\(\s*[0-9]+\s*\)$)");
    static const std::regex repeatPattern(R"(^REPEAT\s*\(\s*[0-9]+\s*\)\s*\{$)");

    std::stringstream stream(commands);
    std::string line;
    bool foundCommand = false;

    while (std::getline(stream, line))
    {
        line = trim(line);
        if (line.empty()) continue;

        if (line == "LEFT" || line == "RIGHT" || line == "}")
        {
            if (line != "}") foundCommand = true;
            continue;
        }

        if (std::regex_match(line, movePattern) ||
            std::regex_match(line, repeatPattern))
        {
            foundCommand = true;
            continue;
        }

        return false;
    }

    return foundCommand;
}
