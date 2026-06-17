#include "imgui.h"
#include "Render.h"
#include "window/Window.h"
#include "../../Main.h"
namespace RenderEditer {
    void top_bar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                //뒤에 컨 있어도 눌렀을 때 실행은 안 됨, 따라서 다음에 다시 연결필요
                if (ImGui::MenuItem("Open", "Ctrl + O")) { /* 열기 로직 */ }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Ctrl + Q")) { main_header::close_window(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl + S")) { /* 저장 로직 */ }
                ImGui::Separator();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Settings")) { /* 설정 창 열기 */ }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                if (ImGui::BeginMenu("General"))
                {
                    if (ImGui::MenuItem("Scene", "Alt + 1")) { Window::render_Scene = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Game", "Alt + 2")) { Window::render_Game = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Inspector", "Alt + 3")) { Window::render_Inspector = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Hierarchy", "Alt + 4")) { Window::render_Hierarchy = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Assets", "Alt + 5")) { Window::render_Assets = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Console", "Alt + 6")) { Window::render_Console = true; }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}