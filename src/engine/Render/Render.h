#pragma once
#include "imgui.h"
#include "../Settings/Settings.h"
#include "../Settings/Language/Language.h"

namespace RenderEditer
{
    void Renders();
    void top_bar();
    void ApplyUnityStyle();

    // ID of the main dockspace built in SetupDockSpace() - exposed so
    // topbar.cpp's "Layout" menu can pass it into Layout::LoadNamed()
    // when applying an entry (needed to rebuild the hand-built "디폴트"
    // arrangement when it was never explicitly saved) without
    // SetupDockSpace() needing to know anything about menus.
    // (SetupDockSpace()에서 만든 메인 도킹스페이스 ID - topbar.cpp의
    // "Layout" 메뉴가 항목을 적용할 때 Layout::LoadNamed()에 넘기도록
    // 공개함(한 번도 직접 저장한 적 없는 "디폴트"의 손수 만든 배치를
    // 다시 만들 때 필요함) - SetupDockSpace()가 메뉴에 대해 알 필요
    // 없이)
    extern ImGuiID g_MainDockspaceId;
}