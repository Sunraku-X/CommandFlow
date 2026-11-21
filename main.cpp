#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <regex>
#include <memory>
#include <mutex>
#include <thread>
#include "validator.h"
#include "parser.h"
#include "pda.h"
#include "parsetree.h"
#include "gemini.h"

std::string input = "";
std::string output = "";
std::string validationStatus = "Waiting for validation...";
std::string parseTree = "Validate commands to generate a parse tree.";
bool manualMode = false;
bool inputFocused = false;
int currentPage = 0;
bool commandsValidated = false;

// Gemini translation runs in the background so the SFML event loop stays responsive.
struct TranslationResult
{
    std::mutex mutex;
    bool done = false;
    bool success = false;
    std::string commands;
    std::string error;
};
std::shared_ptr<TranslationResult> activeTranslation;
bool translationPending = false;

struct RobotAction
{
    enum Type { Move, TurnLeft, TurnRight } type;
    int amount = 1;
};

std::vector<RobotAction> robotActions;
int robotX = 5;
int robotY = 4;
int robotDirection = 0;
size_t robotActionIndex = 0;
bool robotRunning = false;
bool robotFinished = false;
sf::Clock robotClock;

std::string trimLine(const std::string& value)
{
    const size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return "";
    const size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

bool expandProgram(const std::vector<std::string>& lines, size_t& index,
                   std::vector<RobotAction>& actions, bool insideRepeat = false)
{
    static const std::regex movePattern(R"(^(FORWARD|BACKWARD)\s*\(\s*([0-9]+)\s*\)$)");
    static const std::regex repeatPattern(R"(^REPEAT\s*\(\s*([0-9]+)\s*\)\s*\{$)");
    std::smatch match;

    while (index < lines.size())
    {
        std::string line = trimLine(lines[index++]);
        if (line.empty()) continue;
        if (line == "}") return insideRepeat;

        if (line == "LEFT")
            actions.push_back({RobotAction::TurnLeft, 1});
        else if (line == "RIGHT")
            actions.push_back({RobotAction::TurnRight, 1});
        else if (std::regex_match(line, match, movePattern))
        {
            int amount = std::stoi(match[2].str());
            if (amount > 100) return false;
            for (int i = 0; i < amount; ++i)
                actions.push_back({RobotAction::Move, match[1].str() == "BACKWARD" ? -1 : 1});
        }
        else if (std::regex_match(line, match, repeatPattern))
        {
            int count = std::stoi(match[1].str());
            if (count < 1 || count > 20) return false;
            std::vector<RobotAction> nested;
            if (!expandProgram(lines, index, nested, true)) return false;
            if (nested.empty() || actions.size() + nested.size() * count > 1000) return false;
            for (int i = 0; i < count; ++i)
                actions.insert(actions.end(), nested.begin(), nested.end());
        }
        else return false;

        if (actions.size() > 1000) return false;
    }
    return !insideRepeat;
}

bool prepareRobotActions(const std::string& commands)
{
    std::stringstream stream(commands);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) lines.push_back(line);
    size_t index = 0;
    std::vector<RobotAction> prepared;
    if (!expandProgram(lines, index, prepared) || prepared.empty()) return false;
    robotActions = prepared;
    return true;
}

void resetRobot()
{
    robotX = 5;
    robotY = 4;
    robotDirection = 0;
    robotActionIndex = 0;
    robotRunning = false;
    robotFinished = false;
}

