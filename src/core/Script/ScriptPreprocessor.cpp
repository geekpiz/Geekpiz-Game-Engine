// ScriptPreprocessor.cpp
#include "ScriptPreprocessor.h"
#include <sstream>
#include <cctype>

namespace Geekpiz
{
    namespace
    {
        size_t FirstNonSpace(const std::string& line)
        {
            size_t i = 0;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
                ++i;
            return i;
        }

        bool MatchesKeyword(const std::string& line, size_t pos, const std::string& word)
        {
            if (line.compare(pos, word.size(), word) != 0)
                return false;
            size_t after = pos + word.size();
            return after < line.size() && std::isspace(static_cast<unsigned char>(line[after]));
        }

        std::string ExtractFunctionName(const std::string& line, size_t pos)
        {
            static const std::string kFunctionKeyword = "function";
            if (line.compare(pos, kFunctionKeyword.size(), kFunctionKeyword) != 0)
                return "";

            size_t nameStart = pos + kFunctionKeyword.size();
            while (nameStart < line.size() && std::isspace(static_cast<unsigned char>(line[nameStart])))
                ++nameStart;

            size_t nameEnd = nameStart;
            while (nameEnd < line.size() &&
                   (std::isalnum(static_cast<unsigned char>(line[nameEnd])) || line[nameEnd] == '_'))
                ++nameEnd;

            if (nameEnd == nameStart)
                return "";

            return line.substr(nameStart, nameEnd - nameStart);
        }

        bool TryHandleLifecycleLine(
            const std::string& line,
            size_t contentStart,
            const std::string& keyword,
            std::vector<std::string>& outNameList,
            std::ostringstream& output)
        {
            if (!MatchesKeyword(line, contentStart, keyword))
                return false;

            size_t afterKeyword = contentStart + keyword.size();
            while (afterKeyword < line.size() && std::isspace(static_cast<unsigned char>(line[afterKeyword])))
                ++afterKeyword;

            std::string name = ExtractFunctionName(line, afterKeyword);
            if (name.empty())
                return false;

            outNameList.push_back(name);
            output << line.substr(0, contentStart) << line.substr(afterKeyword) << '\n';
            return true;
        }

        // Trims leading/trailing whitespace and, if the whole thing is
        // wrapped in matching "..." or '...', strips the quotes too.
        // (앞뒤 공백을 없애고, 전체가 "..."나 '...'로 감싸져 있으면
        //  따옴표도 벗겨냄)
        std::string TrimAndUnquote(const std::string& raw)
        {
            size_t b = 0, e = raw.size();
            while (b < e && std::isspace(static_cast<unsigned char>(raw[b]))) ++b;
            while (e > b && std::isspace(static_cast<unsigned char>(raw[e - 1]))) --e;
            if (e - b >= 2 && (raw[b] == '"' || raw[b] == '\'') && raw[e - 1] == raw[b])
            {
                ++b; --e;
            }
            return raw.substr(b, e - b);
        }

        // Handles "getkey(KEY, MODE) function NAME(...)". Looks for
        // "getkey" immediately followed by "(" at contentStart (no
        // space required, like a normal call), a matching ")" (simple
        // scan - the argument grammar here is just "thing, thing", no
        // nested parens expected), then "function NAME(" after it.
        // (getkey(KEY, MODE) function NAME(...) 처리. contentStart에서
        //  "getkey" 바로 뒤에 "("가 오는지 보고(일반 함수 호출처럼
        //  공백 없이), 짝이 맞는 ")"를 찾고(인자 문법이 "값, 값"뿐이라
        //  중첩 괄호는 없다고 가정하는 단순 스캔), 그 뒤에서
        //  "function NAME("을 찾음)
        bool TryHandleGetKeyLine(
            const std::string& line,
            size_t contentStart,
            std::vector<KeyBinding>& outBindings,
            std::ostringstream& output)
        {
            static const std::string kGetKey = "getkey";
            if (line.compare(contentStart, kGetKey.size(), kGetKey) != 0)
                return false;

            size_t parenOpen = contentStart + kGetKey.size();
            if (parenOpen >= line.size() || line[parenOpen] != '(')
                return false; // "getkey" wasn't followed by "(" - not our pattern (getkey 뒤에 "("가 없음 - 우리 패턴 아님)

            size_t parenClose = line.find(')', parenOpen);
            if (parenClose == std::string::npos)
                return false; // malformed - leave the line alone, Squirrel will report a real error anyway (형태가 이상함 - 줄을 안 건드리고 놔둠, 어차피 스쿼럴이 진짜 에러를 알려줄 것)

            std::string args = line.substr(parenOpen + 1, parenClose - parenOpen - 1);
            size_t comma = args.find(',');
            if (comma == std::string::npos)
                return false; // needs exactly two args (인자가 정확히 2개 필요함)

            std::string keyName = TrimAndUnquote(args.substr(0, comma));
            std::string mode = TrimAndUnquote(args.substr(comma + 1));

            size_t afterParen = parenClose + 1;
            while (afterParen < line.size() && std::isspace(static_cast<unsigned char>(line[afterParen])))
                ++afterParen;

            std::string name = ExtractFunctionName(line, afterParen);
            if (name.empty())
                return false;

            outBindings.push_back(KeyBinding{keyName, mode, name});

            // Rewrite to plain "function NAME(...)", same trick as
            // start/update: drop everything before "function".
            // (start/update와 같은 방식: "function" 앞부분을 전부
            //  버리고 다시 씀)
            output << line.substr(0, contentStart) << line.substr(afterParen) << '\n';
            return true;
        }

