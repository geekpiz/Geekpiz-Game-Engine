#pragma once
#include "imgui.h"
#include "Render.h"
#include "../Settings/Settings.h" // Check relative path (상대 경로 확인)

namespace RenderEditer {

    // =========================================================================
    // Color Converter (색상 변환 함수)
    // =========================================================================
    // Converts 0xRRGGBB hex values into ImVec4 for ImGui (Hex 코드를 ImVec4로 변환)
    ImVec4 GetImColor(unsigned int hexColor, float alpha = 1.0f) {
        return ImVec4(
            ((hexColor >> 16) & 0xFF) / 255.0f,
            ((hexColor >> 8) & 0xFF) / 255.0f,
            (hexColor & 0xFF) / 255.0f,
            alpha
        );
    }

    // =========================================================================
    // Style Functions (스타일 적용 함수)
    // =========================================================================

    // 1. Classical dark Style (기존 다크 스타일)
    void ApplyUnityStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Load colors inside function to prevent crash (크래시 방지를 위해 함수 내부에서 색상 로드)
        unsigned int MainColor1 = Settings::GetColor("MainColor1", 0x2D2D2D);
        unsigned int ChildBgColor = Settings::GetColor("ChildBgColor", 0x151515);
        unsigned int SubColor1 = Settings::GetColor("SubColor1", 0x121212);
        unsigned int TextColor2 = Settings::GetColor("TextColor2", 0x808080);
        unsigned int FrameBgColor = Settings::GetColor("FrameBgColor", 0x252525);
        unsigned int ButtonColor = Settings::GetColor("ButtonColor", 0x303030);
        unsigned int HoveredColor = Settings::GetColor("HoveredColor", 0x333333);
        unsigned int SubColor2 = Settings::GetColor("SubColor2", 0x262829);

        style.WindowRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;

        // Apply Backgrounds & Borders (배경 및 테두리 적용)
        colors[ImGuiCol_WindowBg] = GetImColor(MainColor1);
        colors[ImGuiCol_ChildBg] = GetImColor(ChildBgColor);
        colors[ImGuiCol_PopupBg] = GetImColor(MainColor1);
        colors[ImGuiCol_Border] = GetImColor(SubColor1);

        // Apply Text (텍스트 적용)
        colors[ImGuiCol_Text] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
        colors[ImGuiCol_TextDisabled] = GetImColor(TextColor2);

        // Apply Frames & Buttons (프레임 및 버튼 적용)
        colors[ImGuiCol_FrameBg] = GetImColor(FrameBgColor);
        colors[ImGuiCol_FrameBgHovered] = GetImColor(ButtonColor);
        colors[ImGuiCol_FrameBgActive] = GetImColor(HoveredColor);
        colors[ImGuiCol_Button] = GetImColor(ButtonColor);
        colors[ImGuiCol_ButtonHovered] = GetImColor(HoveredColor);
        colors[ImGuiCol_ButtonActive] = GetImColor(FrameBgColor);

        // Apply Highlights & Tabs (하이라이트 및 탭 적용)
        colors[ImGuiCol_Header] = GetImColor(SubColor2);
        colors[ImGuiCol_HeaderHovered] = GetImColor(SubColor2);
        colors[ImGuiCol_HeaderActive] = GetImColor(SubColor2);

        colors[ImGuiCol_TitleBg] = GetImColor(ButtonColor);
        colors[ImGuiCol_TitleBgActive] = GetImColor(FrameBgColor);
        colors[ImGuiCol_ScrollbarBg] = GetImColor(SubColor1);
        colors[ImGuiCol_ScrollbarGrab] = GetImColor(ButtonColor);
        colors[ImGuiCol_Separator] = GetImColor(SubColor1);
    }

    // 2. Custom Editor Style (Geekpiz 엔진 전용 커스텀 스타일)
    void ApplyCustomEngineStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Load colors inside function to prevent crash (크래시 방지를 위해 함수 내부에서 색상 로드)
        unsigned int MainColor1 = Settings::GetColor("MainColor1", 0x2D2D2D);
        unsigned int MainColor2 = Settings::GetColor("MainColor2", 0x1C1C1C);
        unsigned int SubColor2 = Settings::GetColor("SubColor2", 0x262829);
        unsigned int HoveredColor = Settings::GetColor("HoveredColor", 0x333333);
        unsigned int SubColor1 = Settings::GetColor("SubColor1", 0x121212);
        unsigned int TextColor1 = Settings::GetColor("TextColor1", 0xFFFFFF);
        unsigned int FrameBgColor = Settings::GetColor("FrameBgColor", 0x252525);

        // Window & Frame Padding (탑바 위아래 두께 및 여백 대폭 확장)
        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(12, 10);     // Thick top bar (탑바를 도톰하고 묵직하게 변경)
        style.ItemSpacing = ImVec2(8, 6);
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;

        // Tab Shapes (탭 디자인 설정)
        style.TabRounding = 8.0f;                // Round tabs (탭 윗부분 모서리를 부드럽고 둥글게)
        style.TabBorderSize = 2.0f;              // Thick tab borders (탭 테두리 두께 강조)

        // Apply Colors via Converter (변환 함수를 통해 상단 변수값 대입)
        colors[ImGuiCol_WindowBg] = GetImColor(MainColor1);
        colors[ImGuiCol_MenuBarBg] = GetImColor(MainColor2);

        // Tab Styling (탭 세부 스타일링)
        colors[ImGuiCol_Tab] = GetImColor(MainColor2);
        colors[ImGuiCol_TabActive] = GetImColor(SubColor2);
        colors[ImGuiCol_TabHovered] = GetImColor(HoveredColor);
        colors[ImGuiCol_TabUnfocused] = GetImColor(MainColor2);
        colors[ImGuiCol_TabUnfocusedActive] = GetImColor(SubColor2);

        // Text & Line Borders (텍스트 및 외곽선 테두리)
        colors[ImGuiCol_Text] = GetImColor(TextColor1);
        colors[ImGuiCol_Border] = GetImColor(SubColor1);
        colors[ImGuiCol_FrameBg] = GetImColor(FrameBgColor);

        unsigned int DockPreviewColor = Settings::GetColor("SubColor2", 0x262829);
        colors[ImGuiCol_DockingPreview] = GetImColor(DockPreviewColor, 0.5f);

    }
}