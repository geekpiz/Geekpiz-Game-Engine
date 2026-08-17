#pragma once

#include "ImGui.h"
#include "Settings.h"
#include "Language/Language.h"

namespace Window {
	//Variable(변수)
	extern bool render_Scene;
	extern bool render_Inspector;
	extern bool render_Hierarchy;
	extern bool render_Game;
	extern bool render_Console;
	extern bool render_Assets;
	extern bool render_CodeEditor;


	//Function(함수)
	void Render_Assets();
	void Render_Console();
	void Render_Game();
	void Render_Hierarchy();
	void Render_Inspector();
	void Render_Scene();
	void Render_CodeEditor();

	// Release the Code Editor's Squirrel VM (called once on engine shutdown)
	// (코드 에디터의 스쿼럴 VM 해제 - 엔진 종료 시 한 번 호출)
	void ShutdownCodeEditor();

	void SetProjectDirectory(const std::string& projectPath);
}