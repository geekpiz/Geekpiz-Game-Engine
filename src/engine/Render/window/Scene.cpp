#include "Window.h"
namespace Window {

	bool render_Scene = true;

	void Render_Scene()
	{
		if (render_Scene)
		{
			if (ImGui::Begin(L::Get("Scene").c_str(), &render_Scene))
			{
				ImGui::Text(L::Get("Scene").c_str());
			}
			ImGui::End();
		}
	}
}