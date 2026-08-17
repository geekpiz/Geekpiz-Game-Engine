#pragma once

#include "imgui.h"
#include <string>
#include <vector>

// Docking Layout Persistence (도킹 레이아웃 저장/복원 시스템)
// -----------------------------------------------------------------
// Dear ImGui already remembers window sizes/positions/docking through
// its own "imgui.ini" file (io.IniFilename) - that's WHY "imgui.ini"
// was showing up next to the build's .exe/.pdb: the engine never set
// io.IniFilename, so ImGui defaulted to writing it into the process's
// current working directory (the build output folder when launched
// from Visual Studio).
//
// This module takes that over explicitly: io.IniFilename is disabled
// (set to nullptr) and the same ini-format blob is instead saved to
// %LOCALAPPDATA%/Geekpiz/Game Engine/Settings/Layout.ini via the
// existing FS:: utility. Small related preferences (asset icon size,
// etc.) still go through Settings::Set/GetFloat/Save - see Settings.h.
// A brand new install (no Layout.ini yet) gets a hand-built default
// layout instead of one giant empty docking area.
//
// (Dear ImGui는 창 크기/위치/도킹 상태를 자체 "imgui.ini" 파일
//  (io.IniFilename)로 이미 기억함 - 그래서 build의 .exe/.pdb 옆에
//  "imgui.ini"가 있었던 것: 엔진이 io.IniFilename을 설정한 적이 없어서
//  ImGui가 기본값대로 프로세스의 현재 작업 디렉토리(비주얼 스튜디오로
//  실행했을 때는 빌드 출력 폴더)에 그냥 써버린 것임.
//
//  이 모듈은 이걸 명시적으로 가져옴: io.IniFilename을 꺼두고(nullptr),
//  같은 ini 형식 데이터를 기존 FS:: 유틸리티를 통해
//  %LOCALAPPDATA%/Geekpiz/Game Engine/Settings/Layout.ini에 저장함.
//  아이콘 크기 같은 자잘한 설정들은 여전히 Settings::Set/GetFloat/Save를
//  씀 - Settings.h 참고. Layout.ini가 없는 첫 설치라면 텅 빈 도킹
//  영역 대신 미리 만들어둔 기본 레이아웃을 사용함)

namespace Layout {

    // Call ONCE right after ImGui::CreateContext() + enabling
    // ImGuiConfigFlags_DockingEnable, before the first ImGui::NewFrame().
    // Disables ImGui's own ini file and loads the saved layout (if any)
    // from Settings/Layout.ini.
    // (ImGui::CreateContext() + 도킹 활성화 직후, 첫 ImGui::NewFrame()
    //  전에 딱 한 번 호출함. ImGui 자체 ini 파일을 끄고 Settings/Layout.ini에
    //  저장된 레이아웃이 있으면 불러옴)
    void Init();

    // Call once per frame, immediately after ImGui::DockSpace(dockspace_id, ...)
    // inside the dockspace host window. On the very first frame after a
    // fresh install (no saved layout found by Init()), this builds the
    // default docking arrangement. Every other frame it's a no-op.
    // (매 프레임, 도킹 스페이스를 담는 창 안에서 ImGui::DockSpace(dockspace_id, ...)
    //  호출 직후에 부름. 저장된 레이아웃이 없는 첫 실행이라면 이 프레임에서
    //  기본 도킹 배치를 만듦. 그 외에는 아무 동작도 하지 않음)
    void Update(ImGuiID dockspaceId);

    // Serialize the current docking/window layout and write it to
    // Settings/Layout.ini. Safe to call anytime after the dockspace has
    // been built at least once (e.g. from a "Save Layout" menu item, or
    // automatically right before the engine shuts down).
    // (현재 도킹/창 레이아웃을 직렬화해서 Settings/Layout.ini에 씀.
    //  도킹 스페이스가 최소 한 번 이상 만들어진 뒤라면 언제 호출해도
    //  안전함 (예: "레이아웃 저장" 메뉴, 엔진 종료 직전 자동 저장))
    void Save();

    // Discards whatever layout is currently docked and rebuilds the
    // hand-built default arrangement in `dockspaceId`. Used by the
    // "Reset to Default Layout" menu item.
    // (현재 도킹된 레이아웃을 버리고 `dockspaceId`에 기본 배치를 다시
    //  만듦. "기본 레이아웃으로 초기화" 메뉴에서 사용함)
    void ResetToDefault(ImGuiID dockspaceId);

    // ---------------------------------------------------------------
    // Named layout presets (이름 있는 레이아웃 프리셋)
    // ---------------------------------------------------------------
    // Separate from the single auto-saved Settings/Layout.ini above:
    // this is a *list* of user-named arrangements the person can switch
    // between, shown in the top-level "Layout" menu - intentionally not
    // nested under "Window", since Window only toggles which panels are
    // visible while this controls where they're arranged. Stored as one
    // Settings/Layouts/<name>.ini per preset plus a
    // Settings/Layouts/index.txt listing their names in save order.
    // GetSavedLayouts() ALWAYS returns kDefaultLayoutName ("디폴트") as
    // the first entry, whether or not the person ever explicitly saved
    // it - it's the hand-built arrangement from BuildDefaultLayoutInternal,
    // so there's always at least one preset to fall back to and the menu
    // never needs a separate "Reset to Default" item. (위 자동저장
    // Settings/Layout.ini 하나와는 별개로, 사용자가 이름을 붙여 여러 개를
    // 저장하고 전환할 수 있는 레이아웃 목록임. 최상위 "Layout" 메뉴에
    // 표시되며, 일부러 "Window" 메뉴 아래에 두지 않음 - Window는 패널
    // 표시 여부만, 이건 배치를 다루기 때문. 프리셋마다
    // Settings/Layouts/<이름>.ini 파일 하나, 그리고 저장 순서대로 이름을
    // 나열하는 Settings/Layouts/index.txt로 저장됨.
    // GetSavedLayouts()는 사용자가 직접 저장한 적이 없어도 항상 첫 번째
    // 항목으로 kDefaultLayoutName("디폴트")을 반환함 - 이건
    // BuildDefaultLayoutInternal이 만드는 손수 만든 배치라서, 항상 되돌아갈
    // 프리셋이 최소 하나는 있고 메뉴에 별도의 "기본으로 초기화" 항목이
    // 필요 없어짐)

