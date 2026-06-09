#include "Window.h"
namespace Window {

	bool render_Game = true;

	void Render_Game()
	{
		if (Render_Game)
		{
			if (ImGui::Begin("Game", &render_Game));
			{
				ImGui::Text("Game");
			}
			ImGui::End();
		}
	}
}