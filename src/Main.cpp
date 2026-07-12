#include "Main.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "engine/Render/Render.h"
#include "engine/Settings/Settings.h"
#include "engine/Settings/Language/Language.h"

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
    RenderEditer::ApplyCustomEngineStyle();

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