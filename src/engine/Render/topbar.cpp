#include "imgui.h"
#include "Render.h"
#include "window/Window.h"
#include "../../Main.h"
namespace RenderEditer {
    void top_bar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu(Settings::Get("File").c_str()))
            {
                // Even if there is a control button on the back, it won't work when pressed, so you'll need to reconnect it later.
                // (뒤에 컨트롤 있어도 눌렀을 때 실행은 안 됨, 따라서 다음에 다시 연결필요)
                if (ImGui::MenuItem(L::Get("Open").c_str(), "Ctrl + O")) { /* 열기 로직 */ }
                ImGui::Separator();
                if (ImGui::MenuItem(L::Get("Exit").c_str(), "Ctrl + Q")) { main_header::close_window(); }
                ImGui::Separator();
                if (ImGui::MenuItem(L::Get("Save").c_str(), "Ctrl + S")) { /* 저장 로직 */ }
                ImGui::Separator();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(L::Get("Edit").c_str()))
            {
                if (ImGui::MenuItem(L::Get("Settings").c_str())) { /* 설정 창 열기 */ }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(L::Get("Window").c_str()))
            {
                if (ImGui::BeginMenu(L::Get("General").c_str()))
                {
                    if (ImGui::MenuItem(L::Get("Scene").c_str(), "Alt + 1")) { Window::render_Scene = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Game").c_str(), "Alt + 2")) { Window::render_Game = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Inspector").c_str(), "Alt + 3")) { Window::render_Inspector = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Hierarchy").c_str(), "Alt + 4")) { Window::render_Hierarchy = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Assets").c_str(), "Alt + 5")) { Window::render_Assets = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Console").c_str(), "Alt + 6")) { Window::render_Console = true; }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }
}