#include "Window.h"
#include "ConsoleLog.h"
#include <imgui.h>
#include <string>
#include <algorithm>

namespace Window {

    bool render_Console = true;

    // Filter Toggle States (로그 레벨 필터 토글 상태 변수)
    static bool showInfo = true;
    static bool showWarning = true;
    static bool showError = true;

    // Helper: Render count badge (카운트 뱃지 렌더링 헬퍼 함수)
    static void RenderBadge(int count) {
        if (count <= 1) return; // Hide if count is 1 or less (1 이하일 때는 표시 안 함)

        ImGui::SameLine(); // Align badge horizontally with previous text (이전 텍스트와 가로로 나란히 배치)

        // Prepare badge text (뱃지 텍스트 준비)
        std::string badgeText = (count > 99) ? "99+" : std::to_string(count);

        // Calculate text size and badge dimensions (텍스트 크기 및 뱃지 치수 계산)
        ImVec2 textSize = ImGui::CalcTextSize(badgeText.c_str());
        float paddingX = 6.0f; // Horizontal padding (가로 여백)
        float paddingY = 2.0f; // Vertical padding (세로 여백)

        float badgeWidth = (std::max)(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
        float badgeHeight = textSize.y + paddingY * 2.0f;

        // Get current cursor position (현재 커서 위치 가져오기)
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Define colors (색상 정의 - 어두운 회색 배경과 밝은 회색 텍스트)
        ImU32 badgeBgColor = IM_COL32(65, 65, 65, 255);   // Dark Gray Background (어두운 회색 배경)
        ImU32 badgeTextColor = IM_COL32(220, 220, 220, 255); // White Text (밝은 회색/흰색 텍스트)

        // Draw rounded rectangle background (둥근 사각형 배경 그리기)
        float cornerRadius = badgeHeight * 0.5f; // Pill shape rounding (알약 모양 라운딩)
        drawList->AddRectFilled(
            cursorPos,
            ImVec2(cursorPos.x + badgeWidth, cursorPos.y + badgeHeight),
            badgeBgColor,
            cornerRadius
        );

        // Draw center-aligned text (중앙 정렬된 텍스트 그리기)
        ImVec2 textPos = ImVec2(
            cursorPos.x + (badgeWidth - textSize.x) * 0.5f,
            cursorPos.y + (badgeHeight - textSize.y) * 0.5f
        );
        drawList->AddText(textPos, badgeTextColor, badgeText.c_str());

        // Advance Dummy cursor for ImGui layout (ImGui 레이아웃을 위한 커서 이동)
        ImGui::Dummy(ImVec2(badgeWidth, badgeHeight));
    }

    void Render_Console()
    {
        if (!render_Console) return;

        // BUG FIX: unlike the Code Editor's settings popup (already fixed
        // previously), this window never checked ImGui::Begin()'s return
        // value, so the whole log list, filter buttons, and slider kept
        // being rebuilt every frame even while the window was collapsed.
        // On an 8GB RAM / 1GB VRAM machine that's wasted CPU work piling up
        // for a panel the user isn't even looking at.
        // (BUG FIX: 이미 고쳤던 코드 에디터의 설정 팝업과 달리, 이 창은
        //  ImGui::Begin()의 반환값을 확인하지 않아서 창이 접혀있어도 로그
        //  목록, 필터 버튼, 슬라이더가 매 프레임 계속 다시 그려지고 있었음.
        //  8GB RAM / 1GB VRAM 환경에서는 보고 있지도 않은 패널을 위해
        //  불필요한 CPU 작업이 계속 쌓이는 셈)
        if (!ImGui::Begin(L::Get("Console").c_str(), &render_Console))
        {
            ImGui::End();
            return;
        }

        // --- 1. Left Controls (좌측 컨트롤 툴바) ---
        // Clear button with localized text (다국어 적용 Clear 버튼)
        if (ImGui::Button(L::Get("Clear").c_str())) {
            ConsoleLog::Clear();
        }

        ImGui::SameLine();

        // Copy All button with localized text (다국어 적용 Copy All 버튼)
        if (ImGui::Button(L::Get("Copy All").c_str())) {
            std::string allLogs;
            for (const auto& entry : ConsoleLog::GetEntries()) {
                if (entry.level == LogLevel::Info && !showInfo) continue;
                if (entry.level == LogLevel::Warning && !showWarning) continue;
                if (entry.level == LogLevel::Error && !showError) continue;

                std::string prefix = (entry.level == LogLevel::Info) ? "[Info] " :
                    (entry.level == LogLevel::Warning) ? "[Warning] " : "[Error] ";
                allLogs += prefix + entry.message;
                if (entry.count > 1) {
                    allLogs += " (x" + std::to_string(entry.count) + ")";
                }
                allLogs += "\n";
            }
            ImGui::SetClipboardText(allLogs.c_str());
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();

        // Font scale slider control with localized label (다국어 적용 글자 크기 라벨)
        static float textScale = 1.0f;
        ImGui::Text("%s", L::Get("Text Scale:").c_str());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::SliderFloat("##TextScale", &textScale, 0.7f, 2.0f, "%.1fx");

        // --- 2. Right Controls - Filter Toggle Buttons (우측 정렬 필터 토글 버튼) ---
        std::string infoBtnText = L::Get("Info").c_str() + std::string(": ") + std::to_string(ConsoleLog::GetInfoCount());
        std::string warningBtnText = L::Get("Warnings").c_str() + std::string(": ") + std::to_string(ConsoleLog::GetWarningCount());
        std::string errorBtnText = L::Get("Errors").c_str() + std::string(": ") + std::to_string(ConsoleLog::GetErrorCount());

        float styleSpacing = ImGui::GetStyle().ItemSpacing.x;
        float infoBtnWidth = ImGui::CalcTextSize(infoBtnText.c_str()).x + 20.0f;
        float warningBtnWidth = ImGui::CalcTextSize(warningBtnText.c_str()).x + 20.0f;
        float errorBtnWidth = ImGui::CalcTextSize(errorBtnText.c_str()).x + 20.0f;

        float totalRightWidth = infoBtnWidth + warningBtnWidth + errorBtnWidth + (styleSpacing * 2.0f);

        float rightStartPosX = ImGui::GetContentRegionAvail().x - totalRightWidth;
        if (rightStartPosX > ImGui::GetCursorPosX()) {
            ImGui::SameLine(ImGui::GetWindowWidth() - totalRightWidth - ImGui::GetStyle().WindowPadding.x);
        }
        else {
            ImGui::SameLine();
        }

        // Info Filter Button (Info 필터 버튼)
        if (!showInfo) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.45f, 1.0f));
        if (ImGui::Button(infoBtnText.c_str(), ImVec2(infoBtnWidth, 0))) {
            showInfo = !showInfo;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Warning Filter Button (Warning 필터 버튼)
        if (!showWarning) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.4f, 0.1f, 1.0f));
        if (ImGui::Button(warningBtnText.c_str(), ImVec2(warningBtnWidth, 0))) {
            showWarning = !showWarning;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Error Filter Button (Error 필터 버튼)
        if (!showError) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        else ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(errorBtnText.c_str(), ImVec2(errorBtnWidth, 0))) {
            showError = !showError;
        }
        ImGui::PopStyleColor();

