#include "Window.h"
namespace Window {

	bool render_Console = true;

	void Render_Console()
	{
		if (render_Console)
		{
			if (ImGui::Begin(L::Get("Console").c_str(), &render_Console))
			{
				ImGui::Text(L::Get("Console").c_str());
			}
			ImGui::End();
		}
	}
}