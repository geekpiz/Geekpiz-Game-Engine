#include "Window.h"
namespace Window {

	bool render_Inspector = true;

	void Render_Inspector()
	{
		if (Render_Inspector)
		{
			if (ImGui::Begin("Inspector", &render_Inspector));
			{
				ImGui::Text("Inspector");
			}
			ImGui::End();
		}
	}
}