std::string translateCommand(std::string sentence)
{
    std::transform(sentence.begin(), sentence.end(), sentence.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::stringstream ss(sentence);
    std::vector<std::string> words;
    std::string word;

    while (ss >> word)
    {
        while (!word.empty() && !std::isalnum(static_cast<unsigned char>(word.back())))
            word.pop_back();
        while (!word.empty() && !std::isalnum(static_cast<unsigned char>(word.front())))
            word.erase(word.begin());
        if (!word.empty()) words.push_back(word);
    }

    std::string result;
    for (size_t i = 0; i < words.size(); ++i)
    {
        if (words[i] == "forward" || words[i] == "backward")
        {
            std::string command = words[i] == "forward" ? "FORWARD" : "BACKWARD";
            std::string number = "1";
            for (size_t j = i + 1; j < words.size(); ++j)
            {
                if (!words[j].empty() &&
                    std::all_of(words[j].begin(), words[j].end(),
                        [](unsigned char c) { return std::isdigit(c); }))
                {
                    number = words[j];
                    i = j;
                    break;
                }
                if (words[j] == "left" || words[j] == "right" ||
                    words[j] == "forward" || words[j] == "backward") break;
            }
            result += command + "(" + number + ")\n";
        }
        else if (words[i] == "left") result += "LEFT\n";
        else if (words[i] == "right") result += "RIGHT\n";
    }

    if (result.empty()) return "No supported commands found. Try forward, backward, left or right.";
    return result;
}

void drawText(sf::RenderWindow& window, sf::Font& font,
              const std::string& content, unsigned int size,
              float x, float y, sf::Color color)
{
    sf::Text text;
    text.setFont(font);
    text.setString(content);
    text.setCharacterSize(size);
    text.setFillColor(color);
    text.setPosition(x, y);
    text.setLineSpacing(1.2f);
    window.draw(text);
}

void drawPanel(sf::RenderWindow& window, float x, float y,
               float width, float height, sf::Color color)
{
    sf::RectangleShape shadow(sf::Vector2f(width, height));
    shadow.setPosition(x + 3, y + 4);
    shadow.setFillColor(sf::Color(0, 0, 0, 130));
    window.draw(shadow);

    sf::RectangleShape panel(sf::Vector2f(width, height));
    panel.setPosition(x, y);
    panel.setFillColor(color);
    panel.setOutlineThickness(1);
    panel.setOutlineColor(sf::Color(0, 190, 230, 145));
    window.draw(panel);

    // Neon edge and angular corner details.
    sf::RectangleShape edge(sf::Vector2f(3, height - 2));
    edge.setPosition(x + 1, y + 1);
    edge.setFillColor(sf::Color(0, 220, 255));
    window.draw(edge);

    sf::RectangleShape topEdge(sf::Vector2f(72, 2));
    topEdge.setPosition(x + 12, y + 1);
    topEdge.setFillColor(sf::Color(255, 43, 214));
    window.draw(topEdge);

    sf::RectangleShape bottomEdge(sf::Vector2f(42, 2));
    bottomEdge.setPosition(x + width - 54, y + height - 3);
    bottomEdge.setFillColor(sf::Color(255, 43, 214));
    window.draw(bottomEdge);
}

void drawButton(sf::RenderWindow& window, sf::Font& font,
                const std::string& label, float x, float y, sf::Color color)
{
    sf::RectangleShape glow(sf::Vector2f(154, 46));
    glow.setPosition(x - 2, y - 2);
    glow.setFillColor(sf::Color(color.r, color.g, color.b, 28));
    window.draw(glow);

    sf::RectangleShape button(sf::Vector2f(150, 42));
    button.setPosition(x, y);
    button.setFillColor(sf::Color(12, 27, 49));
    button.setOutlineThickness(1.5f);
    button.setOutlineColor(color);
    window.draw(button);

    sf::RectangleShape stripe(sf::Vector2f(4, 38));
    stripe.setPosition(x + 2, y + 2);
    stripe.setFillColor(color);
    window.draw(stripe);

    sf::Text text;
    text.setFont(font);
    text.setString(label);
    text.setCharacterSize(15);
    text.setStyle(sf::Text::Bold);
    text.setFillColor(sf::Color(230, 250, 255));
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.f,
                   bounds.top + bounds.height / 2.f);
    text.setPosition(x + 77, y + 21);
    window.draw(text);
}

void resetValidation()
{
    validationStatus = "Waiting for validation...";
    parseTree = "Validate commands to generate a parse tree.";
}

std::vector<std::string> dfaSteps;
size_t dfaStepIndex = 0;
std::vector<std::string> pdaSteps;
size_t pdaStepIndex = 0;
std::string pdaStackState = "EMPTY";
std::vector<std::string> cfgSteps;
size_t cfgStepIndex = 0;

