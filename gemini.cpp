#include "gemini.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <string>
#include <thread>
#include <chrono>

using json = nlohmann::json;

static size_t receiveResponse(void* contents, size_t size, size_t count, void* userData)
{
    const size_t total = size * count;
    std::string* response = static_cast<std::string*>(userData);
    response->append(static_cast<char*>(contents), total);
    return total;
}

static std::string trimText(const std::string& value)
{
    size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string cleanModelText(std::string value)
{
    value = trimText(value);
    if (value.rfind("```", 0) == 0)
    {
        size_t firstLine = value.find('\n');
        if (firstLine != std::string::npos) value = value.substr(firstLine + 1);
        size_t closing = value.rfind("```");
        if (closing != std::string::npos) value.erase(closing);
    }
    return trimText(value);
}

bool geminiTranslate(const std::string& instruction,
                     std::string& commands,
                     std::string& error)
{
    commands.clear();
    error.clear();

    const char* key = std::getenv("GEMINI_API_KEY");
    if (key == nullptr || *key == '\0')
    {
        error = "GEMINI_API_KEY environment variable was not found. Restart VS Code after setting it.";
        return false;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        error = "Could not initialize the HTTP library.";
        return false;
    }

    const std::string prompt =
        "You are the natural-language translator for CommandFlow, a small robot command language. "
        "Translate the user's instruction into ONLY the formal commands below. Do not explain. "
        "Allowed statements: FORWARD(number), BACKWARD(number), LEFT, RIGHT, and REPEAT(number){ "
        "followed by nested statements and a closing brace on its own line }. "
        "Use positive whole numbers only. Preserve the requested order and repetition count. "
        "Do not invent actions, distances, or turns. If the instruction cannot be represented "
        "using these commands, return exactly: TRANSLATION_ERROR: unsupported instruction. "
        "Return one command per line. User instruction: " + instruction;

    json request = {
        {"contents", json::array({
            {{"role", "user"}, {"parts", json::array({{{"text", prompt}}})}}
        })},
        {"generationConfig", {
            {"temperature", 0.1},
            {"maxOutputTokens", 500}
        }}
    };

    std::string response;
    CURL* curl = curl_easy_init();
    if (curl == nullptr)
    {
        curl_global_cleanup();
        error = "Could not create an HTTP request.";
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string apiHeader = std::string("x-goog-api-key: ") + key;
    headers = curl_slist_append(headers, apiHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.8-flash:generateContent");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    const std::string body = request.dump();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receiveResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);

    CURLcode result = CURLE_OK;
    long httpStatus = 0;

    // Retry temporary server-capacity errors. This function is called from
    // the background worker, so waiting here does not freeze the SFML window.
    const int maxAttempts = 3;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt)
    {
        response.clear();
        result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

        if (result != CURLE_OK || httpStatus != 503 || attempt == maxAttempts)
            break;

        // Exponential backoff: wait 2 seconds, then 4 seconds.
        std::this_thread::sleep_for(std::chrono::seconds(2 * attempt));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (result != CURLE_OK)
    {
        error = std::string("Network request failed: ") + curl_easy_strerror(result);
        return false;
    }

    json data;
    try
    {
        data = json::parse(response);
    }
    catch (...)
    {
        error = "Gemini returned an unreadable response (HTTP " + std::to_string(httpStatus) + ").";
        return false;
    }

    if (httpStatus < 200 || httpStatus >= 300)
    {
        if (data.contains("error") && data["error"].contains("message"))
            error = data["error"]["message"].get<std::string>();
        else
            error = "Gemini request failed with HTTP status " + std::to_string(httpStatus) + ".";

        if (httpStatus == 503)
            error += " The model is temporarily overloaded. CommandFlow retried 3 times; please try again shortly.";
        else if (httpStatus == 429)
            error += " Check your Gemini API rate limits and quota in AI Studio.";
        return false;
    }

    try
    {
        commands = cleanModelText(data.at("candidates").at(0)
            .at("content").at("parts").at(0).at("text").get<std::string>());
    }
    catch (...)
    {
        error = "Gemini did not return command text. Try a simpler instruction.";
        return false;
    }

    if (commands.empty())
    {
        error = "Gemini returned an empty translation.";
        return false;
    }

    return true;
}
