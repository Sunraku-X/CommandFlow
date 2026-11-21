
#include "pda.h"
#include <stack>
#include <string>

bool validatePDA(const std::string& commands)
{
    std::stack<char> pdaStack;

    for (char c : commands)
    {
        if (c == '{')
        {
            pdaStack.push(c);
        }
        else if (c == '}')
        {
            if (pdaStack.empty())
                return false;

            pdaStack.pop();
        }
    }

    return pdaStack.empty();
}

