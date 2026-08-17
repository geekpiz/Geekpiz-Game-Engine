#include "Main.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "engine/Render/Render.h"
#include "engine/Render/Window/Window.h"
#include "engine/Settings/Settings.h"
#include "engine/Settings/Language/Language.h"
#include "engine/Utilities/ConsoleLog.h"
#include "engine/Settings/Language/FontLoader.h"
#include "engine/Layout/Layout.h"
#include <string>

GLFWwindow* g_Window = nullptr;

// Accept command-line arguments to receive parameters from the Hub App
// (허브 앱에서 매개변수를 전달받을 수 있도록 명령줄 인자 적용)
int main(int argc, char* argv[])
{
    // Parse Command-Line Arguments from External Hub App
    // (외부 허브 앱으로부터 전달받은 명령줄 인자 파싱)
    std::string targetProjectPath = "";
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--project-path" && i + 1 < argc)
        {
            targetProjectPath = argv[i + 1];
            i++;
        }
    }

    // Apply project path if passed from Hub, otherwise fallback to default path
    // (허브에서 지정한 프로젝트 경로가 있으면 적용하고, 없으면 기본 디렉토리 적용)
    if (!targetProjectPath.empty())
    {
        Window::SetProjectDirectory(targetProjectPath);
        ConsoleLog::Append(LogLevel::Info, "Project Directory Loaded from Hub: " + targetProjectPath);
    }

    Settings::Load();
    L::Load();
    if (!glfwInit()) return -1;

    // Setting the OpenGL version (3.3 Core)(OpenGL 버전 설정 (3.3 Core))
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Creating a Window(창 생성)
    g_Window = glfwCreateWindow(1280, 720, "Geekpiz Game Engine", NULL, NULL);
    if (!g_Window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(g_Window);
    glfwSwapInterval(1);

    // 4. Initializing ImGui(ImGui 초기화)
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Must run after ImGuiConfigFlags_DockingEnable is set and before the
    // first ImGui::NewFrame() - takes over ini persistence from ImGui's
    // default (which used to drop "imgui.ini" into whatever the working
    // directory happened to be) and loads the saved layout (도킹 활성화
    // 직후, 첫 NewFrame() 전에 호출해야 함 - ImGui 기본 ini 저장 방식
    // (작업 디렉토리에 "imgui.ini"를 그냥 떨어뜨리던 것)을 대신 가져오고
    // 저장된 레이아웃을 불러옴)
    Layout::Init();

    ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    RenderEditer::ApplyUnityStyle();
    ImFont* myFontW = FontLoader::Load(Settings::Get("font_name"));

    // Explicitly select the loaded font as the UI's default font, and warn (don't fail silently) if it's missing
    // (로드된 폰트를 UI 기본 폰트로 명시적으로 지정. 실패해도 조용히 넘어가지 않고 경고 로그를 남김)
    if (myFontW) {
        io.FontDefault = myFontW;
    }
    else {
        ConsoleLog::Append(LogLevel::Warning, "Custom font load failed, using ImGui's built-in font (한글 텍스트가 네모(tofu)로 보일 수 있음)");
    }


    // Main Loop(메인 루프)
    while (!glfwWindowShouldClose(g_Window))
    {
        glfwPollEvents();

        // Starting a new frame(프레임 시작)
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Drawing UI(UI 그리기)
        RenderEditer::Renders();

        // Rendering(렌더링)
        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g_Window);
    }

    // Cleanup(종료 처리)
    // Autosave the docking layout on the way out, so the next launch
    // reopens exactly how the user left it, without requiring an
    // explicit "Save Layout" click every time (종료 시 도킹 레이아웃을
    // 자동 저장함 - 매번 "레이아웃 저장"을 누르지 않아도 다음 실행 때
    // 마지막 상태 그대로 열림)
    Layout::Save();

    // BUG FIX: releases the Code Editor's Squirrel VM (see Code_Editor.cpp /
    // ShutdownCodeEditor for details on the leak this fixes).
    // (코드 에디터의 스쿼럴 VM을 해제함 - 어떤 누수를 고치는지는
    //  Code_Editor.cpp의 ShutdownCodeEditor 참고)
    Window::ShutdownCodeEditor();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(g_Window);
    glfwTerminate();

    return 0;
}

namespace main_header {

    void close_window()
    {
        glfwSetWindowShouldClose(g_Window, GLFW_TRUE);
    }
}