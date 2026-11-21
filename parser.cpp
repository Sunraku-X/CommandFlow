#include "parser.h"
#include <string>
#include <cctype>

class Parser
{
    std::string text;
    size_t pos = 0;

    void skipSpaces()
    {
        while (pos < text.size() && std::isspace((unsigned char)text[pos]))
            pos++;
    }

    bool match(const std::string& word)
    {
        skipSpaces();

        if (text.compare(pos, word.size(), word) == 0)
        {
            pos += word.size();
            return true;
        }

        return false;
    }

    bool number()
    {
        skipSpaces();
        size_t start = pos;

        while (pos < text.size() &&
               std::isdigit((unsigned char)text[pos]))
            pos++;

        return pos > start;
    }

    bool move()
    {
        if (match("FORWARD") || match("BACKWARD"))
        {
            return match("(") && number() && match(")");
        }

        return false;
    }

    bool turn()
    {
        return match("LEFT") || match("RIGHT");
    }

    bool repeat()
    {
        if (!match("REPEAT"))
            return false;

        if (!match("(") || !number() || !match(")") || !match("{"))
            return false;

        if (!program(true))
            return false;

        return match("}");
    }

    bool statement()
    {
        size_t saved = pos;

        if (move() || turn() || repeat())
            return true;

        pos = saved;
        return false;
    }

    bool program(bool nested = false)
    {
        skipSpaces();

        while (pos < text.size())
        {
            skipSpaces();

            if (nested && pos < text.size() && text[pos] == '}')
                return true;

            size_t saved = pos;

            if (!statement())
                return false;

            skipSpaces();

            if (pos == saved)
                return false;
        }

        return !nested;
    }

public:
    Parser(const std::string& commands) : text(commands) {}

    bool parse()
    {
        if (!program())
            return false;

        skipSpaces();
        return pos == text.size();
    }
};

bool parseProgram(const std::string& commands)
{
    Parser parser(commands);
    return parser.parse();
}