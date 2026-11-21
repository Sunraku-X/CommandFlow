# CommandFlow
# CommandFlow
### AI-Powered Command Language Validator

CommandFlow is a C++ application developed as a practical implementation of concepts from Theory of Computation. It allows users to give instructions in everyday English, converts them into a predefined command language using Google's Gemini API, and validates the generated commands before execution.

The project combines natural-language processing with formal language concepts to demonstrate how theoretical models can be used in a working application.

## About the Project

The main idea behind CommandFlow is to make interaction with a virtual robot simpler. Instead of manually writing commands, users can describe the actions they want in natural language.

For example:

**Input:**
```text
Move forward 3 steps, turn left, and move forward 2 steps.
```

**Generated Commands:**
```text
FORWARD(3)
LEFT
FORWARD(2)
```

The generated commands are then checked using different validation techniques. Only commands that pass the validation process can be executed in the robot simulator.

## Features

- **Natural Language Translation:** Converts English instructions into structured commands using the Google Gemini API.
- **Regex and DFA Validation:** Checks whether individual commands follow the defined token patterns.
- **CFG Parsing:** Validates the grammatical structure of commands, including nested repetition blocks.
- **PDA Validation:** Uses a stack to check balanced braces and nested structures.
- **Parse Tree Generation:** Displays the structure of a validated command program.
- **Robot Simulation:** Executes valid commands in a 2D environment.
- **Interactive Visualizers:** Provides separate sections to explore DFA transitions, CFG parsing steps, and PDA stack operations.
- **Manual Mode:** Allows users to enter commands directly without using AI.

## Technologies Used

| Technology | Purpose |
|---|---|
| C++ | Core application and program logic |
| SFML 2.6 | Graphical interface and robot simulation |
| Google Gemini API | Natural-language translation |
| Regular Expressions | Command pattern matching |
| DFA | Token recognition |
| CFG | Syntax and grammar validation |
| PDA | Nested structure validation |
| libcurl | API communication |
| nlohmann/json | JSON handling |

## Command Language

CommandFlow supports the following commands:

| Command | Description |
|---|---|
| `FORWARD(n)` | Moves the robot forward by n steps |
| `BACKWARD(n)` | Moves the robot backward by n steps |
| `LEFT` | Turns the robot left |
| `RIGHT` | Turns the robot right |
| `REPEAT(n){...}` | Repeats a block of commands n times |

Nested repetition blocks are also supported.

## How It Works

The application follows a sequence of steps:

1. The user enters an instruction in natural language.
2. Gemini translates the instruction into the predefined command language.
3. Regular Expressions and DFA-based validation check command patterns.
4. The CFG parser checks the grammatical structure of the program.
5. The PDA checks the balance of opening and closing braces.
6. A parse tree is generated for valid programs.
7. The validated commands are executed in the 2D robot simulator.

AI is responsible only for translation. The actual validation and execution decisions are handled by the application's formal language components.

## Application Modules

The application consists of the following sections:

- Dashboard
- Command Editor
- DFA Visualizer
- CFG Analyzer
- PDA Visualizer
- Parse Tree
- Robot Simulator

Each module demonstrates a different part of the command processing and validation system.

## Installation and Setup

### Requirements

- Windows operating system
- C++17 compatible compiler
- MSYS2 UCRT64 environment
- SFML 2.6
- libcurl
- nlohmann/json
- Google Gemini API key

### Install Dependencies

Open the MSYS2 UCRT64 terminal and install the required packages:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc
pacman -S mingw-w64-ucrt-x86_64-sfml
pacman -S mingw-w64-ucrt-x86_64-curl
pacman -S mingw-w64-ucrt-x86_64-nlohmann-json
```

### Clone the Repository

```bash
git clone https://github.com/YOUR-USERNAME/CommandFlow.git
cd CommandFlow
```

### Configure the Gemini API

1. Visit [Google AI Studio](https://aistudio.google.com/app/apikey).
2. Generate an API key.
3. Set the key as a Windows environment variable named `GEMINI_API_KEY`.
4. Restart your terminal or VS Code.

Do not hardcode your API key or upload it to GitHub.

### Compile the Project

Run the following command in PowerShell:

```powershell
g++ -std=c++17 main.cpp gemini.cpp validator.cpp parser.cpp pda.cpp parsetree.cpp -o CommandFlow.exe -I"D:/msys64/ucrt64/include" -L"D:/msys64/ucrt64/lib" -lsfml-graphics -lsfml-window -lsfml-system -lcurl -pthread
```

Update the paths if MSYS2 is installed in a different directory.

### Run the Application

```powershell
.\CommandFlow.exe
```

## Project Structure

```text
CommandFlow/
│
├── main.cpp
├── gemini.cpp
├── gemini.h
├── validator.cpp
├── validator.h
├── parser.cpp
├── parser.h
├── pda.cpp
├── pda.h
├── parsetree.cpp
├── parsetree.h
│
├── screenshots/
├── README.md
└── .gitignore
```

## Future Scope

Some possible improvements for the project include:

- Supporting more natural-language instructions.
- Expanding the command language with additional robot actions.
- Improving the robot simulation and its visual representation.
- Adding more detailed automata and parsing visualizations.
- Allowing users to save and load command programs.

## Project Information

**Project Name:** CommandFlow – AI-Powered Command Language Validator  
**Domain:** Theory of Computation  
**Programming Language:** C++  
**Graphics Library:** SFML  
**AI Integration:** Google Gemini API

Developed as an academic project to explore the practical applications of formal languages, automata theory, and parsing.

---

The project aims to connect natural-language instructions with formal command structures and demonstrate how concepts from Theory of Computation can be applied in an interactive software application.
