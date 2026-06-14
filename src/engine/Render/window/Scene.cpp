#include "Window.h"
namespace Window {

	bool render_Scene = true;

	void Render_Scene()
	{
		if (render_Scene)
		{
			if (ImGui::Begin("Scene", &render_Scene))
			{
				ImGui::Text("Scene");
			}
			ImGui::End();
		}
	}
}