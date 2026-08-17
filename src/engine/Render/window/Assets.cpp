#include "Window.h"
#include "FileSystem.h" // FileSystem (파일 시스템 유틸리티)
#include "Settings.h" // Settings (아이콘 크기 등 설정 값 저장/로드)
#include "Language/Language.h" // Language (다국어 지원)
#include "ConsoleLog.h" // ConsoleLog (콘솔 로그 출력)
#include <filesystem>
#include <string>
#include <vector>
#include <system_error>
#include <algorithm> // std::sort (folders-first, alphabetical ordering in the asset grid) (에셋 그리드의 "폴더 우선 + 알파벳순" 정렬에 사용)
#include <cctype>    // tolower (explicit include instead of relying on it arriving transitively) (다른 헤더를 통해 우연히 딸려오는 것에 기대지 않고 명시적으로 포함)

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace Window {

    bool render_Assets = true; // Render Assets Window Toggle (에셋 창 렌더링 여부)

    // BUG FIX (crash root cause): this used to default to
    // fs::current_path(), which is the BUILD OUTPUT folder when the
    // editor .exe is launched straight from Visual Studio for debugging
    // (no --project-path argument, so Main.cpp never calls
    // SetProjectDirectory()). That put CMakeCache.txt, the editor's own
    // .exe/.pdb/.ilk, and squirrel.lib in the Assets grid - opening
    // CMakeCache.txt (matches the ".txt" text-file rule) loaded a huge,
    // unrelated file straight into the Code Editor, and creating a new
    // script wrote it into the build tree instead of a real project.
    // Defaulting instead to a dedicated folder under the engine's own
    // AppData root means there's always a real, empty, editor-owned
    // project folder to fall back to. (크래시 근본 원인 수정: 기존엔
    // fs::current_path()가 기본값이었는데, 비주얼 스튜디오에서 에디터
    // exe를 바로 디버그 실행하면 이 값이 빌드 출력 폴더가 됨(--project-path
    // 인자가 없어서 Main.cpp가 SetProjectDirectory()를 호출하지 않음).
    // 그 결과 에셋 그리드에 CMakeCache.txt, 에디터 자신의 .exe/.pdb/.ilk,
    // squirrel.lib가 그대로 노출됐고, CMakeCache.txt(".txt" 규칙에 걸림)를
    // 열면 거대하고 무관한 파일이 코드 에디터에 그대로 로드됐으며, 새
    // 스크립트를 만들면 실제 프로젝트가 아니라 빌드 트리 안에 생성됐음.
    // 대신 엔진 자체 AppData 루트 아래 전용 폴더를 기본값으로 두면 항상
    // 실제로 존재하는, 에디터가 관리하는 빈 프로젝트 폴더로 대체됨)
    static fs::path GetDefaultProjectPath()
    {
        fs::path defaultPath = FS::GetEngineRootPath() / "Projects" / "DefaultProject";
        std::error_code ec;
        fs::create_directories(defaultPath, ec);
        return defaultPath;
    }

    // Root Project Path (프로젝트 최상위 폴더 경로)
    static fs::path g_ProjectPath = GetDefaultProjectPath();

    // Current Navigation Path inside Project Folder (프로젝트 폴더 내 현재 탐색 경로)
    static fs::path g_CurrentAssetPath = g_ProjectPath;
    static std::string g_SelectedFilePath = ""; // Currently Selected File/Folder Path (현재 선택된 파일/폴더 경로)

    // Popup States (팝업 상태 변수)
    static bool g_ShowRenamePopup = false;
    static bool g_ShowNewFolderPopup = false;
    static bool g_ShowNewScriptPopup = false;
    static char g_InputBuffer[256] = "";

    // -----------------------------------------------------------------
    // Icon Size & Search (아이콘 크기 및 검색)
    // -----------------------------------------------------------------
    // Multiplies every grid cell dimension below (cellWidth/iconAreaHeight/
    // iconSize). Persisted to Settings.json so it survives restarts, same
    // as the docking layout - loaded lazily on first use rather than at
    // static-init time because Settings::Load() may not have run yet when
    // this translation unit's statics are constructed. (아래 그리드 셀
    // 크기(cellWidth/iconAreaHeight/iconSize) 전체에 곱해지는 배율.
    // 도킹 레이아웃처럼 Settings.json에 저장돼 재시작해도 유지됨 - 정적
    // 초기화 시점엔 Settings::Load()가 아직 실행 안 됐을 수 있어서 처음
    // 쓰일 때 지연 로드함)
    static const char* kIconScaleKey = "Assets_IconScale";
    static float g_AssetIconScale = -1.0f; // -1 = "not loaded yet" sentinel (아직 로드 안 됨을 뜻하는 값)
    static char g_SearchBuffer[128] = "";

    static float GetAssetIconScale()
    {
        if (g_AssetIconScale < 0.0f)
        {
            g_AssetIconScale = Settings::GetFloat(kIconScaleKey, 1.0f);
        }
        return g_AssetIconScale;
    }

    // Case-insensitive substring match, ASCII-only on purpose - the
    // filename bytes here are UTF-8, and naively lower-casing multi-byte
    // Korean bytes with tolower() would corrupt them. ASCII letters
    // (a-z/A-Z) are single-byte in UTF-8 and never appear as a
    // continuation byte of a multi-byte sequence, so this is safe: it
    // just won't case-fold Korean characters (Korean has no case anyway).
    // (대소문자 구분 없는 부분 문자열 검색, 의도적으로 ASCII만 처리함 -
    // 파일명 바이트는 UTF-8인데 tolower()로 한글 멀티바이트를 함부로
    // 소문자화하면 깨짐. ASCII 알파벳(a-z/A-Z)은 UTF-8에서 항상 1바이트고
    // 멀티바이트 시퀀스의 연속 바이트로 절대 나오지 않으므로 안전함 -
    // 한글은 애초에 대소문자가 없으니 그냥 그대로 비교됨)
    static bool MatchesSearch(const std::string& name, const std::string& query)
    {
        if (query.empty()) return true;

        auto foldAscii = [](unsigned char c) -> unsigned char {
            return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c;
            };

        std::string foldedName, foldedQuery;
        foldedName.reserve(name.size());
        for (unsigned char c : name) foldedName.push_back(static_cast<char>(foldAscii(c)));
        foldedQuery.reserve(query.size());
        for (unsigned char c : query) foldedQuery.push_back(static_cast<char>(foldAscii(c)));

        return foldedName.find(foldedQuery) != std::string::npos;
    }

    // External References from CodeEditor (CodeEditor 창 연동을 위한 외부 함수 및 변수 참조)
    extern bool render_CodeEditor;
    extern void OpenInCodeEditor(const std::string& filePath);
    extern std::string g_OpenedFilePath;

    // UTF-8 Path Conversion Helper (ImGui 한글 깨짐 방지를 위한 UTF-8 경로 변환 헬퍼 함수)
    static std::string PathToUtf8String(const fs::path& path)
    {
        auto u8str = path.u8string();
        return std::string(reinterpret_cast<const char*>(u8str.data()), u8str.size());
    }

    // -----------------------------------------------------------------
    // Unity-Style Icon Grid Support (유니티 스타일 아이콘 그리드 지원)
    // -----------------------------------------------------------------
    // All icons below are drawn procedurally with ImDrawList primitives
    // instead of loaded image textures. On an 8GB RAM / 1GB VRAM machine,
    // every thumbnail texture the Assets tab would otherwise keep resident
    // adds up fast, so drawing flat-color shapes costs zero VRAM and zero
    // decode/allocation time per frame.
    // (아래 아이콘들은 이미지 텍스처를 로드하는 대신 ImDrawList 도형으로
    //  직접 그림. 8GB RAM / 1GB VRAM 환경에서는 에셋 탭이 들고 있어야 할
    //  썸네일 텍스처 하나하나가 부담이 되기 때문에, 단색 도형을 그리는
    //  방식은 VRAM 비용과 매 프레임 디코딩/할당 비용이 전혀 없음)

    // Broad category used to pick an icon shape/color for a given entry (항목별 아이콘 모양/색을 고르기 위한 대분류)
    enum class AssetIconKind
    {
        Folder,
        Script,  // .nut (Squirrel script)
        Code,    // .cpp / .h / .hpp
        Text,    // .txt / .md
        Json,    // .json
        Image,   // .png / .jpg / .jpeg / .bmp / .tga
        Generic  // anything else
    };

    // Classify a directory entry into an icon category by extension (확장자로 디렉토리 항목의 아이콘 카테고리 분류)
    static AssetIconKind ClassifyAssetIcon(bool isDirectory, const fs::path& path)
    {
        if (isDirectory) return AssetIconKind::Folder;

        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        if (ext == ".nut") return AssetIconKind::Script;
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp") return AssetIconKind::Code;
        if (ext == ".txt" || ext == ".md") return AssetIconKind::Text;
        if (ext == ".json") return AssetIconKind::Json;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") return AssetIconKind::Image;
        return AssetIconKind::Generic;
    }

    // Draw a single procedural icon centered at `center`, `size` pixels tall (`center`를 중심으로 높이 `size` 픽셀짜리 아이콘 하나를 그림)
    static void DrawAssetIcon(ImDrawList* drawList, AssetIconKind kind, ImVec2 center, float size)
    {
        const float half = size * 0.5f;

        if (kind == AssetIconKind::Folder)
        {
            // Two-tone folder shape: darker back tab + lighter front body (이중톤 폴더 모양: 어두운 뒷탭 + 밝은 앞면)
            const ImU32 backColor = IM_COL32(96, 150, 196, 255);
            const ImU32 frontColor = IM_COL32(130, 186, 230, 255);

            ImVec2 tabMin(center.x - half, center.y - half * 0.85f);
            ImVec2 tabMax(center.x - half * 0.15f, center.y - half * 0.30f);
            drawList->AddRectFilled(tabMin, tabMax, backColor, 2.0f);

            ImVec2 bodyMin(center.x - half, center.y - half * 0.45f);
            ImVec2 bodyMax(center.x + half, center.y + half * 0.80f);
            drawList->AddRectFilled(bodyMin, bodyMax, backColor, 2.0f);

            ImVec2 frontMin(center.x - half + 3.0f, center.y - half * 0.15f);
            ImVec2 frontMax(center.x + half - 3.0f, center.y + half * 0.80f - 3.0f);
            drawList->AddRectFilled(frontMin, frontMax, frontColor, 2.0f);
            return;
        }

        // Every non-folder kind shares a document base (dog-eared rectangle) with a colored accent band (폴더가 아닌 모든 종류는 접힌 모서리 문서 모양을 공유하고 색상 강조 밴드로 구분함)
        ImU32 accentColor;
        switch (kind)
        {
            case AssetIconKind::Script:  accentColor = IM_COL32(230, 150, 60, 255);  break; // Squirrel script (스쿼럴 스크립트)
            case AssetIconKind::Code:    accentColor = IM_COL32(90, 160, 230, 255);  break; // C++ source (C++ 소스)
            case AssetIconKind::Text:    accentColor = IM_COL32(190, 190, 190, 255); break; // Plain text (일반 텍스트)
            case AssetIconKind::Json:    accentColor = IM_COL32(220, 200, 80, 255);  break; // JSON data (JSON 데이터)
            case AssetIconKind::Image:   accentColor = IM_COL32(110, 200, 130, 255); break; // Image file (이미지 파일)
            default:                     accentColor = IM_COL32(150, 150, 150, 255); break; // Generic (일반 파일)
        }

        const float docHalfW = half * 0.68f;
        const float foldSize = half * 0.32f;
        ImVec2 docMin(center.x - docHalfW, center.y - half);
        ImVec2 docMax(center.x + docHalfW, center.y + half);

        drawList->AddRectFilled(docMin, docMax, IM_COL32(228, 228, 228, 255), 2.0f);

        // Folded corner (접힌 모서리)
        ImVec2 foldP1(docMax.x - foldSize, docMin.y);
        ImVec2 foldP2(docMax.x, docMin.y);
        ImVec2 foldP3(docMax.x, docMin.y + foldSize);
        drawList->AddTriangleFilled(foldP1, foldP2, foldP3, IM_COL32(180, 180, 180, 255));

        // Accent band across the top identifies the file type by color (상단 강조 밴드가 색상으로 파일 종류를 구분함)
        ImVec2 bandMax(docMax.x - foldSize, docMin.y + half * 0.30f);
        drawList->AddRectFilled(docMin, bandMax, accentColor, 0.0f);

        if (kind == AssetIconKind::Image)
        {
            // Small mountain + sun glyph inside the document to read as "image" at a glance (문서 안 작은 산+태양 표시로 한눈에 "이미지"임을 알 수 있게 함)
            float glyphY = docMax.y - half * 0.45f;
            drawList->AddCircleFilled(ImVec2(center.x - docHalfW * 0.35f, docMin.y + half * 0.62f), half * 0.14f, IM_COL32(255, 220, 120, 255));
            drawList->AddTriangleFilled(
                ImVec2(docMin.x + 3.0f, glyphY + half * 0.35f),
                ImVec2(center.x - docHalfW * 0.1f, glyphY - half * 0.15f),
                ImVec2(center.x + docHalfW * 0.5f, glyphY + half * 0.35f),
                IM_COL32(120, 170, 130, 255));
        }
    }

    // Trim `label` (with an ellipsis) so it renders within `maxLines` lines at `wrapWidth`,
    // using the currently active ImGui font for measurement.
    // (현재 활성화된 ImGui 폰트로 측정해서, `label`이 `wrapWidth` 안에서 `maxLines`
    //  줄 이내로 들어가도록 말줄임표를 붙여 잘라냄)
    static std::string TrimLabelToLines(const std::string& label, float wrapWidth, int maxLines)
    {
        ImVec2 fullSize = ImGui::CalcTextSize(label.c_str(), nullptr, false, wrapWidth);
        float lineHeight = ImGui::GetTextLineHeight();
        if (fullSize.y <= lineHeight * maxLines + 1.0f)
        {
            return label; // Already fits (이미 다 들어감)
        }

        // Collect only byte offsets that sit on a UTF-8 character boundary, so a cut
        // can never split a multi-byte character (e.g. Korean) in half and produce
        // garbled text.
        // (UTF-8 문자 경계에 있는 바이트 오프셋만 모아서, 한글 같은 멀티바이트
        //  문자가 중간에 잘려서 깨진 글자가 나오는 일이 없도록 함)
        std::vector<size_t> boundaries;
        boundaries.push_back(0);
        for (size_t i = 0; i < label.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(label[i]);
            // A byte is NOT a continuation byte (10xxxxxx) => it starts a new character (연속 바이트(10xxxxxx)가 아니면 새 문자의 시작임)
            if ((c & 0xC0) != 0x80 && i != 0) boundaries.push_back(i);
        }

        // Binary-search the longest valid prefix that still fits, then append "..." (아직 들어가는 가장 긴 유효 접두사를 이진 탐색으로 찾은 뒤 "..." 을 붙임)
        size_t lo = 0, hi = boundaries.size() - 1;
        while (lo < hi)
        {
            size_t mid = (lo + hi + 1) / 2;
            std::string candidate = label.substr(0, boundaries[mid]) + "...";
            ImVec2 size = ImGui::CalcTextSize(candidate.c_str(), nullptr, false, wrapWidth);
            if (size.y <= lineHeight * maxLines + 1.0f) lo = mid;
            else hi = mid - 1;
        }
        return label.substr(0, boundaries[lo]) + "...";
    }

    // Shared NutComponent script template, used by both the "Create
    // NutComponent Script" popup below AND CreateNewScriptAtProjectRoot()
    // (Code Editor Start Page's "New Script" button). Factored out so the
    // template text only lives in one place. (팝업과
    // CreateNewScriptAtProjectRoot() (코드 에디터 시작 화면의 "새 스크립트"
    // 버튼)가 공유하는 NutComponent 스크립트 템플릿. 템플릿 텍스트가 한
    // 곳에만 존재하도록 분리함)
    static bool CreateScriptFileAt(const fs::path& folder, const std::string& scriptNameIn, fs::path& outPath)
    {
        std::string scriptName = scriptNameIn;
        if (scriptName.find(".nut") == std::string::npos) {
            scriptName += ".nut";
        }
        outPath = folder / scriptName;

        std::string templateCode =
            "// Geekpiz NutComponent Script Template\n\n"
            "start function Start()\n"
            "{\n"
            "	\n"
            "}\n\n"
            "update function Update()\n"
            "{\n"
            "	\n"
            "}\n";

        return FS::WriteFile(PathToUtf8String(outPath), templateCode);
    }

    // Exposed for the Code Editor's Start Page "New Script" button - creates
    // a fresh NutComponent script at the current project's root and returns
    // its absolute path (empty string on failure). (코드 에디터 시작 화면의
    // "새 스크립트" 버튼용으로 공개 - 현재 프로젝트 루트에 새
    // NutComponent 스크립트를 만들고 절대 경로를 반환함(실패 시 빈 문자열))
    std::string CreateNewScriptAtProjectRoot()
    {
        fs::path newPath;
        std::string baseName = "NewScript";
        std::string candidate = baseName;
        int suffix = 1;

        // Avoid silently overwriting an existing "NewScript.nut" from a
        // previous click (이전에 만든 "NewScript.nut"을 조용히 덮어쓰지 않도록 함)
        while (fs::exists(g_ProjectPath / (candidate + ".nut")))
        {
            candidate = baseName + std::to_string(++suffix);
        }

        if (!CreateScriptFileAt(g_ProjectPath, candidate, newPath))
        {
            return "";
        }
        return PathToUtf8String(newPath);
    }

    // Helper Function to Set Project Directory from Hub (허브에서 프로젝트를 열었을 때 경로를 설정하는 외부 호출용 함수)
    void SetProjectDirectory(const std::string& projectPath)
    {
        g_ProjectPath = fs::path(projectPath);
        g_CurrentAssetPath = g_ProjectPath; // Start at root project folder (최상위 프로젝트 폴더부터 시작)
        g_SelectedFilePath = "";
    }

    // Open File with System Default App (시스템 기본 앱으로 파일 열기 함수 - 이미지 등)
    static void OpenWithSystemDefaultApp(const fs::path& path)
    {
#ifdef _WIN32
        ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
    }

    // Check if File is Script or Text (스크립트 또는 텍스트 파일 여부 확인 함수)
    static bool IsScriptOrTextFile(const fs::path& path)
    {
        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(tolower(c));
        return (ext == ".nut" || ext == ".txt" || ext == ".json" || ext == ".cpp" || ext == ".h" || ext == ".lua");
    }

    // Recursive Function to Render Folder Tree (좌측 폴더 트리 뷰 재귀 렌더링 함수)
    void RenderFolderTree(const fs::path& currentPath)
    {
        try {
            for (const auto& entry : fs::directory_iterator(currentPath))
            {
                if (entry.is_directory())
                {
                    const auto& path = entry.path();
                    std::string folderName = PathToUtf8String(path.filename());

                    bool hasSubDir = false;
                    for (const auto& subEntry : fs::directory_iterator(path))
                    {
                        if (subEntry.is_directory())
                        {
                            hasSubDir = true;
                            break;
                        }
                    }

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    if (g_CurrentAssetPath == path)
                    {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }
                    if (!hasSubDir)
                    {
                        flags |= ImGuiTreeNodeFlags_Leaf;
                    }

                    ImGui::PushID(folderName.c_str());
                    bool nodeOpen = ImGui::TreeNodeEx(folderName.c_str(), flags);

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        g_CurrentAssetPath = path;
                    }

                    if (nodeOpen)
                    {
                        if (hasSubDir)
                        {
                            RenderFolderTree(path);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();

                    // NOTE: 이전 PushID는 PathToUtf8String(path).c_str()로 임시 std::string의
                    // c_str() 포인터를 전달해 수명 문제로 인한 댕글링 포인터(UB)를 일으켰음.
                    // folderName은 로컬 변수로 수명이 충분하므로 이를 사용하도록 변경함.
                }
            }
        }
        catch (const std::exception& e) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Tree Error: %s", e.what());
        }
    }

    // Render Assets Window Main Function (에셋 메인 렌더링 함수)
    void Render_Assets()
    {
        if (!render_Assets) return;

        if (ImGui::Begin(L::Get("Assets").c_str(), &render_Assets))
        {
            // Ensure Root Project Directory Exists (기본 프로젝트 디렉토리가 없으면 자동 생성)
            if (!fs::exists(g_ProjectPath)) {
                fs::create_directories(g_ProjectPath);
            }

            // -------------------------------------------------------------
            // 1. Top Breadcrumb Bar starting from Project Root (최상위 프로젝트 폴더 기준 경로 바)
            // -------------------------------------------------------------
            ImGui::Text("%s:", L::Get("CurrentPath").c_str());
            ImGui::SameLine();

            fs::path nextPathToSet = g_CurrentAssetPath;
            bool shouldUpdatePath = false;

            // Root Project Folder Name Button (최상위 프로젝트 폴더 버튼)
            std::string rootName = PathToUtf8String(g_ProjectPath.filename()) + " >";
            if (rootName.empty() || rootName == " >") rootName = "Project >";

            if (ImGui::SmallButton(rootName.c_str()))
            {
                nextPathToSet = g_ProjectPath;
                shouldUpdatePath = true;
            }

            // Relative Path Calculation inside Project Folder (프로젝트 폴더 기준 상대 경로 계산)
            fs::path relativePath;
            try {
                relativePath = fs::relative(g_CurrentAssetPath, g_ProjectPath);
            }
            catch (...) {
                relativePath = "";
            }

            if (!relativePath.empty() && relativePath != ".")
            {
                fs::path accumulatedPath = g_ProjectPath;
                for (const auto& part : relativePath)
                {
                    accumulatedPath /= part;
                    ImGui::SameLine();

                    std::string partName = PathToUtf8String(part) + " >";
                    if (ImGui::SmallButton(partName.c_str()))
                    {
                        nextPathToSet = accumulatedPath;
                        shouldUpdatePath = true;
                    }
                }
            }

            if (shouldUpdatePath)
            {
                g_CurrentAssetPath = nextPathToSet;
            }

            ImGui::Separator();

            // -------------------------------------------------------------
            // 1.5. Search Bar (검색 창)
            // -------------------------------------------------------------
            // Filters the grid below by filename substring, case-insensitive
            // for ASCII (see MatchesSearch() for why Korean isn't case-folded -
            // it doesn't need to be). Search only affects the current folder's
            // grid, not the tree on the left, so navigating stays predictable.
            // (아래 그리드를 파일명 부분 문자열로 필터링함, ASCII는 대소문자
            // 구분 없음(MatchesSearch() 참고 - 한글은 대소문자가 없어서 굳이
            // 처리 안 함). 검색은 현재 폴더의 그리드에만 적용되고 왼쪽 트리에는
            // 영향을 주지 않아서 탐색 동작이 예측 가능하게 유지됨)
            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputTextWithHint("##AssetSearch", L::Get("SearchAssets").c_str(), g_SearchBuffer, IM_ARRAYSIZE(g_SearchBuffer));
            if (g_SearchBuffer[0] != '\0')
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("x##ClearSearch"))
                {
                    g_SearchBuffer[0] = '\0';
                }
            }

            ImGui::Separator();

            // -------------------------------------------------------------
            // 2. Main Workspace Table Layout (좌측 트리 / 우측 콘텐츠 테이블 레이아웃)
            // -------------------------------------------------------------
            if (ImGui::BeginTable("AssetBrowserTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("FolderTree", ImGuiTableColumnFlags_WidthStretch, 0.25f);
                ImGui::TableSetupColumn("AssetContents", ImGuiTableColumnFlags_WidthStretch, 0.75f);

                ImGui::TableNextRow();

                // ---------------------------------------------------------
                // Left Column: Folder Tree View (좌측: 프로젝트 폴더 트리 뷰)
                // ---------------------------------------------------------
                ImGui::TableSetColumnIndex(0);
                ImGui::BeginChild("LeftFolderTreeArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);

                // Root Project Folder Node (최상위 프로젝트 루트 노드)
                ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
                if (g_CurrentAssetPath == g_ProjectPath) rootFlags |= ImGuiTreeNodeFlags_Selected;

                std::string rootFolderLabel = PathToUtf8String(g_ProjectPath.filename());
                if (rootFolderLabel.empty()) rootFolderLabel = "Project";

                bool rootOpen = ImGui::TreeNodeEx(rootFolderLabel.c_str(), rootFlags);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                {
                    g_CurrentAssetPath = g_ProjectPath;
                }

                if (rootOpen)
                {
                    RenderFolderTree(g_ProjectPath);
                    ImGui::TreePop();
                }

                ImGui::EndChild();

                // ---------------------------------------------------------
                // Right Column: Asset List & Grid View (우측: 파일 및 폴더 목록)
                // ---------------------------------------------------------
                ImGui::TableSetColumnIndex(1);
                ImGui::BeginChild("RightAssetListArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);

                // Handle F2 Key Shortcut for Rename (F2 단축키 처리)
                if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2) && !g_SelectedFilePath.empty())
                {
                    fs::path selPath = fs::path(g_SelectedFilePath);
                    std::string currentFilename = PathToUtf8String(selPath.filename());
                    memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
                    strncpy(g_InputBuffer, currentFilename.c_str(), sizeof(g_InputBuffer) - 1);
                    g_ShowRenamePopup = true;
                }

                // -----------------------------------------------------
                // Unity-Style Icon Grid (유니티 스타일 아이콘 그리드)
                // -----------------------------------------------------
                // BUG FIX / FEATURE: the old right panel was a plain text
                // list ("[Dir] name" / "[File] name"). This replaces it
                // with a folders-first, alphabetically sorted icon grid,
                // and also visually marks the file currently open in the
                // Code Editor tab (blue outline) so the two tabs read as
                // one connected workspace instead of two disconnected
                // windows.
                // (기존 우측 패널은 "[Dir] 이름" / "[File] 이름" 형태의
                //  단순 텍스트 목록이었음. 이를 폴더 우선 + 알파벳순으로
                //  정렬된 아이콘 그리드로 교체하고, 현재 코드 에디터
                //  탭에서 열려 있는 파일을 파란 테두리로 표시해서 두 탭이
                //  서로 분리된 창이 아니라 하나로 연결된 작업 공간처럼
                //  보이게 함)
                struct AssetEntry
                {
                    fs::path path;
                    std::string displayName;
                    bool isDirectory;
                };

                std::vector<AssetEntry> entries;
                try {
                    for (const auto& dirEntry : fs::directory_iterator(g_CurrentAssetPath))
                    {
                        AssetEntry ae;
                        ae.path = dirEntry.path();
                        ae.displayName = PathToUtf8String(ae.path.filename());
                        ae.isDirectory = dirEntry.is_directory();
                        entries.push_back(std::move(ae));
                    }

                    // Folders first, then alphabetical within each group (폴더가 먼저, 그다음 각 그룹 안에서 알파벳순)
                    std::sort(entries.begin(), entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
                        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                        return a.displayName < b.displayName;
                        });

                    // Apply the search filter, if any (검색어가 있으면 필터링 적용)
                    if (g_SearchBuffer[0] != '\0')
                    {
                        std::string query = g_SearchBuffer;
                        entries.erase(
                            std::remove_if(entries.begin(), entries.end(), [&query](const AssetEntry& e) {
                                return !MatchesSearch(e.displayName, query);
                                }),
                            entries.end());
                    }
                }
                catch (const std::exception& e) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", e.what());
                }

                // Cell geometry (grid layout constants) (셀 크기 - 그리드 레이아웃 상수)
                // cellHeight leaves room for the icon area PLUS a full 2-line wrapped
                // label below it (label height depends on the active font, so this is
                // sized generously rather than measured exactly - a couple of spare
                // pixels here is cheaper than clipped text).
                // (cellHeight는 아이콘 영역 + 그 아래 최대 2줄로 줄바꿈된 라벨이 들어갈
                //  공간까지 넉넉히 잡음 - 라벨 높이는 활성 폰트에 따라 달라지므로
                //  정확히 계산하기보다 여유 있게 잡음. 텍스트가 잘리는 것보단 여백이
                //  몇 픽셀 남는 게 나음)
                const float iconScale = GetAssetIconScale();
                const float cellWidth = 88.0f * iconScale;
                const float iconAreaHeight = 52.0f * iconScale;
                const float iconSize = 40.0f * iconScale;
                const float cellHeight = iconAreaHeight + ImGui::GetTextLineHeight() * 2.0f + 8.0f;

                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float availWidth = ImGui::GetContentRegionAvail().x;
                const int columnCount = std::max(1, static_cast<int>((availWidth + spacing) / (cellWidth + spacing)));

                ImDrawList* drawList = ImGui::GetWindowDrawList();

                for (size_t i = 0; i < entries.size(); ++i)
                {
                    const AssetEntry& item = entries[i];
                    const std::string pathStr = PathToUtf8String(item.path);

                    if (i % static_cast<size_t>(columnCount) != 0)
                    {
                        ImGui::SameLine();
                    }

                    ImGui::PushID(static_cast<int>(i));

                    ImVec2 cellPos = ImGui::GetCursorScreenPos();
                    bool clicked = ImGui::InvisibleButton("##assetCell", ImVec2(cellWidth, cellHeight));
                    bool hovered = ImGui::IsItemHovered();
                    bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                    bool isSelected = (g_SelectedFilePath == pathStr);
                    // A file counts as "open" only while its actual Code Editor tab is visible - this
                    // stays in sync automatically if the user closes that tab. (코드 에디터 탭이 실제로
                    // 열려있을 때만 "열려있음"으로 취급함 - 사용자가 탭을 닫으면 자동으로 표시도 사라짐)
                    bool isOpenInEditor = (!item.isDirectory && render_CodeEditor && g_OpenedFilePath == pathStr);

                    ImVec2 cellMax(cellPos.x + cellWidth, cellPos.y + cellHeight);

                    if (isOpenInEditor)
                    {
                        drawList->AddRectFilled(cellPos, cellMax, IM_COL32(70, 110, 150, 90), 4.0f);
                        drawList->AddRect(cellPos, cellMax, IM_COL32(100, 150, 210, 255), 4.0f, 0, 1.5f);
                    }
                    else if (isSelected)
                    {
                        drawList->AddRectFilled(cellPos, cellMax, IM_COL32(90, 90, 90, 130), 4.0f);
                    }
                    else if (hovered)
                    {
                        drawList->AddRectFilled(cellPos, cellMax, IM_COL32(255, 255, 255, 18), 4.0f);
                    }

                    AssetIconKind iconKind = ClassifyAssetIcon(item.isDirectory, item.path);
                    ImVec2 iconCenter(cellPos.x + cellWidth * 0.5f, cellPos.y + iconAreaHeight * 0.5f + 2.0f);
                    DrawAssetIcon(drawList, iconKind, iconCenter, iconSize);

                    // Filename label, wrapped/truncated to 2 lines and centered under the icon (파일명 라벨 - 최대 2줄로 줄바꿈/생략되어 아이콘 아래 가운데 정렬됨)
                    float labelWrapWidth = cellWidth - 6.0f;
                    std::string label = TrimLabelToLines(item.displayName, labelWrapWidth, 2);
                    ImVec2 labelSize = ImGui::CalcTextSize(label.c_str(), nullptr, false, labelWrapWidth);
                    ImVec2 labelPos(cellPos.x + (cellWidth - labelSize.x) * 0.5f, cellPos.y + iconAreaHeight + 2.0f);
                    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), labelPos,
                        ImGui::GetColorU32(ImGuiCol_Text), label.c_str(), nullptr, labelWrapWidth);

                    if (clicked)
                    {
                        g_SelectedFilePath = pathStr;
                    }

                    // Right-click Context Menu on Item (아이템 개별 우클릭 메뉴)
                    if (ImGui::BeginPopupContextItem())
                    {
                        g_SelectedFilePath = pathStr;

                        if (ImGui::MenuItem("Open (열기)"))
                        {
                            if (item.isDirectory) {
                                g_CurrentAssetPath = item.path;
                            }
                            else if (IsScriptOrTextFile(item.path)) {
                                OpenInCodeEditor(pathStr);
                            }
                            else {
                                OpenWithSystemDefaultApp(item.path);
                            }
                        }

                        if (ImGui::MenuItem("Rename (이름 바꾸기 - F2)"))
                        {
                            memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
                            strncpy(g_InputBuffer, item.displayName.c_str(), sizeof(g_InputBuffer) - 1);
                            g_ShowRenamePopup = true;
                        }

                        if (ImGui::MenuItem("Delete (삭제)"))
                        {
                            std::error_code ec;
                            fs::remove_all(item.path, ec);
                            if (!ec) {
                                g_SelectedFilePath = "";
                            }
                        }

                        ImGui::EndPopup();
                    }

                    // Double-Click Action (더블클릭 동작)
                    if (doubleClicked)
                    {
                        if (item.isDirectory)
                        {
                            g_CurrentAssetPath = item.path;
                        }
                        else if (IsScriptOrTextFile(item.path))
                        {
                            // Open in Code Editor with Selected File (선택한 파일을 코드 에디터로 연동)
                            OpenInCodeEditor(pathStr);
                        }
                        else
                        {
                            // Open with System Default Application (이미지 및 기타 파일은 시스템 연결 프로그램으로 열기)
                            OpenWithSystemDefaultApp(item.path);
                        }
                    }

                    ImGui::PopID();
                }

                // Background Context Menu for Empty Space (빈 공간 우클릭 메뉴)
                if (ImGui::BeginPopupContextWindow("AssetAreaContextMenu", ImGuiPopupFlags_NoOpenOverExistingPopup | ImGuiPopupFlags_MouseButtonRight))
                {
                    if (ImGui::MenuItem("New Folder (새 폴더 생성)"))
                    {
                        memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
                        strcpy(g_InputBuffer, "NewFolder");
                        g_ShowNewFolderPopup = true;
                    }

                    if (ImGui::MenuItem("Create NutComponent Script (NutComponent 스크립트 생성)"))
                    {
                        memset(g_InputBuffer, 0, sizeof(g_InputBuffer));
                        strcpy(g_InputBuffer, "NewScript.nut");
                        g_ShowNewScriptPopup = true;
                    }

                    ImGui::EndPopup();
                }

                ImGui::EndChild();

                ImGui::EndTable();
            }

            // -------------------------------------------------------------
            // 3. Modals & Popups for Asset Actions (폴더/스크립트 생성 및 이름 바꾸기 팝업)
            // -------------------------------------------------------------
            if (g_ShowRenamePopup) ImGui::OpenPopup("Rename File/Folder");
            if (g_ShowNewFolderPopup) ImGui::OpenPopup("Create New Folder");
            if (g_ShowNewScriptPopup) ImGui::OpenPopup("Create NutComponent Script");

            // Rename Modal (이름 변경 팝업 창 - 예외 예방 및 abort 크래시 차단 적용)
            if (ImGui::BeginPopupModal("Rename File/Folder", &g_ShowRenamePopup, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Enter new name (새 이름을 입력하세요):");
                ImGui::InputText("##NewName", g_InputBuffer, IM_ARRAYSIZE(g_InputBuffer));

                if (ImGui::Button("OK (확인)", ImVec2(120, 0)))
                {
                    if (strlen(g_InputBuffer) > 0 && !g_SelectedFilePath.empty())
                    {
                        fs::path oldPath = fs::path(g_SelectedFilePath);
                        fs::path newPath = oldPath.parent_path() / g_InputBuffer;

                        std::error_code ec;
                        fs::rename(oldPath, newPath, ec);

                        if (!ec)
                        {
                            g_SelectedFilePath = PathToUtf8String(newPath);

                            // 현재 코드 에디터에 열려있는 파일이면 에디터 경로도 함께 업데이트
                            if (g_OpenedFilePath == PathToUtf8String(oldPath)) {
                                g_OpenedFilePath = PathToUtf8String(newPath);
                            }
                        }
                        else
                        {
                            ConsoleLog::Append(LogLevel::Error, "Rename failed: " + ec.message());
                        }
                    }
                    g_ShowRenamePopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel (취소)", ImVec2(120, 0)))
                {
                    g_ShowRenamePopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // New Folder Modal (새 폴더 생성 팝업 창)
            if (ImGui::BeginPopupModal("Create New Folder", &g_ShowNewFolderPopup, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Folder Name (폴더 이름):");
                ImGui::InputText("##FolderName", g_InputBuffer, IM_ARRAYSIZE(g_InputBuffer));

                if (ImGui::Button("Create (생성)", ImVec2(120, 0)))
                {
                    if (strlen(g_InputBuffer) > 0)
                    {
                        fs::path newFolderPath = g_CurrentAssetPath / g_InputBuffer;
                        std::error_code ec;
                        fs::create_directories(newFolderPath, ec);
                    }
                    g_ShowNewFolderPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel (취소)", ImVec2(120, 0)))
                {
                    g_ShowNewFolderPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // New NutComponent Script Modal (NutComponent 스크립트 생성 팝업 창)
            if (ImGui::BeginPopupModal("Create NutComponent Script", &g_ShowNewScriptPopup, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Script Name (스크립트 이름):");
                ImGui::InputText("##ScriptName", g_InputBuffer, IM_ARRAYSIZE(g_InputBuffer));

                if (ImGui::Button("Create (생성)", ImVec2(120, 0)))
                {
                    if (strlen(g_InputBuffer) > 0)
                    {
                        fs::path newScriptPath;
                        CreateScriptFileAt(g_CurrentAssetPath, g_InputBuffer, newScriptPath);
                    }
                    g_ShowNewScriptPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel (취소)", ImVec2(120, 0)))
                {
                    g_ShowNewScriptPopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // -------------------------------------------------------------
            // 4. Bottom Status Bar (하단 선택 상태 표시줄)
            // -------------------------------------------------------------
            ImGui::Separator();
            if (!g_SelectedFilePath.empty())
            {
                ImGui::Text("%s: %s", L::Get("Selected").c_str(), g_SelectedFilePath.c_str());
            }
            else
            {
                ImGui::Text("%s", L::Get("NoFileSelected").c_str());
            }

            // Icon size slider, right-aligned on the same line as the status
            // text. Range 0.6x-2.0x keeps the smallest size still readable
            // and the largest still reasonable on a 1080p-ish screen.
            // Persists to Settings.json only once the user releases the
            // slider (IsItemDeactivatedAfterEdit), not on every in-between
            // frame while dragging - same reasoning as everywhere else Save()
            // is called explicitly rather than on every Set(). (아이콘 크기
            // 슬라이더, 상태 텍스트와 같은 줄 오른쪽 정렬. 0.6x~2.0x 범위는
            // 가장 작을 때도 읽을 수 있고 가장 클 때도 1080p 화면 기준으로
            // 과하지 않도록 잡음. 슬라이더를 놓았을 때만
            // (IsItemDeactivatedAfterEdit) Settings.json에 저장함 - 드래그
            // 도중 매 프레임 저장하지 않는 건 Save()를 다른 곳에서도 명시적으로
            //만 호출하는 것과 같은 이유)
            {
                float scale = GetAssetIconScale();
                const float sliderWidth = 140.0f;
                float availAfterText = ImGui::GetContentRegionAvail().x;
                if (availAfterText > sliderWidth + 20.0f)
                {
                    ImGui::SameLine(0.0f, availAfterText - sliderWidth);
                }
                else
                {
                    // Not enough room on this line (narrow window) - drop to
                    // its own line instead of overlapping the status text
                    // (창이 좁아서 같은 줄에 공간이 부족하면 상태 텍스트와
                    // 겹치지 않도록 다음 줄로 내림)
                }
                ImGui::SetNextItemWidth(sliderWidth);
                if (ImGui::SliderFloat("##AssetIconScale", &scale, 0.6f, 2.0f, "%.1fx"))
                {
                    g_AssetIconScale = scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    Settings::SetFloat(kIconScaleKey, g_AssetIconScale);
                    Settings::Save();
                }
            }
        }
        ImGui::End();
    }
}