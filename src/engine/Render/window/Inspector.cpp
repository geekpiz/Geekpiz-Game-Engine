#include "Window.h"
namespace Window {

	bool render_Inspector = true;

	void Render_Inspector()
	{
		if (render_Inspector)
		{
			if (ImGui::Begin(L::Get("Inspector").c_str(), &render_Inspector))
			{
				ImGui::Text(L::Get("Inspector").c_str());
			}
			ImGui::End();
		}
	}
}