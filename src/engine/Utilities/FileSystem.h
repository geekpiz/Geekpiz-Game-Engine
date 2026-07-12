#pragma once

#include <string>
#include <filesystem>

namespace FS {
    // Get the root path of the engine data directory (엔진 데이터 디렉토리의 루트 경로 가져오기)
    std::filesystem::path GetEngineRootPath();

    // Read the entire content of a file as a string (파일의 전체 내용을 문자열로 읽기)
    std::string ReadFile(const std::string& relativePath);

    // Write a string content to a file (문자열 내용을 파일에 쓰기)
    bool WriteFile(const std::string& relativePath, const std::string& content);
}