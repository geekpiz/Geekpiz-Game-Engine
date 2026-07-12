#include "Settings.h"
#include "FileSystem.h" // Include the cross-platform file system module (크로스 플랫폼 파일 시스템 모듈 포함)
#include <sstream>
#include <unordered_map>

namespace L {

    // Internal data storage (내부 데이터 저장소)
    static std::unordered_map<std::string, std::string> Ldata;

    void Load() {
        Ldata.clear(); // Clear existing data (기존 데이터 초기화)

        // Read the file string content through the absolute base engine path (절대적인 기본 엔진 경로를 통해 파일 문자열 내용 읽기)
        std::string fileContent = FS::ReadFile("Settings/Settings.json");

        // Skip processing if the file is empty or missing (파일이 비어있거나 없으면 처리 건너뛰기)
        if (fileContent.empty()) return;

        std::stringstream ss(fileContent);
        std::string line;

        while (std::getline(ss, line)) {
            size_t delimiterPos = line.find(':'); // Find separator (구분자 찾기)
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                Ldata[key] = value; // Store in map (맵에 저장)
            }
        }
    }

    std::string Get(const std::string& key) {
        return Ldata.count(key) ? Ldata[key] : key;
    }
}