void buildCfgTrace(const std::string& commands, bool accepted)
{
    cfgSteps.clear();
    cfgStepIndex = 0;
    cfgSteps.push_back("Start: PROGRAM");
    cfgSteps.push_back("PROGRAM -> STATEMENT*");
    cfgSteps.push_back("STATEMENT -> MOVE | TURN | REPEAT");

    std::stringstream stream(commands);
    std::string line;
    while (std::getline(stream, line))
    {
        line = trimLine(line);
        if (line.empty()) continue;
        if (line.rfind("REPEAT(", 0) == 0)
        {
            cfgSteps.push_back("REPEAT -> REPEAT(NUMBER) { PROGRAM }");
            cfgSteps.push_back("NUMBER -> repetition count");
            cfgSteps.push_back("PROGRAM -> nested STATEMENT* until }");
        }
        else if (line == "LEFT" || line == "RIGHT")
        {
            cfgSteps.push_back("STATEMENT -> TURN");
            cfgSteps.push_back("TURN -> " + line);
        }
        else if (line.rfind("FORWARD(", 0) == 0 || line.rfind("BACKWARD(", 0) == 0)
        {
            cfgSteps.push_back("STATEMENT -> MOVE");
            cfgSteps.push_back("MOVE -> " + line);
            cfgSteps.push_back("NUMBER -> numeric argument");
        }
        else if (line == "}")
            cfgSteps.push_back("Close nested PROGRAM -> }");
        else
            cfgSteps.push_back("Unrecognized statement: " + line);
    }
    cfgSteps.push_back(accepted ? "Parse result: ACCEPTED" : "Parse result: REJECTED");
}

void buildDfaTrace(const std::string& commands)
{
    dfaSteps.clear();
    dfaStepIndex = 0;
    dfaSteps.push_back("Initial state: START");
    std::stringstream stream(commands);
    std::string line;
    static const std::regex moveRe(R"(^(FORWARD|BACKWARD)\s*\(\s*[0-9]+\s*\)$)");
    static const std::regex repeatRe(R"(^REPEAT\s*\(\s*[0-9]+\s*\)\s*\{$)");
    while (std::getline(stream, line))
    {
        line = trimLine(line);
        if (line.empty()) continue;
        dfaSteps.push_back("START -> TOKEN: " + line);
        if (line == "LEFT" || line == "RIGHT")
            dfaSteps.push_back("TOKEN -> ACCEPT (turn command)");
        else if (std::regex_match(line, moveRe))
        {
            dfaSteps.push_back("KEYWORD -> OPEN_PAREN -> NUMBER");
            dfaSteps.push_back("NUMBER -> CLOSE_PAREN -> ACCEPT");
        }
        else if (std::regex_match(line, repeatRe))
        {
            dfaSteps.push_back("REPEAT -> OPEN_PAREN -> NUMBER");
            dfaSteps.push_back("CLOSE_PAREN -> OPEN_BRACE -> ACCEPT");
        }
        else if (line == "}")
            dfaSteps.push_back("CLOSING_BRACE -> ACCEPT");
        else
        {
            dfaSteps.push_back("DEAD STATE: invalid token pattern");
            break;
        }
    }
    if (dfaSteps.back().find("DEAD STATE") == std::string::npos)
        dfaSteps.push_back("Final result: token patterns accepted");
}

void buildPdaTrace(const std::string& commands)
{
    pdaSteps.clear();
    pdaStepIndex = 0;
    std::vector<char> stack;
    pdaSteps.push_back("Initial stack: EMPTY");
    for (char c : commands)
    {
        if (c == '{')
        {
            stack.push_back('{');
            pdaSteps.push_back("Read '{' : PUSH   Stack size = " + std::to_string(stack.size()));
        }
        else if (c == '}')
        {
            if (stack.empty())
            {
                pdaSteps.push_back("Read '}' : REJECT (stack is empty)");
                break;
            }
            stack.pop_back();
            pdaSteps.push_back("Read '}' : POP    Stack size = " + std::to_string(stack.size()));
        }
    }
    if (pdaSteps.back().find("REJECT") == std::string::npos)
        pdaSteps.push_back(stack.empty() ? "ACCEPT: stack is EMPTY" : "REJECT: unmatched opening brace");
    pdaStackState = std::to_string(stack.size());
}

