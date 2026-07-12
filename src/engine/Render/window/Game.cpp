#include "Window.h"
namespace Window {

	bool render_Game = true;

	void Render_Game()
	{
		if (render_Game)
		{
			if (ImGui::Begin(L::Get("Game").c_str(), &render_Game))
			{
				ImGui::Text(L::Get("Game").c_str());
			}
			ImGui::End();
		}
	}
}