        // Replaces every standalone `cupdate()` call in one line with
        // the Squirrel keyword `yield`. Word-boundary aware (won't
        // touch "mycupdate()"), and only matches the exact
        // no-argument-call shape "cupdate" + optional spaces + "(" +
        // optional spaces + ")", since that's the only shape cupdate()
        // is meant to be used in.
        // (한 줄 안의 모든 독립적인 cupdate() 호출을 스쿼럴 키워드
        //  `yield`로 바꿈. 단어 경계를 확인해서 "mycupdate()" 같은 건
        //  안 건드리고, "cupdate" + 공백* + "(" + 공백* + ")" 형태만
        //  매칭함 - cupdate()는 원래 이 형태로만 쓰이게 되어 있으니까)
        std::string ReplaceCupdateCalls(const std::string& line)
        {
            static const std::string kName = "cupdate";
            std::string result;
            result.reserve(line.size());

            size_t i = 0;
            while (i < line.size())
            {
                bool isWordStart = (i == 0) ||
                    !(std::isalnum(static_cast<unsigned char>(line[i - 1])) || line[i - 1] == '_');

                if (isWordStart && line.compare(i, kName.size(), kName) == 0)
                {
                    size_t after = i + kName.size();
                    bool notPartOfLongerWord = after >= line.size() ||
                        !(std::isalnum(static_cast<unsigned char>(line[after])) || line[after] == '_');

                    if (notPartOfLongerWord)
                    {
                        size_t p = after;
                        while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;
                        if (p < line.size() && line[p] == '(')
                        {
                            ++p;
                            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;
                            if (p < line.size() && line[p] == ')')
                            {
                                result += "yield";
                                i = p + 1;
                                continue;
                            }
                        }
                    }
                }

                result += line[i];
                ++i;
            }
            return result;
        }
    }

    std::string PreprocessLifecycleAttributes(const std::string& source, ScriptLifecycleInfo& outInfo)
    {
        std::istringstream input(source);
        std::ostringstream output;
        std::string line;

        while (std::getline(input, line))
        {
            size_t contentStart = FirstNonSpace(line);

            if (TryHandleLifecycleLine(line, contentStart, "start", outInfo.startFunctionNames, output))
                continue;

            if (TryHandleLifecycleLine(line, contentStart, "update", outInfo.updateFunctionNames, output))
                continue;

            if (TryHandleGetKeyLine(line, contentStart, outInfo.keyBindings, output))
                continue;

            // Not a special-attribute line - still run the cupdate()
            // rewrite on it (cupdate can appear on ANY line, deep
            // inside a loop, so it's checked independently of the
            // three cases above).
            // (특수 속성 줄이 아님 - 그래도 cupdate() 변환은 적용함
            //  (cupdate는 루프 안쪽 등 "어떤" 줄에도 나올 수 있으니까
            //  위 세 경우와 별개로 항상 확인함))
            output << ReplaceCupdateCalls(line) << '\n';
        }

        return output.str();
    }
}
