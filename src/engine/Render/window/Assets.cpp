#include "Window.h"

namespace Window{

	bool render_Assets = true;

	void Render_Assets()
	{
		if (render_Assets)
		{
			if (ImGui::Begin(L::Get("Assets").c_str(), &render_Assets))
			{
				ImGui::Text(L::Get("Assets").c_str());
			}
			ImGui::End();
		}
	}
}