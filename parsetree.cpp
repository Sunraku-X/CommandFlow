
#include "parsetree.h"
#include <string>
#include <vector>
#include <sstream>

struct TreeNode
{
    std::string value;
    std::vector<TreeNode> children;
};

std::string trim(const std::string& text)
{
    size_t start = text.find_first_not_of(" \t\r\n");

    if (start == std::string::npos)
        return "";

    size_t end = text.find_last_not_of(" \t\r\n");

    return text.substr(start, end - start + 1);
}

bool buildNodes(const std::vector<std::string>& lines,
                size_t& index, TreeNode& parent)
{
    while (index < lines.size())
    {
        std::string line = trim(lines[index]);

        if (line.empty())
        {
            index++;
            continue;
        }

        if (line == "}")
            return true;

        TreeNode node;

        if (line.rfind("REPEAT(", 0) == 0 &&
            !line.empty() && line.back() == '{')
        {
            node.value = line.substr(0, line.size() - 1);
            index++;

            if (!buildNodes(lines, index, node))
                return false;

            if (index >= lines.size() || trim(lines[index]) != "}")
                return false;

            index++;
        }
        else
        {
            if (line == "LEFT" || line == "RIGHT")
            {
                node.value = "TURN";
                node.children.push_back({line, {}});
            }
            else if (line.rfind("FORWARD(", 0) == 0 ||
                     line.rfind("BACKWARD(", 0) == 0)
            {
                node.value = "MOVE";
                node.children.push_back({line, {}});
            }
            else
            {
                return false;
            }

            index++;
        }

        parent.children.push_back(node);
    }

    return true;
}

void printTree(const TreeNode& node,
               std::string prefix,
               bool last,
               std::ostringstream& result,
               bool root)
{
    if (root)
        result << node.value << "\n";
    else
        result << prefix << (last ? "+-- " : "|-- ")
               << node.value << "\n";

    for (size_t i = 0; i < node.children.size(); i++)
    {
        std::string nextPrefix = prefix;

        if (!root)
            nextPrefix += last ? "    " : "|   ";

        printTree(
            node.children[i],
            nextPrefix,
            i == node.children.size() - 1,
            result,
            false
        );
    }
}

std::string generateParseTree(const std::string& commands)
{
    std::istringstream stream(commands);
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(stream, line))
        lines.push_back(line);

    TreeNode root;
    root.value = "PROGRAM";

    size_t index = 0;

    if (!buildNodes(lines, index, root) || index != lines.size())
        return "Could not generate parse tree.";

    std::ostringstream result;

    printTree(root, "", true, result, true);

    return result.str();
}
