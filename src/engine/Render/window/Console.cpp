#include "Window.h"
namespace Window {

	bool render_Console = true;

	void Render_Console()
	{
		if (render_Console)
		{
			if (ImGui::Begin("Console", &render_Console))
			{
				ImGui::Text("Console");
			}
			ImGui::End();
		}
	}
}