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

GLFWwindow* g_Window = nullptr;

int main()
{

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
    ImGui_ImplGlfw_InitForOpenGL(g_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    RenderEditer::ApplyUnityStyle();
    ImFont* myFontW =  FontLoader::Load(Settings::Get("font_name"));

    // Explicitly select the loaded font as the UI's default font, and warn (don't fail silently) if it's missing
    // (로드된 폰트를 UI 기본 폰트로 명시적으로 지정. 실패해도 조용히 넘어가지 않고 경고 로그를 남김)
    if (myFontW) {
        io.FontDefault = myFontW;
    } else {
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