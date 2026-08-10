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

namespace Window {

    bool render_CodeEditor = false;  // 기본적으로 코드 에디터 창을 켠 상태로 시작
    TextEditor editor;
    bool isEditorInitialized = false; // Flag for editor initialization (에디터 초기화 여부 플래그)

    // Editor Settings Variables (에디터 설정 변수)
    static float g_EditorFontSize = 1.0f; // Font scale factor (글자 크기 스케일)
    static bool g_ShowSettingsWindow = false; // Toggle for Settings popup (설정 팝업 표시 여부)
    static bool g_EnableCodeFolding = true; // Toggle for Code Folding functionality (코드 접기 기능 활성화 여부)

    HSQUIRRELVM g_ScriptVM = nullptr; // Global Squirrel VM instance (전역 스쿼럴 VM 인스턴스)

    // Template File Path for NutComponent relative to Engine Root (엔진 루트 기준 NutComponent 템플릿 상대 경로)
    const std::string SCRIPT_TEMPLATE_PATH = "Squirrel Script/NutComponent.txt";

    struct CompileErrorInfo {
        int line = -1;
        std::string message;
        uint64_t errorTime = 0; // 에러 발생 시간 (ms)
    };
    static CompileErrorInfo g_LastError;
    static const uint64_t ERROR_DISPLAY_DURATION = 5000; // 에러 표시 시간 (5초)

    // Squirrel compiler error handler callback (스쿼럴 컴파일러 에러 콜백 함수)
    void SquirrelCompilerErrorHandler(HSQUIRRELVM v, const SQChar* desc, const SQChar* source, SQInteger line, SQInteger column)
    {
        g_LastError.line = static_cast<int>(line);
        g_LastError.message = desc ? std::string(desc) : L::Get("UnknownSyntaxError");
        g_LastError.errorTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        ConsoleLog::Append(LogLevel::Error,
            L::Get("ScriptErrorPrefix") + std::to_string(line) + ", Col " + std::to_string(column) + ": " + g_LastError.message);
    }

    // Adjust error line heuristic function (에러 줄 보정 휴리스틱 함수)
    void AdjustErrorLine(CompileErrorInfo& error) {
        if (error.line > 1 && (error.message.find("expected 'IDENTIFIER'") != std::string::npos ||
            error.message.find("expected ')'") != std::string::npos ||
            error.message.find("expected ';'") != std::string::npos))
        {
            // Move error marker to the previous line where the actual mistake likely is (실제 실수가 발생했을 가능성이 높은 이전 줄로 마커 이동)
            error.line -= 1;
        }
    }

    void InitCodeEditor()
    {
        // Load Settings file first (설정 파일 먼저 로드)
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

        // Enable Code Folding feature (코드 접기 기능 활성화)
        editor.SetFoldingEnabled(g_EnableCodeFolding);

        // Customize palette using Settings::GetColor (Settings::GetColor를 이용한 동적 색상 설정)
        auto palette = TextEditor::GetDarkPalette();

        // Helper lambda to swap Red and Blue channels (빨간색과 파란색 채널을 교환하는 헬퍼 람다 함수)
        auto GetSwappedColor = [](const std::string& key, unsigned int defaultArgb) -> unsigned int {
            unsigned int color = Settings::GetColor(key, defaultArgb);
            return (color & 0xFF00FF00) | ((color & 0x00FF0000) >> 16) | ((color & 0x000000FF) << 16);
            };

        // 1. Error Marker Color: Fixed to Red (에러 마커 색상: 빨간색으로 수정)
        palette[(int)TextEditor::PaletteIndex::ErrorMarker] = GetSwappedColor("Editor_ErrorMarker", 0x80FF0000);

        // 2. Syntax Highlighting Colors from Settings (설정 파일 기반 구문 강조 색상)
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

        // Load default code template from AppData using FS::ReadFile (FS::ReadFile을 이용하여 AppData에서 기본 코드 템플릿 로드)
        std::string defaultScript = FS::ReadFile(SCRIPT_TEMPLATE_PATH);

        // If file doesn't exist, generate default NutComponent file via FS::WriteFile (파일이 없으면 FS::WriteFile로 기본 NutComponent 파일 생성)
        if (defaultScript.empty()) {
            defaultScript =
                "// Geekpiz NutComponent Script Template\n\n"
                "start function Start()\n"
                "{\n"
                "	\n"
                "}\n\n"
                "update function Update()\n"
                "{\n"
                "	\n"
                "}\n";
            // Save default file to AppData path (AppData 경로에 기본 NutComponent 파일 저장)
            FS::WriteFile(SCRIPT_TEMPLATE_PATH, defaultScript);
        }

        editor.SetText(defaultScript);
        isEditorInitialized = true;
    }

    // Full syntax check across the entire code (전체 코드 대상 구문 검사)
    void CheckSyntaxFull()
    {
        editor.SetErrorMarkers(TextEditor::ErrorMarkers());

        // 에러 타이머 확인: 5초 이상 지났으면 에러 메시지 자동 삭제
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

        // Compile without executing (실행하지 않고 전체 컴파일 구문 검사만 수행)
        if (SQ_FAILED(sq_compilebuffer(g_ScriptVM, processedCode.c_str(), processedCode.length(), _SC("EditorScriptBuffer"), SQTrue)))
        {
            if (g_LastError.line > 0) {
                AdjustErrorLine(g_LastError);

                // Highlight all syntax errors in the entire file (전체 파일 내의 발생한 구문 에러를 모두 표시)
                TextEditor::ErrorMarkers markers;
                markers.insert({ g_LastError.line, g_LastError.message });
                editor.SetErrorMarkers(markers);
            }
        }
        else {
            // If compilation succeeded, pop the closure off the stack (컴파일 성공 시 스택 정리)
            sq_pop(g_ScriptVM, 1);
            g_LastError = CompileErrorInfo(); // 에러 초기화
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

    void Render_CodeEditor()
    {
        if (!render_CodeEditor) return;

        if (!isEditorInitialized) {
            InitCodeEditor();
        }

        if (ImGui::Begin(L::Get("CodeEditor").c_str(), &render_CodeEditor, ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu(L::Get("File").c_str()))
                {
                    if (ImGui::MenuItem(L::Get("Save").c_str(), "Ctrl+S"))
                    {
                        // Auto-save edited script back to NutComponent.txt file (NutComponent.txt 파일에 수정 내용 자동 저장)
                        if (FS::WriteFile(SCRIPT_TEMPLATE_PATH, editor.GetText())) {
                            ConsoleLog::Append(LogLevel::Info, L::Get("SaveSuccess"));
                        }
                        else {
                            ConsoleLog::Append(LogLevel::Error, L::Get("SaveFailed"));
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

            ImGui::SetWindowFontScale(g_EditorFontSize);

            // Render Text Editor (텍스트 에디터 렌더링)
            editor.Render("TextEditor");

            // Live Syntax Checking on Text Change (텍스트 변경 시 실시간 전체 구문 검사)
            if (editor.IsTextChanged())
            {
                CheckSyntaxFull();
            }
        }
        ImGui::End();

        // Settings Popup Window (설정 팝업 창)
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

                // Toggle Code Folding checkbox (코드 접기 활성화 체크박스)
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

        // Auto-clear error markers after timeout (타임아웃 후 에러 마커 자동 삭제)
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