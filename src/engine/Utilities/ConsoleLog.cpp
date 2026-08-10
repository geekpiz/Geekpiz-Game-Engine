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
            // BUG FIX: the old code just erased the oldest 25% of entries
            // without ever decrementing g_InfoCount/g_WarningCount/g_ErrorCount
            // for the entries being removed. The Code Editor's live syntax
            // check runs on every keystroke, so a long editing session with
            // syntax errors can easily push thousands of "[Script Error]"
            // lines through here - once the trim kicks in, the Error counter
            // shown in the Console window keeps growing forever and never
            // matches what's actually on screen.
            // (기존 코드는 오래된 로그 25%를 지우기만 하고 지워지는 항목들의
            //  개수를 g_InfoCount/g_WarningCount/g_ErrorCount에서 빼주지
            //  않았음. 코드 에디터의 실시간 문법 검사가 키 입력마다 실행되기
            //  때문에, 문법 에러가 있는 상태로 오래 타이핑하면 "[Script
            //  Error]" 로그가 수천 줄 쌓일 수 있고, 트림이 한 번이라도
            //  발생하면 콘솔 창의 에러 카운터가 실제 로그 개수와 어긋난 채
            //  계속 커지기만 했음)
            size_t trimCount = g_Entries.size() / 4;
            for (size_t i = 0; i < trimCount; ++i)
            {
                const LogEntry& trimmed = g_Entries[i];
                if (trimmed.level == LogLevel::Info) g_InfoCount -= trimmed.count;
                else if (trimmed.level == LogLevel::Warning) g_WarningCount -= trimmed.count;
                else if (trimmed.level == LogLevel::Error) g_ErrorCount -= trimmed.count;
            }
            g_Entries.erase(g_Entries.begin(), g_Entries.begin() + trimCount);
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