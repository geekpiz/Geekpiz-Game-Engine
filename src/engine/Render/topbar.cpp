#include "imgui.h"
#include "Render.h"
#include "window/Window.h"
#include "../../Main.h"
#include "../Layout/Layout.h"
#include <string>
#include <vector>

namespace RenderEditer {

    // Text buffer for the "Save Layout As" name popup. Sized generously
    // since layout names may contain Korean (UTF-8, multiple bytes per
    // character). ("다른 이름으로 레이아웃 저장" 팝업의 이름 입력 버퍼.
    // 한글(UTF-8, 글자당 여러 바이트)이 들어갈 수 있어 넉넉하게 잡음)
    static char s_NewLayoutNameBuf[128] = "";

    // Renders the modal popup used by "Layout > Save As New Layout...".
    // Called every frame from top_bar() itself (NOT from inside the
    // "Layout" BeginMenu block) so the popup keeps rendering and taking
    // input across frames even after the menu that opened it has already
    // closed - ImGui menus only stay open while the mouse is over them.
    // ("Layout > 새 레이아웃으로 저장..."이 쓰는 모달 팝업. "Layout"
    //  BeginMenu 블록 안이 아니라 top_bar()에서 매 프레임 직접 호출함 -
    //  ImGui 메뉴는 마우스가 위에 있을 때만 열려있으므로, 팝업을 연 메뉴가
    //  이미 닫힌 뒤에도 팝업이 계속 그려지고 입력을 받게 하기 위함)
    static void DrawSaveLayoutPopup()
    {
        std::string title = L::Get("SaveLayoutAsTitle");
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(L::Get("SaveLayoutAsPrompt").c_str());
            ImGui::SetNextItemWidth(240.0f);
            bool enterPressed = ImGui::InputText("##NewLayoutName", s_NewLayoutNameBuf,
                IM_ARRAYSIZE(s_NewLayoutNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();

            bool okClicked = ImGui::Button(L::Get("OK").c_str(), ImVec2(100, 0));
            ImGui::SameLine();
            bool cancelClicked = ImGui::Button(L::Get("Cancel").c_str(), ImVec2(100, 0));

            bool wantsSave = (enterPressed || okClicked) && s_NewLayoutNameBuf[0] != '\0';
            if (wantsSave)
            {
                Layout::SaveAs(s_NewLayoutNameBuf);
                s_NewLayoutNameBuf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            else if (cancelClicked)
            {
                s_NewLayoutNameBuf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void top_bar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu(L::Get("File").c_str()))
            {
                // Even if there is a control button on the back, it won't work when pressed, so you'll need to reconnect it later.
                // (뒤에 컨트롤 있어도 눌렀을 때 실행은 안 됨, 따라서 다음에 다시 연결필요)
                if (ImGui::MenuItem(L::Get("Open").c_str(), "Ctrl + O")) { /* 열기 로직 */ }
                ImGui::Separator();
                if (ImGui::MenuItem(L::Get("Save").c_str(), "Ctrl + S")) { /* 저장 로직 */ }
                ImGui::Separator();
                if (ImGui::MenuItem(L::Get("Exit").c_str(), "Ctrl + Q")) { main_header::close_window(); }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(L::Get("Edit").c_str()))
            {
                if (ImGui::MenuItem(L::Get("Settings").c_str())) { /* 설정 창 열기 */ }
                ImGui::EndMenu();
            }

            // "Window": toggles which panels are visible/open. Layout
            // presets (below) are intentionally a separate top-level menu,
            // not nested in here - Window is about WHAT is shown, Layout
            // is about WHERE it's arranged and saved presets of that.
            // ("Window": 어떤 패널이 보이는지/열려있는지 토글함. 레이아웃
            //  프리셋(아래)은 일부러 여기 안에 넣지 않고 별도의 최상위
            //  메뉴로 둠 - Window는 "무엇을 보여줄지", Layout은 "어떻게
            //  배치하고 그 배치를 프리셋으로 저장할지"를 다루기 때문)
            if (ImGui::BeginMenu(L::Get("Window").c_str()))
            {
                if (ImGui::BeginMenu(L::Get("General").c_str()))
                {
                    if (ImGui::MenuItem(L::Get("Scene").c_str(), "Alt + 1")) { Window::render_Scene = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Game").c_str(), "Alt + 2")) { Window::render_Game = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Inspector").c_str(), "Alt + 3")) { Window::render_Inspector = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Hierarchy").c_str(), "Alt + 4")) { Window::render_Hierarchy = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Assets").c_str(), "Alt + 5")) { Window::render_Assets = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("Console").c_str(), "Alt + 6")) { Window::render_Console = true; }
                    ImGui::Separator();
                    if (ImGui::MenuItem(L::Get("CodeEditor").c_str(), "Alt + 7")) { Window::render_CodeEditor = true; }
                    ImGui::EndMenu();
                }

