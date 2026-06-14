#pragma once

#include "ImGui.h"

namespace Window {
	//Variable(변수)
	extern bool render_Scene;
	extern bool render_Inspector;
	extern bool render_Hierarchy;
	extern bool render_Game;
	extern bool render_Console;
	extern bool render_Assets;


	//Function(함수)
	void Render_Assets();
	void Render_Console();
	void Render_Game();
	void Render_Hierarchy();
	void Render_Inspector();
	void Render_Scene();
}