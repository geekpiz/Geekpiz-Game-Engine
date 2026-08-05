#pragma once
#include <string>
#include <vector>

enum class LogLevel {
    Info,
    Warning,
    Error
};

struct LogEntry {
    LogLevel level;
    std::string message; // (comment: 'text' 대신 'message'로 통일)
    int count = 1;
};

namespace ConsoleLog
{
    // 로그 추가 함수
    void Append(LogLevel level, const std::string& line);

    // 로그 초기화 함수
    void Clear();

    // 로그 목록 반환
    const std::vector<LogEntry>& GetEntries();

    // 카운터 조회 함수 (Info, Warning, Error 개수)
    int GetInfoCount();
    int GetWarningCount();
    int GetErrorCount();
}