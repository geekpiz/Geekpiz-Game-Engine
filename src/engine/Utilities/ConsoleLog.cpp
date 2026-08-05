#include "ConsoleLog.h"
#include <mutex>
#include <vector>

namespace ConsoleLog
{
    namespace
    {
        std::vector<LogEntry> g_Entries;
        constexpr size_t kMaxEntries = 5000;
        std::mutex g_Mutex;

        int g_InfoCount = 0;
        int g_WarningCount = 0;
        int g_ErrorCount = 0;

        void IncrementCount(LogLevel level)
        {
            if (level == LogLevel::Info) g_InfoCount++;
            else if (level == LogLevel::Warning) g_WarningCount++;
            else if (level == LogLevel::Error) g_ErrorCount++;
        }
    }

    void Append(LogLevel level, const std::string& line)
    {
        std::lock_guard<std::mutex> lock(g_Mutex);

        IncrementCount(level);

        // Check duplicates with the last entry only (직전 로그 항목과 동일한 경우 카운트만 증가)
        if (!g_Entries.empty())
        {
            auto& lastEntry = g_Entries.back();
            if (lastEntry.level == level && lastEntry.message == line)
            {
                lastEntry.count++;
                return;
            }
        }

        // Add new log entry (새 로그 항목 추가)
        g_Entries.push_back({ level, line, 1 });

        // Buffer limit protection (최대 개수 초과 시 오래된 로그 제거)
        if (g_Entries.size() > kMaxEntries)
        {
            g_Entries.erase(g_Entries.begin(), g_Entries.begin() + (g_Entries.size() / 4));
        }
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_Entries.clear();
        g_InfoCount = 0;
        g_WarningCount = 0;
        g_ErrorCount = 0;
    }

    const std::vector<LogEntry>& GetEntries()
    {
        return g_Entries;
    }

    int GetInfoCount() { return g_InfoCount; }
    int GetWarningCount() { return g_WarningCount; }
    int GetErrorCount() { return g_ErrorCount; }
}