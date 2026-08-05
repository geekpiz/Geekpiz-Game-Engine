#include "Window.h"

namespace Window{

	bool render_TextEditor = true;

	void Render_TextEditor()
	{
		if (render_TextEditor)
		{
			if (ImGui::Begin(L::Get("Text Editor").c_str(), &render_TextEditor))
			{
				ImGui::Text(L::Get("Text Editor").c_str());
			}
			ImGui::End();
		}
	}
}