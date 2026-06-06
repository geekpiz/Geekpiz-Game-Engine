#include "imgui.h"
#include "Render.h"

namespace RenderEditer
{
    // Docking Space Settings(도킹 스페이스 설정)
    void SetupDockSpace()
    {
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("MyDockSpace", nullptr, window_flags | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        ImGui::End();
    }

	void render_chanyul()
	{
		ImGui::Begin("Geekpiz Engine Controller");
		ImGui::Text("Engine is running smoothly, Chan-yul!");
		ImGui::End();

		ImGui::Begin("Geekpiz  Controller");
		ImGui::Text("Engine is  smoothly, Chan-yul!");
		ImGui::End();
	}

	void Renders()
	{
        SetupDockSpace();
		render_chanyul();
	}
}