        ImGui::Separator();

        // --- 3. Log Scroll Area (로그 스크롤 영역) ---
        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::SetWindowFontScale(textScale);

        int logIndex = 0; // Unique ID index for selectable item (선택 가능 항목 고유 ID 키값)
        for (const auto& entry : ConsoleLog::GetEntries()) {
            // Filter out disabled log levels (꺼져 있는 로그 레벨 제외)
            if (entry.level == LogLevel::Info && !showInfo) continue;
            if (entry.level == LogLevel::Warning && !showWarning) continue;
            if (entry.level == LogLevel::Error && !showError) continue;

            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Info: White (흰색)
            if (entry.level == LogLevel::Warning) color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Warning: Yellow (노란색)
            else if (entry.level == LogLevel::Error) color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // Error: Red (빨간색)

            // Render clickable log message (클릭 가능하고 복사 기능이 제공되는 로그 메시지 렌더링)
            ImGui::PushStyleColor(ImGuiCol_Text, color);

            std::string labelId = entry.message + "##" + std::to_string(logIndex++);
            if (ImGui::Selectable(labelId.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                std::string prefix = (entry.level == LogLevel::Info) ? "[Info] " :
                    (entry.level == LogLevel::Warning) ? "[Warning] " : "[Error] ";
                std::string singleLog = prefix + entry.message;
                if (entry.count > 1) {
                    singleLog += " (x" + std::to_string(entry.count) + ")";
                }

                ImGui::SetClipboardText(singleLog.c_str());
            }
            ImGui::PopStyleColor();

            // Localized hover tooltip (다국어화 툴팁)
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("%s", L::Get("Click to copy log").c_str());
            }

            // Render count badge on the same line (같은 줄에 카운트 뱃지 출력)
            RenderBadge(entry.count);

            // Draw line separator under each log item (로그 하나 밑에 선 긋기)
            ImGui::Separator();
        }

        // Auto-scroll to bottom (하단 자동 스크롤)
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
        ImGui::End();
    }

}