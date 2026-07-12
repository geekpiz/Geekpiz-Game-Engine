#include "Window.h"
namespace Window {

	bool render_Hierarchy = true;

	void Render_Hierarchy()
	{
		if (render_Hierarchy)
		{
			if (ImGui::Begin(L::Get("Hierarchy").c_str(), &render_Hierarchy))
			{
				ImGui::Text(L::Get("Hierarchy").c_str());
			}
			ImGui::End();
		}
	}
}