    // Fixed name of the always-present built-in default preset. Compare
    // against this instead of hardcoding "디폴트" at call sites.
    // (항상 존재하는 내장 기본 프리셋의 고정 이름. 호출하는 쪽에서
    //  "디폴트"를 직접 하드코딩하지 말고 이 값과 비교할 것)
    extern const char* const kDefaultLayoutName;

    // Names of every saved layout preset, in the order they were first
    // saved, with kDefaultLayoutName always injected first (even if it
    // was never explicitly saved). (저장된 레이아웃 프리셋 이름 목록,
    // 처음 저장한 순서대로 - kDefaultLayoutName은 직접 저장한 적이
    // 없어도 항상 맨 앞에 포함됨)
    std::vector<std::string> GetSavedLayouts();

    // Saves the current docking arrangement as a named preset (used by
    // the "Save Layout As..." popup). Adds it to GetSavedLayouts() if
    // `name` is new, or silently overwrites the existing preset with
    // that name otherwise - including kDefaultLayoutName, which lets the
    // person customize what "디폴트" itself opens to. Becomes the
    // "active" layout. (현재 도킹 배치를 이름 있는 프리셋으로 저장함
    // ("레이아웃 저장" 팝업에서 사용). `name`이 새 이름이면 목록에
    // 추가되고, 이미 있는 이름이면 덮어씀 - kDefaultLayoutName도 예외가
    // 아니라서 "디폴트" 자체가 여는 배치를 직접 바꿀 수 있음. 저장 후
    // "활성" 레이아웃이 됨)
    void SaveAs(const std::string& name);

    // Applies a preset by name into `dockspaceId` (used when the person
    // clicks an entry in the "Layout" menu). For kDefaultLayoutName:
    // loads the person's own re-saved snapshot if one exists on disk,
    // otherwise rebuilds the original hand-built arrangement - so
    // "디폴트" never disappears even if it was never saved and even
    // after DeleteNamed(kDefaultLayoutName). For any other name, no-op
    // (with a console warning) if it isn't in GetSavedLayouts() - e.g. it
    // was deleted from another session. (이름으로 저장된 프리셋을
    // `dockspaceId`에 적용함("Layout" 메뉴에서 항목을 클릭했을 때 사용).
    // kDefaultLayoutName인 경우: 디스크에 사용자가 다시 저장한 버전이
    // 있으면 그걸 불러오고, 없으면 손수 만든 원래 기본 배치를 다시 만듦 -
    // 그래서 한 번도 저장한 적이 없거나 DeleteNamed(kDefaultLayoutName)
    // 이후에도 "디폴트"는 절대 사라지지 않음. 그 외 이름은 GetSavedLayouts()
    // 에 없으면(다른 세션에서 삭제된 경우 등) 아무 동작도 하지 않고
    // 콘솔에 경고만 남김)
    void LoadNamed(const std::string& name, ImGuiID dockspaceId);

    // Deletes a saved preset from disk and from GetSavedLayouts() (used
    // by the right-click "Delete" context menu on a Layout menu entry).
    // Does NOT touch whatever is currently docked on screen. Deleting
    // kDefaultLayoutName only discards the person's own re-saved
    // snapshot of it (if any) - the entry itself stays in
    // GetSavedLayouts() and future LoadNamed() calls fall back to the
    // hand-built arrangement again. (저장된 프리셋을 디스크와 목록에서
    // 삭제함(Layout 메뉴 항목의 우클릭 "삭제" 메뉴에서 사용). 현재
    // 화면에 도킹되어 있는 것에는 영향 없음. kDefaultLayoutName을
    // 삭제해도 사용자가 다시 저장해둔 스냅샷만 버려짐 - 항목 자체는
    // GetSavedLayouts()에 계속 남고, 이후 LoadNamed() 호출은 다시 손수
    // 만든 기본 배치로 돌아감)
    void DeleteNamed(const std::string& name);

    // Name of the currently active preset (the one last saved to or
    // loaded from), or an empty string if none is active - e.g. before
    // any preset has ever been saved or loaded this session. Used by
    // the "Layout" menu to put a checkmark next to the active entry.
    // (마지막으로 저장하거나 불러온 프리셋의 이름, 활성 프리셋이 없으면
    // (이번 세션에서 프리셋을 한 번도 저장/로드한 적 없을 때) 빈 문자열.
    // "Layout" 메뉴에서 활성 항목에 체크 표시를 하는 데 사용됨)
    const std::string& GetActiveLayoutName();
}