int main()
{
    sf::RenderWindow window(
        sf::VideoMode(1100, 700),
        "CommandFlow - AI Powered Command Language Validator"
    );
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf"))
    {
        std::cout << "Font could not be loaded!" << std::endl;
        return 1;
    }

    const sf::Color background(7, 10, 24);
    const sf::Color panelColor(15, 21, 43);
    const sf::Color white(230, 247, 255);
    const sf::Color grey(126, 153, 180);
    const sf::Color blue(0, 210, 255);       // neon cyan
    const sf::Color green(255, 43, 214);     // neon magenta accent
    const sf::Color red(255, 76, 96);

    const sf::FloatRect inputArea(275, 165, 750, 75);
    const sf::FloatRect translateArea(275, 250, 150, 42);
    const sf::FloatRect modeArea(445, 250, 150, 42);
    const sf::FloatRect validateArea(685, 455, 150, 42);

    const std::vector<std::string> menuItems = {
        "Dashboard", "Command Editor", "DFA Visualizer",
        "CFG Analyzer", "PDA Visualizer", "Parse Tree", "Robot Simulator"
    };

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left)
            {
                sf::Vector2f mouse = window.mapPixelToCoords(
                    sf::Vector2i(event.mouseButton.x, event.mouseButton.y));

                if (mouse.x < 220 && mouse.y >= 125 && mouse.y <= 510)
                {
                    int selected = static_cast<int>((mouse.y - 125) / 55);
                    if (selected >= 0 && selected < static_cast<int>(menuItems.size()))
                    {
                        currentPage = selected;
                        inputFocused = false;
                    }
                }

                if (currentPage == 0 || currentPage == 1)
                {
                    inputFocused = inputArea.contains(mouse);

                    if (modeArea.contains(mouse))
                    {
                        manualMode = !manualMode;
                        input.clear();
                        output.clear();
                        inputFocused = true;
                        resetValidation();
                        commandsValidated = false;
                        dfaSteps.clear(); pdaSteps.clear(); cfgSteps.clear();
                        dfaStepIndex = pdaStepIndex = cfgStepIndex = 0;
                        resetRobot();
                    }
                    else if (translateArea.contains(mouse))
                    {
                        resetValidation();
                        commandsValidated = false;
                        dfaSteps.clear(); pdaSteps.clear(); cfgSteps.clear();
                        dfaStepIndex = pdaStepIndex = cfgStepIndex = 0;
                        resetRobot();
                        inputFocused = false;

                        if (manualMode)
                        {
                            output = input;
                        }
                        else if (input.empty())
                        {
                            output.clear();
                            validationStatus = "Enter an English instruction first.";
                        }
                        else if (!translationPending)
                        {
                            // Copy the input before starting the worker thread.
                            const std::string instruction = input;
                            auto result = std::make_shared<TranslationResult>();
                            activeTranslation = result;
                            translationPending = true;
                            output.clear();
                            validationStatus = "Translating with Gemini...";

                            std::thread([result, instruction]()
                            {
                                std::string translated;
                                std::string error;
                                const bool ok = geminiTranslate(instruction, translated, error);

                                std::lock_guard<std::mutex> lock(result->mutex);
                                result->success = ok;
                                result->commands = std::move(translated);
                                result->error = std::move(error);
                                result->done = true;
                            }).detach();
                        }
                    }
                    else if (validateArea.contains(mouse))
                    {
                        inputFocused = false;
                        if (output.empty())
                        {
                            validationStatus = "REJECTED: Translate or enter commands first.";
                            parseTree = "No parse tree available.";
                            commandsValidated = false;
                        }
                        else
                        {
                            const bool regexDfaOk = validateCommands(output);
                            const bool cfgOk = parseProgram(output);
                            const bool pdaOk = validatePDA(output);
                            buildDfaTrace(output);
                            buildPdaTrace(output);
                            buildCfgTrace(output, regexDfaOk && cfgOk && pdaOk);

                            if (regexDfaOk && cfgOk && pdaOk)
                            {
                                validationStatus = "ACCEPTED: Regex + DFA + CFG + PDA passed!";
                                parseTree = generateParseTree(output);
                                commandsValidated = prepareRobotActions(output);
                                resetRobot();
                                if (!commandsValidated)
                                    validationStatus = "ACCEPTED by validators, but simulator could not expand this program.";
                            }
                            else
                            {
                                validationStatus = "REJECTED: Validation failed!";
                                parseTree = "Cannot generate tree for invalid commands.";
                                commandsValidated = false;
                                resetRobot();
                            }
                        }
                    }
                }
                else if (currentPage == 6)
                {
                    inputFocused = false;
                    const sf::FloatRect runArea(275, 555, 150, 42);
                    const sf::FloatRect resetArea(445, 555, 150, 42);
                    if (runArea.contains(mouse))
                    {
                        if (commandsValidated && !robotActions.empty() && !robotFinished)
                        {
                            robotRunning = true;
                            robotClock.restart();
                        }
                    }
                    else if (resetArea.contains(mouse))
                        resetRobot();
                }
                else if (currentPage == 2)
                {
                    inputFocused = false;
                    if (sf::FloatRect(280, 535, 150, 42).contains(mouse))
                    {
                        if (dfaStepIndex + 1 < dfaSteps.size()) ++dfaStepIndex;
                    }
                    else if (sf::FloatRect(450, 535, 150, 42).contains(mouse))
                        dfaStepIndex = 0;
                }
                else if (currentPage == 3)
                {
                    inputFocused = false;
                    if (sf::FloatRect(280, 535, 150, 42).contains(mouse))
                    {
                        if (cfgStepIndex + 1 < cfgSteps.size()) ++cfgStepIndex;
                    }
                    else if (sf::FloatRect(450, 535, 150, 42).contains(mouse))
                        cfgStepIndex = 0;
                }
                else if (currentPage == 4)
                {
                    inputFocused = false;
                    if (sf::FloatRect(280, 535, 150, 42).contains(mouse))
                    {
                        if (pdaStepIndex + 1 < pdaSteps.size()) ++pdaStepIndex;
                    }
                    else if (sf::FloatRect(450, 535, 150, 42).contains(mouse))
                        pdaStepIndex = 0;
                }
                else inputFocused = false;
            }

            if (event.type == sf::Event::TextEntered && inputFocused &&
                (currentPage == 0 || currentPage == 1))
            {
                if (event.text.unicode == 8)
                {
                    if (!input.empty()) input.pop_back();
                }
                else if (event.text.unicode == 13)
                {
                    if (manualMode && input.size() < 500) input += '\n';
                }
                else if (event.text.unicode >= 32 && event.text.unicode < 127 &&
                         input.size() < 500)
                {
                    input += static_cast<char>(event.text.unicode);
                }
            }

            if (event.type == sf::Event::KeyPressed && inputFocused &&
                (currentPage == 0 || currentPage == 1))
            {
                if (event.key.code == sf::Keyboard::BackSpace && !input.empty())
                    input.pop_back();

                const bool pasteShortcut =
                    (event.key.control && event.key.code == sf::Keyboard::V) ||
                    (event.key.shift && event.key.code == sf::Keyboard::Insert);

                if (pasteShortcut)
                {
                    std::string pasted = sf::Clipboard::getString().toAnsiString();
                    pasted.erase(std::remove(pasted.begin(), pasted.end(), '\r'), pasted.end());
                    if (input.size() + pasted.size() <= 500)
                        input += pasted;
                }

                if (event.key.control && event.key.code == sf::Keyboard::A)
                    inputFocused = true;
            }
        }

        // Check whether Gemini has finished without blocking the interface.
        if (translationPending && activeTranslation)
        {
            std::lock_guard<std::mutex> lock(activeTranslation->mutex);
            if (activeTranslation->done)
            {
                if (activeTranslation->success)
                {
                    output = activeTranslation->commands;
                    validationStatus = "Translation complete. Click Validate.";
                }
                else
                {
                    output.clear();
                    validationStatus = "Gemini error: " + activeTranslation->error;
                }

                translationPending = false;
                activeTranslation.reset();
            }
        }

        if (robotRunning && robotClock.getElapsedTime().asSeconds() >= 0.28f)
        {
            robotClock.restart();
            if (robotActionIndex < robotActions.size())
            {
                const RobotAction& action = robotActions[robotActionIndex++];
                if (action.type == RobotAction::TurnLeft)
                    robotDirection = (robotDirection + 3) % 4;
                else if (action.type == RobotAction::TurnRight)
                    robotDirection = (robotDirection + 1) % 4;
                else
                {
                    int step = action.amount;
                    if (robotDirection == 0) robotY -= step;
                    else if (robotDirection == 1) robotX += step;
                    else if (robotDirection == 2) robotY += step;
                    else robotX -= step;
                }
            }
            if (robotActionIndex >= robotActions.size())
            {
                robotRunning = false;
                robotFinished = true;
            }
        }

        window.clear(background);

        // Subtle cyber-grid background.
        for (int gx = 225; gx < 1100; gx += 40)
        {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(static_cast<float>(gx), 0), sf::Color(0, 160, 210, 22)),
                sf::Vertex(sf::Vector2f(static_cast<float>(gx), 700), sf::Color(0, 160, 210, 22))
            };
            window.draw(line, 2, sf::Lines);
        }
        for (int gy = 0; gy < 700; gy += 40)
        {
            sf::Vertex line[] = {
                sf::Vertex(sf::Vector2f(220, static_cast<float>(gy)), sf::Color(0, 160, 210, 18)),
                sf::Vertex(sf::Vector2f(1100, static_cast<float>(gy)), sf::Color(0, 160, 210, 18))
            };
            window.draw(line, 2, sf::Lines);
        }

        // Sidebar: dark glass with neon identity.
        drawPanel(window, 0, 0, 220, 700, sf::Color(10, 16, 34));
        sf::CircleShape brandGlow(20);
        brandGlow.setPosition(17, 22);
        brandGlow.setFillColor(sf::Color(0, 210, 255, 35));
        window.draw(brandGlow);
        sf::CircleShape brandMark(15);
        brandMark.setPosition(22, 27);
        brandMark.setFillColor(sf::Color(10, 31, 53));
        brandMark.setOutlineThickness(2);
        brandMark.setOutlineColor(sf::Color(0, 220, 255));
        window.draw(brandMark);
        drawText(window, font, "C", 18, 29, 28, blue);
        drawText(window, font, "COMMAND", 18, 61, 25, white);
        drawText(window, font, "FLOW // AI CORE", 10, 62, 47, green);
        drawText(window, font, "SYSTEM MODULES", 10, 22, 112, blue);

        for (int i = 0; i < static_cast<int>(menuItems.size()); ++i)
        {
            const float itemY = 140 + i * 55;
            if (currentPage == i)
            {
                sf::RectangleShape active(sf::Vector2f(190, 43));
                active.setPosition(14, itemY - 7);
                active.setFillColor(sf::Color(0, 190, 230, 24));
                active.setOutlineThickness(1);
                active.setOutlineColor(sf::Color(0, 220, 255, 150));
                window.draw(active);
                sf::RectangleShape marker(sf::Vector2f(3, 27));
                marker.setPosition(14, itemY + 1);
                marker.setFillColor(sf::Color(255, 43, 214));
                window.draw(marker);
                drawText(window, font, ">", 14, 23, itemY, green);
            }
            sf::Color itemColor = currentPage == i ? white : grey;
            drawText(window, font, menuItems[i], 15, 43, itemY, itemColor);
        }

        sf::RectangleShape sidebarLine(sf::Vector2f(176, 1));
        sidebarLine.setPosition(22, 555);
        sidebarLine.setFillColor(sf::Color(0, 190, 230, 100));
        window.draw(sidebarLine);
        drawText(window, font, "CORE STATUS", 10, 22, 578, blue);
        drawText(window, font, "●  FORMAL ENGINE ONLINE", 10, 22, 600, sf::Color(80, 255, 180));
        drawText(window, font, "●  GEMINI LINK READY", 10, 22, 620, sf::Color(255, 80, 220));

        // Main page heading and HUD decoration.
        drawText(window, font, menuItems[currentPage], 27, 255, 24, white);
        drawText(window, font, "AI COMMAND INTERFACE   /   THEORY OF COMPUTATION", 11, 257, 64, grey);
        drawText(window, font, "SYS.ONLINE", 10, 930, 31, sf::Color(80, 255, 180));
        sf::CircleShape statusDot(4);
        statusDot.setPosition(914, 33);
        statusDot.setFillColor(sf::Color(80, 255, 180));
        window.draw(statusDot);
        sf::RectangleShape headerLine(sf::Vector2f(800, 1));
        headerLine.setPosition(255, 94);
        headerLine.setFillColor(sf::Color(0, 190, 230, 130));
        window.draw(headerLine);

        // Tiny angular HUD marks at the upper-right corner.
        for (int i = 0; i < 4; ++i)
        {
            sf::RectangleShape tick(sf::Vector2f(3, 12 + i * 4));
            tick.setPosition(1030 + i * 8, 52 - i * 2);
            tick.setFillColor(i % 2 == 0 ? blue : green);
            window.draw(tick);
        }

        if (currentPage == 0 || currentPage == 1)
        {
            drawPanel(window, 255, 115, 800, 190, panelColor);
            drawText(window, font,
                     manualMode ? "Command Language Input" : "Natural Language Input",
                     20, 275, 130, white);

            sf::RectangleShape inputBox(sf::Vector2f(750, 75));
            inputBox.setPosition(275, 165);
            inputBox.setFillColor(sf::Color(7, 14, 30));
            inputBox.setOutlineThickness(inputFocused ? 2.5f : 1.2f);
            inputBox.setOutlineColor(inputFocused ? green : blue);
            window.draw(inputBox);

            drawText(window, font,
                     input.empty()
                        ? (manualMode ? "Click here and enter/paste commands..."
                                      : "Click here and type your instruction...")
                        : input,
                     15, 285, 172, input.empty() ? grey : white);

            drawButton(window, font, translationPending ? "Translating..." : "Translate", 275, 250, blue);
            drawButton(window, font, manualMode ? "English Mode" : "Manual Mode",
                       445, 250, green);

            drawPanel(window, 255, 325, 390, 300, panelColor);
            drawText(window, font, "Generated Commands", 20, 275, 340, white);
            drawText(window, font, translationPending ? "GEMINI / WORKING" : "COMMAND OUTPUT",
                     10, 485, 347, translationPending ? sf::Color(56, 189, 248) : grey);
            drawText(window, font, output, 15, 285, 390, green);

            drawPanel(window, 665, 325, 390, 300, panelColor);
            drawText(window, font, "Validation Status", 20, 685, 340, white);
            drawText(window, font, validationStatus, 14, 685, 400,
                     validationStatus.find("ACCEPTED") != std::string::npos ? green : grey);
            drawButton(window, font, "Validate", 685, 455, green);
        }
        else if (currentPage == 2)
        {
            drawPanel(window, 255, 115, 800, 480, panelColor);
            drawText(window, font, "DFA - Deterministic Finite Automaton", 22, 280, 135, blue);
            drawText(window, font, "Step through the token-state trace for the last validation.", 15, 280, 175, white);
            drawText(window, font, "START -> TOKEN -> KEYWORD / NUMBER -> ACCEPT or DEAD", 15, 280, 210, green);
            if (dfaSteps.empty())
                drawText(window, font, "Validate a command first to generate the DFA trace.", 16, 280, 270, grey);
            else
            {
                size_t visible = std::min(dfaStepIndex + 1, dfaSteps.size());
                size_t begin = visible > 8 ? visible - 8 : 0;
                float y = 260;
                for (size_t i = begin; i < visible; ++i)
                {
                    sf::Color c = (i == dfaStepIndex) ? blue : (dfaSteps[i].find("DEAD") != std::string::npos ? red : white);
                    drawText(window, font, dfaSteps[i], 15, 285, y, c);
                    y += 31;
                }
                drawText(window, font, "Step " + std::to_string(dfaStepIndex + 1) + " / " + std::to_string(dfaSteps.size()), 13, 800, 500, grey);
            }
            drawButton(window, font, "Next Step", 280, 535, green);
            drawButton(window, font, "Restart", 450, 535, blue);
        }
        else if (currentPage == 3)
        {
            drawPanel(window, 255, 115, 800, 480, panelColor);
            drawText(window, font, "CFG - Context-Free Grammar Analyzer", 22, 280, 135, blue);
            drawText(window, font, "PROGRAM -> STATEMENT*", 15, 280, 180, white);
            drawText(window, font, "STATEMENT -> MOVE | TURN | REPEAT", 15, 280, 208, white);
            drawText(window, font, "MOVE -> FORWARD(NUMBER) | BACKWARD(NUMBER)", 14, 280, 236, grey);
            drawText(window, font, "TURN -> LEFT | RIGHT", 14, 280, 264, grey);
            drawText(window, font, "REPEAT -> REPEAT(NUMBER){PROGRAM}", 14, 280, 292, grey);
            if (cfgSteps.empty())
                drawText(window, font, "Validate a command first to generate parsing steps.", 15, 280, 355, grey);
            else
            {
                size_t visible = std::min(cfgStepIndex + 1, cfgSteps.size());
                size_t begin = visible > 5 ? visible - 5 : 0;
                float y = 345;
                for (size_t i = begin; i < visible; ++i)
                {
                    sf::Color c = (i == cfgStepIndex) ? blue :
                        (cfgSteps[i].find("REJECTED") != std::string::npos ? red : white);
                    drawText(window, font, cfgSteps[i], 14, 285, y, c);
                    y += 29;
                }
                drawText(window, font, "Step " + std::to_string(cfgStepIndex + 1) + " / " + std::to_string(cfgSteps.size()), 13, 800, 500, grey);
            }
            drawButton(window, font, "Next Step", 280, 535, green);
            drawButton(window, font, "Restart", 450, 535, blue);
        }
        else if (currentPage == 4)
        {
            drawPanel(window, 255, 115, 800, 480, panelColor);
            drawText(window, font, "PDA - Pushdown Automaton", 22, 280, 135, blue);
            drawText(window, font, "Step through stack operations for the last validation.", 15, 280, 175, white);
            drawText(window, font, "Input brace '{' = PUSH     Input brace '}' = POP", 16, 280, 210, green);
            drawText(window, font, "Current stack depth: " + pdaStackState, 14, 280, 235, grey);
            if (pdaSteps.empty())
                drawText(window, font, "Validate a command first to generate the PDA trace.", 16, 280, 270, grey);
            else
            {
                size_t visible = std::min(pdaStepIndex + 1, pdaSteps.size());
                size_t begin = visible > 8 ? visible - 8 : 0;
                float y = 255;
                for (size_t i = begin; i < visible; ++i)
                {
                    sf::Color c = (i == pdaStepIndex) ? blue : (pdaSteps[i].find("REJECT") != std::string::npos ? red : white);
                    drawText(window, font, pdaSteps[i], 15, 285, y, c);
                    y += 31;
                }
                drawText(window, font, "Step " + std::to_string(pdaStepIndex + 1) + " / " + std::to_string(pdaSteps.size()), 13, 800, 500, grey);
            }
            drawButton(window, font, "Next Step", 280, 535, green);
            drawButton(window, font, "Restart", 450, 535, blue);
        }
        else if (currentPage == 5)
        {
            drawPanel(window, 255, 115, 800, 480, panelColor);
            drawText(window, font, "Generated Parse Tree", 22, 280, 135, blue);
            drawText(window, font, parseTree, 15, 285, 195, green);
        }
        else if (currentPage == 6)
        {
            drawPanel(window, 255, 115, 800, 480, panelColor);
            drawText(window, font, "2D Robot Simulator", 22, 280, 135, blue);
            drawText(window, font,
                     commandsValidated ? "Validated program ready to execute." : "Validate a command program before running the robot.",
                     15, 280, 175, commandsValidated ? green : grey);

            const float gridX = 285.0f;
            const float gridY = 215.0f;
            const float cell = 38.0f;
            for (int row = 0; row < 8; ++row)
            {
                for (int col = 0; col < 12; ++col)
                {
                    sf::RectangleShape tile(sf::Vector2f(cell - 2, cell - 2));
                    tile.setPosition(gridX + col * cell, gridY + row * cell);
                    tile.setFillColor(sf::Color(10, 28, 48));
                    tile.setOutlineThickness(1);
                    tile.setOutlineColor(sf::Color(0, 175, 220, 95));
                    window.draw(tile);
                }
            }

            if (robotX >= 0 && robotX < 12 && robotY >= 0 && robotY < 8)
            {
                sf::CircleShape robot(13);
                robot.setOrigin(13, 13);
                robot.setPosition(gridX + robotX * cell + cell / 2,
                                  gridY + robotY * cell + cell / 2);
                robot.setFillColor(blue);
                window.draw(robot);

                sf::ConvexShape nose;
                nose.setPointCount(3);
                if (robotDirection == 0)
                {
                    nose.setPoint(0, sf::Vector2f(0, -10));
                    nose.setPoint(1, sf::Vector2f(-6, 2));
                    nose.setPoint(2, sf::Vector2f(6, 2));
                }
                else if (robotDirection == 1)
                {
                    nose.setPoint(0, sf::Vector2f(10, 0));
                    nose.setPoint(1, sf::Vector2f(-2, -6));
                    nose.setPoint(2, sf::Vector2f(-2, 6));
                }
                else if (robotDirection == 2)
                {
                    nose.setPoint(0, sf::Vector2f(0, 10));
                    nose.setPoint(1, sf::Vector2f(-6, -2));
                    nose.setPoint(2, sf::Vector2f(6, -2));
                }
                else
                {
                    nose.setPoint(0, sf::Vector2f(-10, 0));
                    nose.setPoint(1, sf::Vector2f(2, -6));
                    nose.setPoint(2, sf::Vector2f(2, 6));
                }
                nose.setPosition(gridX + robotX * cell + cell / 2,
                                 gridY + robotY * cell + cell / 2);
                nose.setFillColor(sf::Color::White);
                window.draw(nose);
            }
            else
                drawText(window, font, "Robot moved outside the grid!", 15, 750, 260, red);

            drawButton(window, font, "Run", 275, 555, commandsValidated ? green : grey);
            drawButton(window, font, "Reset", 445, 555, blue);
            std::string state = robotFinished ? "Execution complete" :
                                robotRunning ? "Robot running..." : "Robot paused";
            drawText(window, font, state, 14, 625, 568, white);
            drawText(window, font,
                     "Actions: " + std::to_string(robotActionIndex) + " / " + std::to_string(robotActions.size()),
                     13, 625, 590, grey);
        }

        drawText(window, font, "CommandFlow | Regex | DFA | CFG | PDA", 14, 255, 650, grey);
        window.display();
    }

    return 0;
}
