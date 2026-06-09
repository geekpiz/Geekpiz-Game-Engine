#include "Window.h"
namespace Window{

	bool render_Assets = true;

	void Render_Assets()
	{
		if (Render_Assets)
		{
			if (ImGui::Begin("Assets", &render_Assets));
			{
				ImGui::Text("Assets");
			}
			ImGui::End();
		}
	}
}