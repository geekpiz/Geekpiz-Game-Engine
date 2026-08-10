#pragma once

#include <string>
#include "imgui.h"

namespace FontLoader {

    ImFont* Load(const std::string& fontName, float fontSize = 18.0f);

    // Main single-line load function for all languages and user fonts (모든 언어 및 유저 폰트를 위한 단일 줄 메인 로드 함수)
    ImFont* Load(const std::wstring& fontName, float fontSize = 18.0f);

    // Overloaded load function for detailed control with ImGuiIO and ImFontConfig (ImGuiIO 및 ImFontConfig 상세 제어용 오버로딩 로드 함수)
    ImFont* Load(ImGuiIO& io, const std::wstring& fontName, float fontSize = 18.0f, const ImWchar* glyphRanges = nullptr, ImFontConfig* fontConfig = nullptr);

}