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

    // Delete a file if it exists. Returns true if the file is gone
    // afterwards (including when it never existed to begin with), false
    // only on an actual filesystem error. Used by named-layout deletion.
    // (파일이 있으면 삭제함. 삭제 후 파일이 없으면 true(원래 없었던 경우
    //  포함), 실제 파일시스템 오류일 때만 false. 이름 있는 레이아웃 삭제에 사용됨)
    bool DeleteFile(const std::string& relativePath);
}