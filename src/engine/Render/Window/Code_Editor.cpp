#include "Window.h"
#include <ImGuiColorTextEdit/TextEditor.h>
#include "ConsoleLog.h"
#include "Squirrel_Binding.h"
#include "Script/ScriptPreprocessor.h"
#include "Language/Language.h"
#include "FileSystem.h"
#include "Settings.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace Window {

    bool render_CodeEditor = false;
    TextEditor editor;
    bool isEditorInitialized = false;

    static float g_EditorFontSize = 1.0f;
    static bool g_ShowSettingsWindow = false;
    static bool g_EnableCodeFolding = true;

    // Provided by Assets.cpp - used by the Start Page's "New Script" button (Assets.cpp에서 제공 - 시작 화면의 "새 스크립트" 버튼용)
    extern std::string CreateNewScriptAtProjectRoot();

    HSQUIRRELVM g_ScriptVM = nullptr;

    // ★ 현재 에디터에 열려있는 파일의 전체 경로를 저장하는 전역 변수
    std::string g_OpenedFilePath = "";

    // Whether the editor's text currently differs from what's saved on disk
    // for g_OpenedFilePath (에디터의 텍스트가 g_OpenedFilePath의 디스크
    // 저장 내용과 현재 다른 상태인지 여부)
    static bool g_IsDirty = false;

    // Set by OpenInCodeEditor() to ask Render_CodeEditor() to pull the docked
    // tab to the front on its next Begin() this frame (Assets 탭에서 파일을
    // 열 때 OpenInCodeEditor()가 세팅함 - Render_CodeEditor()가 이번 프레임의
    // 다음 Begin() 호출에서 도킹된 탭을 앞으로 가져오도록 요청함)
    static bool g_RequestFocus = false;

    // "Unsaved changes" confirmation popup state, used when OpenInCodeEditor()
    // is asked to switch to a different file while g_IsDirty is true
    // ("저장되지 않은 변경사항" 확인 팝업 상태 - g_IsDirty가 true인 상태에서
    // OpenInCodeEditor()가 다른 파일로 전환하라는 요청을 받았을 때 사용됨)
    static bool g_ShowUnsavedChangesPopup = false;
    static std::string g_PendingOpenFilePath = "";

    // -----------------------------------------------------------------
    // Start Page / Recent Files (시작 화면 / 최근 파일 목록)
    // -----------------------------------------------------------------
    // Shown instead of a blank text area whenever no file is open yet
    // (right after the Code Editor tab is first opened, or after closing
    // the last file). Recent files persist across sessions via
    // Settings::Get/Set + Save() - the Settings.json line format can't
    // hold a real array, so the list is stored as one line of paths
    // joined by '|' (a character that can't appear in a Windows path).
    // (아직 파일이 열려있지 않을 때(코드 에디터 탭을 처음 열었을 때, 또는
    // 마지막 파일을 닫았을 때) 빈 텍스트 영역 대신 표시됨. 최근 파일
    // 목록은 Settings::Get/Set + Save()로 세션 사이에도 유지됨 -
    // Settings.json의 줄 기반 포맷은 배열을 담을 수 없어서, 목록을
    // '|'(윈도우 경로에 나올 수 없는 문자)로 이어붙인 한 줄로 저장함)
    static const char* kRecentFilesKey = "CodeEditor_RecentFiles";
    static const size_t kMaxRecentFiles = 8;
    static std::vector<std::string> g_RecentFiles;
    static bool g_RecentFilesLoaded = false;

    // A generous cap on what the editor will load as text. Without this,
    // double-clicking a large/binary file that happens to match one of
    // IsScriptOrTextFile()'s extensions (a stray ".txt" build log, for
    // example) would try to pull the whole thing into TextEditor's line
    // buffer in one shot - exactly the kind of thing that produces an
    // abort() on an 8GB machine instead of a clean error message.
    // (에디터가 텍스트로 불러올 파일 크기의 상한선. 이게 없으면
    // IsScriptOrTextFile()의 확장자 중 하나와 우연히 일치하는 크고/바이너리인
    // 파일(예: 우연히 ".txt"인 빌드 로그)을 더블클릭했을 때 TextEditor의
    // 라인 버퍼에 통째로 밀어넣으려다가, 깔끔한 에러 메시지 대신 8GB
    // 환경에서 딱 abort()가 나는 상황이 벌어짐)
    static const size_t kMaxEditableFileBytes = 4 * 1024 * 1024; // 4MB

    static void LoadRecentFilesOnce()
    {
        if (g_RecentFilesLoaded) return;
        g_RecentFilesLoaded = true;

        std::string joined = Settings::Get(kRecentFilesKey);
        if (joined.empty() || joined == kRecentFilesKey) return; // Settings::Get() echoes the key back when missing (키가 없으면 Get()이 키 자체를 그대로 돌려줌)

        std::stringstream ss(joined);
        std::string item;
        while (std::getline(ss, item, '|'))
        {
            if (!item.empty()) g_RecentFiles.push_back(item);
        }
    }

    static void RememberRecentFile(const std::string& filePath)
    {
        LoadRecentFilesOnce();

        // Move to front if it's already in the list, dedup (이미 목록에 있으면 맨 앞으로 이동, 중복 제거)
        g_RecentFiles.erase(std::remove(g_RecentFiles.begin(), g_RecentFiles.end(), filePath), g_RecentFiles.end());
        g_RecentFiles.insert(g_RecentFiles.begin(), filePath);
        if (g_RecentFiles.size() > kMaxRecentFiles) g_RecentFiles.resize(kMaxRecentFiles);

        std::string joined;
        for (size_t i = 0; i < g_RecentFiles.size(); ++i)
        {
            if (i > 0) joined += "|";
            joined += g_RecentFiles[i];
        }
        Settings::Set(kRecentFilesKey, joined);
        Settings::Save();
    }

    // UTF-8 Path Conversion Helper (ImGui 한글 깨짐 방지를 위한 UTF-8 경로 변환 헬퍼 함수)
    // Duplicated locally rather than shared from Assets.cpp, matching this
    // project's "one header + one source per feature" convention (다른 파일과
    // 공유하지 않고 이 파일 안에 따로 둠 - '기능 하나당 헤더 하나 + 소스 하나'
    // 컨벤션에 맞춤)
    static std::string PathToUtf8String(const fs::path& path)
    {
        auto u8str = path.u8string();
        return std::string(reinterpret_cast<const char*>(u8str.data()), u8str.size());
    }

    struct CompileErrorInfo {
        int line = -1;
        std::string message;
        uint64_t errorTime = 0;
    };
    static CompileErrorInfo g_LastError;
    static const uint64_t ERROR_DISPLAY_DURATION = 5000;

    void SquirrelCompilerErrorHandler(HSQUIRRELVM v, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column)
    {
        g_LastError.line = static_cast<int>(line);
        g_LastError.message = desc ? std::string(desc) : L::Get("UnknownSyntaxError");
        g_LastError.errorTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        ConsoleLog::Append(LogLevel::Error,
            L::Get("ScriptErrorPrefix") + std::to_string(line) + ", Col " + std::to_string(column) + ": " + g_LastError.message);
    }

    void AdjustErrorLine(CompileErrorInfo& error) {
        if (error.line > 1 && (error.message.find("expected 'IDENTIFIER'") != std::string::npos ||
            error.message.find("expected ')'") != std::string::npos ||
            error.message.find("expected ';'") != std::string::npos))
        {
            error.line -= 1;
        }
    }

    void InitCodeEditor()
    {
        Settings::Load();

        auto lang = TextEditor::LanguageDefinition::CPlusPlus();

        static const char* geekpizKeywords[] = {
            "start", "update", "getkey", "cupdate", "print"
        };
        for (auto& kw : geekpizKeywords) {
            lang.mKeywords.insert(kw);
        }

        static const char* squirrelKeywords[] = {
            "function", "local", "base", "resume", "yield", "clone", "constructor", "this", "null"
        };
        for (auto& kw : squirrelKeywords) {
            lang.mKeywords.insert(kw);
        }

        editor.SetLanguageDefinition(lang);
        editor.SetFoldingEnabled(g_EnableCodeFolding);

        auto palette = TextEditor::GetDarkPalette();
        auto GetSwappedColor = [](const std::string& key, unsigned int defaultArgb) -> unsigned int {
            unsigned int color = Settings::GetColor(key, defaultArgb);
            return (color & 0xFF00FF00) | ((color & 0x00FF0000) >> 16) | ((color & 0x000000FF) << 16);
            };

        palette[(int)TextEditor::PaletteIndex::ErrorMarker] = GetSwappedColor("Editor_ErrorMarker", 0x80FF0000);
        palette[(int)TextEditor::PaletteIndex::Keyword] = GetSwappedColor("Editor_Keyword", 0xFFD7BA56);
        palette[(int)TextEditor::PaletteIndex::Number] = GetSwappedColor("Editor_Number", 0xFFB5CEA8);
        palette[(int)TextEditor::PaletteIndex::String] = GetSwappedColor("Editor_String", 0xFFFF7700);
        palette[(int)TextEditor::PaletteIndex::Comment] = GetSwappedColor("Editor_Comment", 0xFF6A9955);
        palette[(int)TextEditor::PaletteIndex::Punctuation] = GetSwappedColor("Editor_Punctuation", 0xFFD4D4D4);
        palette[(int)TextEditor::PaletteIndex::Preprocessor] = GetSwappedColor("Editor_Preprocessor", 0xFFC586C0);
        palette[(int)TextEditor::PaletteIndex::Identifier] = GetSwappedColor("Editor_Identifier", 0xFF9CDCFE);
        palette[(int)TextEditor::PaletteIndex::KnownIdentifier] = GetSwappedColor("Editor_KnownIdentifier", 0xFF4EC9B0);

        editor.SetPalette(palette);
        editor.SetTabSize(4);
        editor.SetShowWhitespaces(false);

        isEditorInitialized = true;
    }

    // Actually loads `filePath`'s content into the editor and resets all
    // per-file state (dirty flag, error markers). Split out of
    // OpenInCodeEditor() so both the "no unsaved changes" fast path and the
    // "Save & Open"/"Discard & Open" confirmation buttons can share it.
    // (실제로 `filePath`의 내용을 에디터에 불러오고 파일 단위 상태(더티
    // 플래그, 오류 마커)를 초기화함. OpenInCodeEditor()에서 분리해서
    // "저장 안 된 변경사항 없음" 빠른 경로와 확인 팝업의 "저장 후 열기"/
    // "저장 안 하고 열기" 버튼이 함께 재사용할 수 있게 함)
    static void LoadFileIntoEditor(const std::string& filePath)
    {
        // Empty path means "close the current file, go back to the Start
        // Page" (reused by the File > Close menu item's unsaved-changes
        // confirmation flow) - not an actual file to read/remember.
        // (빈 경로는 "현재 파일을 닫고 시작 화면으로 돌아감"을 의미함
        // (File > 닫기 메뉴의 저장 확인 흐름에서 재사용됨) - 실제로 읽거나
        // 기억할 파일이 아님)
        if (filePath.empty())
        {
            g_OpenedFilePath = "";
            editor.SetText("");
            editor.SetErrorMarkers(TextEditor::ErrorMarkers());
            g_LastError = CompileErrorInfo();
            g_IsDirty = false;
            return;
        }

        // Defensive size check BEFORE reading (see kMaxEditableFileBytes
        // above for why) - std::filesystem::file_size() is a cheap stat
        // call, no need to read the whole file just to measure it.
        // (읽기 전에 크기부터 확인함(위 kMaxEditableFileBytes 설명 참고) -
        // std::filesystem::file_size()는 가벼운 stat 호출이라 크기만
        // 재려고 파일 전체를 읽을 필요가 없음)
        std::error_code sizeEc;
        uintmax_t fileSize = fs::file_size(fs::path(filePath), sizeEc);
        if (!sizeEc && fileSize > kMaxEditableFileBytes)
        {
            ConsoleLog::Append(LogLevel::Warning,
                "File too large to open in the Code Editor / 코드 에디터로 열기엔 너무 큰 파일입니다 (" +
                std::to_string(fileSize / (1024 * 1024)) + "MB): " + filePath);
            return; // Leave the editor showing whatever it showed before (편집기는 이전 상태 그대로 둠)
        }

        g_OpenedFilePath = filePath;

        std::string content = FS::ReadFile(filePath);
        editor.SetText(content);
        editor.SetErrorMarkers(TextEditor::ErrorMarkers());
        g_LastError = CompileErrorInfo();
        g_IsDirty = false;

        RememberRecentFile(filePath);
    }

    // Writes the editor's current text back to g_OpenedFilePath. Shared by
    // the File > Save menu item, the real Ctrl+S shortcut, and the "Save &
    // Open" confirmation button (에디터의 현재 텍스트를 g_OpenedFilePath에
    // 저장함. File > Save 메뉴, 실제로 동작하는 Ctrl+S 단축키, 확인 팝업의
    // "저장 후 열기" 버튼이 함께 사용함)
    static bool SaveCurrentFile()
    {
        if (g_OpenedFilePath.empty()) return false;

        if (FS::WriteFile(g_OpenedFilePath, editor.GetText()))
        {
            ConsoleLog::Append(LogLevel::Info, L::Get("SaveSuccess"));
            g_IsDirty = false;
            return true;
        }

        ConsoleLog::Append(LogLevel::Error, L::Get("SaveFailed"));
        return false;
    }

    // ★ 외부(Assets.cpp)에서 파일을 더블클릭하면 호출될 새 함수!
    void OpenInCodeEditor(const std::string& filePath)
    {
        if (!isEditorInitialized) {
            InitCodeEditor();
        }

        render_CodeEditor = true;
        g_RequestFocus = true; // Bring the Code Editor tab to front this frame, even if it's already open behind another tab (다른 탭 뒤에 이미 열려있더라도 이번 프레임에 코드 에디터 탭을 앞으로 가져옴)

        // Re-opening the file that's already showing just re-focuses the tab -
        // no need to reload it from disk or ask about unsaved changes.
        // (이미 보여지고 있는 같은 파일을 다시 여는 경우엔 탭만 다시
        //  포커스하면 됨 - 디스크에서 다시 불러오거나 저장 여부를 물어볼
        //  필요 없음)
        if (filePath == g_OpenedFilePath) {
            return;
        }

        // BUG FIX: this function used to overwrite the editor's text
        // unconditionally, so double-clicking a different file in the Assets
        // tab silently threw away any unsaved edits with zero warning. Now it
        // asks first whenever there's unsaved work, via the confirmation
        // popup rendered in Render_CodeEditor().
        // (이 함수는 원래 에디터 텍스트를 무조건 덮어써서, Assets 탭에서
        //  다른 파일을 더블클릭하면 경고 하나 없이 저장 안 된 수정 내용이
        //  그대로 사라졌음. 이제는 저장 안 된 작업이 있으면 Render_CodeEditor()
        //  에서 그려주는 확인 팝업을 통해 먼저 물어봄)
        if (g_IsDirty) {
            g_PendingOpenFilePath = filePath;
            g_ShowUnsavedChangesPopup = true;
            return;
        }

        LoadFileIntoEditor(filePath);
    }

    void CheckSyntaxFull()
    {
        // ★ .nut 파일이 아니면 오류 검사를 아예 생략함
        if (g_OpenedFilePath.length() < 4 || g_OpenedFilePath.substr(g_OpenedFilePath.length() - 4) != ".nut") {
            return;
        }

        editor.SetErrorMarkers(TextEditor::ErrorMarkers());

        if (g_LastError.line > 0)
        {
            uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (currentTime - g_LastError.errorTime > ERROR_DISPLAY_DURATION)
            {
                g_LastError = CompileErrorInfo();
            }
        }

        if (!g_ScriptVM) {
            g_ScriptVM = InitGeekpizSquirrelVM();
        }

        sq_setcompilererrorhandler(g_ScriptVM, SquirrelCompilerErrorHandler);
        std::string rawScript = editor.GetText();
        Geekpiz::ScriptLifecycleInfo lifecycleInfo;
        std::string processedCode = Geekpiz::PreprocessLifecycleAttributes(rawScript, lifecycleInfo);

        if (SQ_FAILED(sq_compilebuffer(g_ScriptVM, processedCode.c_str(), processedCode.length(), _SC("EditorScriptBuffer"), SQTrue)))
        {
            if (g_LastError.line > 0) {
                AdjustErrorLine(g_LastError);
                TextEditor::ErrorMarkers markers;
                markers.insert({ g_LastError.line, g_LastError.message });
                editor.SetErrorMarkers(markers);
            }
        }
        else {
            sq_pop(g_ScriptVM, 1);
            g_LastError = CompileErrorInfo();
        }
    }

    void ShutdownCodeEditor()
    {
        if (g_ScriptVM) {
            DestroyGeekpizSquirrelVM(g_ScriptVM);
            g_ScriptVM = nullptr;
        }
    }

    void CompileAndRunScript()
    {
        // ★ .nut 파일일 때만 실행 가능하도록 방어 코드 추가
        if (g_OpenedFilePath.length() < 4 || g_OpenedFilePath.substr(g_OpenedFilePath.length() - 4) != ".nut") {
            ConsoleLog::Append(LogLevel::Warning, "이 파일은 스크립트(.nut)가 아닙니다.");
            return;
        }

        editor.SetErrorMarkers(TextEditor::ErrorMarkers());
        g_LastError = CompileErrorInfo();

        if (!g_ScriptVM) {
            g_ScriptVM = InitGeekpizSquirrelVM();
        }

        sq_setcompilererrorhandler(g_ScriptVM, SquirrelCompilerErrorHandler);
        std::string rawScript = editor.GetText();
        Geekpiz::ScriptLifecycleInfo lifecycleInfo;
        std::string processedCode = Geekpiz::PreprocessLifecycleAttributes(rawScript, lifecycleInfo);

        if (SQ_SUCCEEDED(sq_compilebuffer(g_ScriptVM, processedCode.c_str(), processedCode.length(), _SC("EditorScriptBuffer"), SQTrue)))
        {
            sq_pushroottable(g_ScriptVM);
            if (SQ_SUCCEEDED(sq_call(g_ScriptVM, 1, SQFalse, SQTrue))) {
                ConsoleLog::Append(LogLevel::Info, L::Get("CompileSuccess"));
            }
            else {
                ConsoleLog::Append(LogLevel::Error, L::Get("RuntimeError"));
            }
            sq_pop(g_ScriptVM, 1);
        }
        else
        {
            if (g_LastError.line > 0) {
                AdjustErrorLine(g_LastError);
                TextEditor::ErrorMarkers markers;
                markers.insert({ g_LastError.line, g_LastError.message });
                editor.SetErrorMarkers(markers);
            }
        }
    }

    // Start Page shown in place of the text area whenever no file is open
    // yet - the very first thing you see on opening the Code Editor tab,
    // and again after closing the last open file. (아직 열린 파일이 없을
    // 때 텍스트 영역 대신 보여지는 시작 화면 - 코드 에디터 탭을 처음 열
    // 때, 그리고 마지막으로 열려있던 파일을 닫은 뒤에도 다시 표시됨)
    static void RenderStartPage()
    {
        LoadRecentFilesOnce();

        ImGui::Dummy(ImVec2(0.0f, 24.0f));

        ImGui::Indent(24.0f);
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", L::Get("CodeEditor").c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("Geekpiz NutComponent Script Editor");

        ImGui::Dummy(ImVec2(0.0f, 16.0f));

        if (ImGui::Button(L::Get("NewScript").c_str(), ImVec2(220, 40)))
        {
            std::string newPath = CreateNewScriptAtProjectRoot();
            if (!newPath.empty())
            {
                OpenInCodeEditor(newPath);
            }
            else
            {
                ConsoleLog::Append(LogLevel::Error, "Failed to create new script (새 스크립트 생성 실패)");
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 24.0f));
        ImGui::TextDisabled("%s", L::Get("RecentFiles").c_str());
        ImGui::Separator();

        if (g_RecentFiles.empty())
        {
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::TextDisabled("(No recent files yet / 아직 최근 파일이 없습니다)");
        }
        else
        {
            for (size_t i = 0; i < g_RecentFiles.size(); ++i)
            {
                const std::string& path = g_RecentFiles[i];
                std::string label = PathToUtf8String(fs::path(path).filename());
                if (label.empty()) continue;

                ImGui::PushID(static_cast<int>(i));
                bool exists = fs::exists(path);
                if (!exists) ImGui::BeginDisabled();

                if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, 24)))
                {
                    OpenInCodeEditor(path);
                }
                if (!exists) ImGui::EndDisabled();

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", path.c_str());
                }
                ImGui::PopID();
            }
        }

        ImGui::Unindent(24.0f);
    }

    void Render_CodeEditor()
    {
        if (!render_CodeEditor) return;

        if (!isEditorInitialized) {
            InitCodeEditor();
        }

        // Window title shows the open file name and a "*" while there are
        // unsaved changes - this is what visually ties the Code Editor tab
        // back to whatever's selected in the Assets tab. The "###CodeEditor"
        // suffix keeps the docking/window ID fixed even though the visible
        // label text changes every time the file or dirty state changes
        // (only the text BEFORE "###" is shown; ImGui derives the window's
        // identity from the text AFTER it).
        // (창 제목에 열려있는 파일 이름과, 저장 안 된 변경사항이 있을 땐 "*"를
        // 표시함 - 이게 코드 에디터 탭과 Assets 탭에서 선택된 항목을 시각적으로
        // 이어주는 부분임. "###CodeEditor" 접미사는 파일이나 더티 상태가
        // 바뀔 때마다 라벨 텍스트가 바뀌어도 도킹/창 ID는 고정되게 해줌
        // ("###" 앞 텍스트만 화면에 보이고, ImGui는 창의 정체성을 "###" 뒤
        // 텍스트로 판단함))
        std::string windowTitle = L::Get("CodeEditor");
        if (!g_OpenedFilePath.empty())
        {
            windowTitle += " - " + PathToUtf8String(fs::path(g_OpenedFilePath).filename());
        }
        if (g_IsDirty)
        {
            windowTitle += " *";
        }
        windowTitle += "###CodeEditor";

        if (ImGui::Begin(windowTitle.c_str(), &render_CodeEditor, ImGuiWindowFlags_MenuBar))
        {
            // BUG FIX / FEATURE: bring this tab to the front when Assets.cpp's
            // OpenInCodeEditor() requests it - previously opening a file while
            // the Code Editor was docked behind another tab (e.g. Console)
            // left it hidden, so double-clicking a file appeared to do nothing.
            // (BUG FIX / 기능 추가: Assets.cpp의 OpenInCodeEditor()가 요청하면
            //  이 탭을 앞으로 가져옴 - 이전에는 코드 에디터가 다른 탭(예: 콘솔)
            //  뒤에 도킹되어 있는 상태로 파일을 열면 숨겨진 채로 있어서 파일을
            //  더블클릭해도 아무 반응이 없는 것처럼 보였음)
            if (g_RequestFocus)
            {
                ImGui::SetWindowFocus();
                g_RequestFocus = false;
            }

            // BUG FIX: File > Save and Script > Compile & Test showed "Ctrl+S"
            // and "F5" as shortcut labels, but Dear ImGui's MenuItem shortcut
            // text is purely cosmetic - neither key actually did anything.
            // This adds the real key handling for both.
            // (BUG FIX: File > Save와 Script > Compile & Test 메뉴가 "Ctrl+S"와
            //  "F5"를 단축키 텍스트로 보여주긴 했지만, Dear ImGui의 MenuItem
            //  단축키 텍스트는 순전히 표시용이라 실제로는 두 키 다 아무 동작도
            //  하지 않았음. 이제 둘 다 실제로 동작하는 키 입력 처리를 추가함)
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            {
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
                {
                    SaveCurrentFile();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
                {
                    CompileAndRunScript();
                }
            }

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu(L::Get("File").c_str()))
                {
                    if (ImGui::MenuItem(L::Get("Save").c_str(), "Ctrl+S", false, !g_OpenedFilePath.empty()))
                    {
                        SaveCurrentFile();
                    }
                    // Back to the Start Page, without discarding unsaved
                    // work silently - reuses the same confirmation popup
                    // as switching files (저장 안 된 내용을 조용히 버리지
                    // 않고 시작 화면으로 돌아감 - 다른 파일로 전환할 때와
                    // 같은 확인 팝업을 재사용함)
                    if (ImGui::MenuItem(L::Get("CloseFile").c_str(), nullptr, false, !g_OpenedFilePath.empty()))
                    {
                        if (g_IsDirty)
                        {
                            g_PendingOpenFilePath = ""; // Empty target = "close, don't load anything" (빈 대상 = "닫고 아무것도 안 엶")
                            g_ShowUnsavedChangesPopup = true;
                        }
                        else
                        {
                            g_OpenedFilePath = "";
                            editor.SetText("");
                        }
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(L::Get("Script").c_str()))
                {
                    if (ImGui::MenuItem(L::Get("CompileAndTest").c_str(), "F5"))
                    {
                        CompileAndRunScript();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu(L::Get("Setting").c_str()))
                {
                    if (ImGui::MenuItem(L::Get("EditorSettings").c_str()))
                    {
                        g_ShowSettingsWindow = true;
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenuBar();
            }

            if (g_OpenedFilePath.empty())
            {
                // No file open yet - show the Start Page instead of a
                // blank text area (아직 열린 파일이 없음 - 빈 텍스트
                // 영역 대신 시작 화면을 보여줌)
                RenderStartPage();
            }
            else
            {
                ImGui::SetWindowFontScale(g_EditorFontSize);

                editor.Render("TextEditor");

                if (editor.IsTextChanged())
                {
                    g_IsDirty = true; // Text no longer matches what's on disk (텍스트가 더 이상 디스크의 내용과 일치하지 않음)
                    CheckSyntaxFull(); // 이 안에서 nut 파일인지 검사함
                }
            }
        }
        ImGui::End();

        // -----------------------------------------------------------------
        // "Unsaved Changes" Confirmation Popup (저장되지 않은 변경사항 확인 팝업)
        // -----------------------------------------------------------------
        // Shown when OpenInCodeEditor() (called from Assets.cpp on a double
        // click) is asked to switch away from a file with unsaved edits.
        // (Assets.cpp에서 더블클릭 시 호출되는 OpenInCodeEditor()가 저장 안
        //  된 수정사항이 있는 파일에서 다른 파일로 전환하라는 요청을 받았을
        //  때 표시됨)
        if (g_ShowUnsavedChangesPopup)
        {
            ImGui::OpenPopup("Unsaved Changes (저장되지 않은 변경사항)");
        }
        if (ImGui::BeginPopupModal("Unsaved Changes (저장되지 않은 변경사항)", &g_ShowUnsavedChangesPopup, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Save changes before opening a different file?");
            ImGui::Text("(다른 파일을 열기 전에 현재 변경사항을 저장할까요?)");
            ImGui::Spacing();

            if (ImGui::Button("Save & Open (저장 후 열기)", ImVec2(170, 0)))
            {
                SaveCurrentFile();
                LoadFileIntoEditor(g_PendingOpenFilePath);
                g_PendingOpenFilePath.clear();
                g_ShowUnsavedChangesPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard & Open (저장 안 하고 열기)", ImVec2(190, 0)))
            {
                LoadFileIntoEditor(g_PendingOpenFilePath);
                g_PendingOpenFilePath.clear();
                g_ShowUnsavedChangesPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel (취소)", ImVec2(100, 0)))
            {
                g_PendingOpenFilePath.clear();
                g_ShowUnsavedChangesPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (g_ShowSettingsWindow)
        {
            if (ImGui::Begin(L::Get("EditorSettings").c_str(), &g_ShowSettingsWindow, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("%s", L::Get("FontSettings").c_str());
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::SliderFloat(L::Get("FontScale").c_str(), &g_EditorFontSize, 0.5f, 2.5f, "%.1f x");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Checkbox(L::Get("EnableCodeFolding").c_str(), &g_EnableCodeFolding))
                {
                    editor.SetFoldingEnabled(g_EnableCodeFolding);
                }

                ImGui::Spacing();
                if (ImGui::Button(L::Get("Close").c_str()))
                {
                    g_ShowSettingsWindow = false;
                }
            }
            ImGui::End();
        }

        if (!editor.GetErrorMarkers().empty() && g_LastError.line > 0)
        {
            uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (currentTime - g_LastError.errorTime > ERROR_DISPLAY_DURATION)
            {
                editor.SetErrorMarkers(TextEditor::ErrorMarkers());
                g_LastError = CompileErrorInfo();
            }
        }
    }
}