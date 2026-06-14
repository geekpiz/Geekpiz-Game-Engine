#include "Window.h"
namespace Window {

	bool render_Hierarchy = true;

	void Render_Hierarchy()
	{
		if (render_Hierarchy)
		{
			if (ImGui::Begin("Hierarchy", &render_Hierarchy))
			{
				ImGui::Text("Hierarchy");
			}
			ImGui::End();
		}
	}
}