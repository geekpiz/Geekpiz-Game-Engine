#include "Settings.h"
#include "FileSystem.h"
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace Settings {

    static std::unordered_map<std::string, std::string> settingsData;

    static std::string TrimString(const std::string& str) {
        if (str.empty()) return "";

        size_t first = 0;
        while (first < str.size() && (std::isspace(static_cast<unsigned char>(str[first])) || str[first] == '"')) {
            first++;
        }
        if (first == str.size()) return "";

        size_t last = str.size() - 1;
        while (last >= first && (std::isspace(static_cast<unsigned char>(str[last])) || str[last] == '"' || str[last] == '\r')) {
            if (last == 0) break;
            last--;
        }
        return str.substr(first, (last - first + 1));
    }

    void Load() {
        settingsData.clear();

        std::string fileContent = FS::ReadFile("Settings/Settings.json");

        if (fileContent.empty()) return;

        std::stringstream ss(fileContent);
        std::string line;

        while (std::getline(ss, line)) {
            size_t delimiterPos = line.find(':');
            if (delimiterPos != std::string::npos) {
                std::string key = TrimString(line.substr(0, delimiterPos));
                std::string value = TrimString(line.substr(delimiterPos + 1));

                if (!key.empty()) {
                    settingsData[key] = value;
                }
            }
        }
    }

    std::string Get(const std::string& key) {
        return settingsData.count(key) ? settingsData[key] : key;
    }

    unsigned int GetColor(const std::string& key, unsigned int defaultColor) {
        if (settingsData.find(key) == settingsData.end()) {
            return defaultColor;
        }

        std::string colorStr = settingsData[key];

        colorStr.erase(std::remove(colorStr.begin(), colorStr.end(), ' '), colorStr.end());
        colorStr.erase(std::remove(colorStr.begin(), colorStr.end(), '\r'), colorStr.end());
        colorStr.erase(std::remove(colorStr.begin(), colorStr.end(), '\n'), colorStr.end());

        if (colorStr.empty()) return defaultColor;

        try {
            if (colorStr.substr(0, 2) != "0x") {
                colorStr = "0x" + colorStr;
            }
            unsigned int parsedColor = static_cast<unsigned int>(std::stoul(colorStr, nullptr, 16));
            return parsedColor;
        }
        catch (...) {
            return defaultColor;
        }
    }
}