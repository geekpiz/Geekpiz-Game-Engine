#include "Window.h"
namespace Window {

	bool render_Game = true;

	void Render_Game()
	{
		if (render_Game)
		{
			if (ImGui::Begin("Game", &render_Game))
			{
				ImGui::Text("Game");
			}
			ImGui::End();
		}
	}
}