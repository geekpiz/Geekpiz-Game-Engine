#pragma once
#include <string>
#include <unordered_map>

namespace L {
    // Load settings from file (파일에서 설정 로드)
    void Load();

    // Get string value by key (키로 문자열 값 가져오기)
    std::string Get(const std::string& key);
}