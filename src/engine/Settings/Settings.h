#pragma once
#include <string>
#include <unordered_map>

namespace Settings {
    // Load settings from file (파일에서 설정 로드)
    void Load();

    // Get string value by key (키로 문자열 값 가져오기)
    std::string Get(const std::string& key);

    // Get color value as hex (색상 값을 16진수로 가져오기)
    unsigned int GetColor(const std::string& key, unsigned int defaultColor);
}