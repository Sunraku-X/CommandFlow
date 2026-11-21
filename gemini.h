#ifndef GEMINI_H
#define GEMINI_H

#include <string>

bool geminiTranslate(const std::string& instruction,
                     std::string& commands,
                     std::string& error);

#endif
