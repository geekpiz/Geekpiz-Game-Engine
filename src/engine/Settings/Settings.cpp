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

    // -----------------------------------------------------------------
    // Write-back API (NEW) (설정 저장용 API - 신규 추가)
    // -----------------------------------------------------------------
    void Set(const std::string& key, const std::string& value) {
        settingsData[key] = value;
    }

    float GetFloat(const std::string& key, float defaultValue) {
        auto it = settingsData.find(key);
        if (it == settingsData.end() || it->second.empty()) return defaultValue;

        try {
            return std::stof(it->second);
        }
        catch (...) {
            return defaultValue; // Corrupted/hand-edited value - fall back instead of crashing (값이 손상/직접 수정됐으면 크래시 대신 기본값 사용)
        }
    }

    void SetFloat(const std::string& key, float value) {
        settingsData[key] = std::to_string(value);
    }

    bool GetBool(const std::string& key, bool defaultValue) {
        auto it = settingsData.find(key);
        if (it == settingsData.end() || it->second.empty()) return defaultValue;
        return it->second == "1" || it->second == "true" || it->second == "True";
    }

    void SetBool(const std::string& key, bool value) {
        settingsData[key] = value ? "1" : "0";
    }

    void Save() {
        // Keep the exact "key:value" per-line format Load() parses -
        // Settings.json isn't real JSON despite the extension, it's a
        // flat line-based config, so we must never write a value that
        // contains a newline (that would silently corrupt the next
        // Load()). Anything multi-line (like the docking layout) belongs
        // in its own file - see Layout.cpp.
        // (Load()가 파싱하는 "key:value" 줄 포맷을 그대로 유지함 -
        //  Settings.json은 확장자와 다르게 실제 JSON이 아니라 줄 기반의
        //  단순한 설정 파일이라, 값 안에 줄바꿈이 들어가면 다음 Load()가
        //  조용히 깨짐. 여러 줄짜리 데이터(도킹 레이아웃 등)는 별도
        //  파일로 저장해야 함 - Layout.cpp 참고)
        std::ostringstream out;
        for (const auto& pair : settingsData) {
            bool hasNewline = pair.second.find('\n') != std::string::npos ||
                pair.second.find('\r') != std::string::npos;
            if (hasNewline) continue; // Skip - would corrupt the file format (파일 포맷을 깨뜨리므로 건너뜀)
            out << pair.first << ":" << pair.second << "\n";
        }
        FS::WriteFile("Settings/Settings.json", out.str());
    }
}