                // BUG FIX: this outer "Window" BeginMenu() was never
                // matched with an EndMenu() - only the inner "General" (and
                // formerly "Layout") submenus were closed. Forgetting
                // EndMenu() after a BeginMenu() that returned true corrupts
                // ImGui's ID/window stack, which asserts almost immediately
                // the next time this menu bar renders - the crash-on-open
                // this fix addresses. (바깥쪽 "Window" BeginMenu()가
                // EndMenu()와 짝이 맞지 않았음 - 안쪽 "General"(과 예전엔
                // "Layout") 서브메뉴만 닫혔었음. true를 반환한 BeginMenu()
                // 뒤에 EndMenu()를 빼먹으면 ImGui의 ID/창 스택이 깨져서
                // 다음 메뉴바 렌더링 때 거의 바로 assert가 터짐 - "열기만
                // 해도 터지는" 원인이 이거였음)
                ImGui::EndMenu();
            }

            // Top-level "Layout" menu (see comment above "Window"). Just
            // the 3 things this needs: save the current layout under a
            // new name, apply a saved layout from the list, and delete a
            // saved layout via right-click on its entry. "디폴트" is
            // always the first entry in the list (see Layout::GetSavedLayouts)
            // so there's no separate "Reset to Default" item needed.
            // (최상위 "Layout" 메뉴 - 위 "Window" 주석 참고. 딱 3가지만
            // 있으면 됨: 현재 레이아웃을 새 이름으로 저장, 목록에서 저장된
            // 레이아웃 적용, 목록 항목 우클릭으로 삭제. "디폴트"는 항상
            // 목록 첫 항목이라(Layout::GetSavedLayouts 참고) 별도의
            // "기본으로 초기화" 항목이 필요 없음)
            if (ImGui::BeginMenu(L::Get("Layout").c_str()))
            {
                if (ImGui::MenuItem(L::Get("SaveLayoutAs").c_str(), "Ctrl + Shift + S"))
                {
                    s_NewLayoutNameBuf[0] = '\0';
                    ImGui::OpenPopup(L::Get("SaveLayoutAsTitle").c_str());
                }

                ImGui::Separator();

                // Saved layout list, "디폴트" always first (레이아웃 리스트,
                // "디폴트"가 항상 첫 항목) - clicking an entry applies it;
                // the checkmark marks whichever one is active; right-click
                // opens a small "Delete" context menu on that entry.
                // (항목을 클릭하면 적용됨, 체크 표시는 현재 활성 항목을
                //  나타냄, 우클릭하면 해당 항목에 작은 "삭제" 컨텍스트
                //  메뉴가 뜸)
                std::string activeName = Layout::GetActiveLayoutName();
                std::vector<std::string> savedLayouts = Layout::GetSavedLayouts();

                for (const std::string& name : savedLayouts)
                {
                    bool isActive = (name == activeName);
                    bool clicked = ImGui::MenuItem(name.c_str(), nullptr, isActive);

                    // Right-click context menu on THIS entry only - the
                    // "##ctx_" + name id keeps each entry's popup separate
                    // from the others. (이 항목 하나에 대한 우클릭 메뉴 -
                    // "##ctx_" + name ID로 항목마다 팝업을 서로 구분함)
                    if (ImGui::BeginPopupContextItem(("##ctx_" + name).c_str()))
                    {
                        if (ImGui::MenuItem(L::Get("Delete").c_str()))
                        {
                            Layout::DeleteNamed(name);
                        }
                        ImGui::EndPopup();
                    }

                    if (clicked && g_MainDockspaceId != 0)
                    {
                        Layout::LoadNamed(name, g_MainDockspaceId);
                    }
                }

                ImGui::EndMenu();
            }

            // BUG FIX: BeginMainMenuBar() was never matched with
            // EndMainMenuBar(). Besides being the same kind of stack
            // corruption as the missing EndMenu() above, skipping it also
            // stops ImGui from shrinking the main viewport's work area by
            // the menu bar's height - every other window (including the
            // dockspace) was laid out as if the menu bar took up zero
            // space, which is what produced the stray vertical lines /
            // overlapping top-bar artifacts seen on screen.
            // (BeginMainMenuBar()가 EndMainMenuBar()와 짝이 맞지 않았음.
            //  위 EndMenu() 누락과 같은 종류의 스택 손상인 것은 물론, 이걸
            //  건너뛰면 ImGui가 메인 뷰포트의 작업 영역을 메뉴바 높이만큼
            //  줄이지 않게 됨 - 도킹스페이스를 포함한 다른 모든 창이 메뉴바가
            //  차지하는 공간이 0인 것처럼 배치되어, 화면에 보이던 삐져나온
            //  세로선/겹치는 상단 바 잔상이 여기서 나온 것)
            ImGui::EndMainMenuBar();
        }

        DrawSaveLayoutPopup();
    }
}
