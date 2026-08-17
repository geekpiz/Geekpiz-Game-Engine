#include "Layout.h"

// IMGUI_DEFINE_MATH_OPERATORS must come before imgui_internal.h in at
// least one translation unit that uses it - not needed here, we don't
// do ImVec2 arithmetic, but imgui_internal.h itself is required for the
// DockBuilder* family (they're intentionally not in the public imgui.h).
// (DockBuilder* 계열 함수는 일부러 공개 imgui.h가 아니라 imgui_internal.h에
//  있어서, 여기서만 내부 헤더를 포함함)
#include "imgui_internal.h"
#include "FileSystem.h"
#include "Language/Language.h"
#include "ConsoleLog.h"
#include <algorithm>
#include <sstream>

namespace Layout {

    // True until the first Update() call after a fresh install (no
    // Layout.ini found) builds the default arrangement once. (설치 후
    // 저장된 레이아웃이 없을 때 Update()가 기본 배치를 한 번 만들 때까지 true)
    static bool g_NeedsDefaultLayout = false;

    // Guards against building the default layout more than once even if
    // Update() somehow gets called again before the windows exist yet
    // (첫 프레임에 아직 창이 하나도 안 열렸을 때를 대비한 안전장치)
    static bool g_DefaultLayoutBuilt = false;

    // Name of the currently active named preset (see SaveAs/LoadNamed in
    // Layout.h), empty when none is active. (현재 활성 이름있는 프리셋,
    // 없으면 빈 문자열)
    static std::string g_ActiveLayoutName;

    // Fixed name of the built-in default preset that GetSavedLayouts()
    // always injects first - see the doc comment on kDefaultLayoutName
    // in Layout.h. (GetSavedLayouts()가 항상 맨 앞에 넣는 내장 기본
    // 프리셋의 고정 이름 - Layout.h의 kDefaultLayoutName 문서 참고)
    const char* const kDefaultLayoutName = "디폴트";

    // In-memory cache of Settings/Layouts/index.txt, refreshed lazily via
    // LoadIndex() so GetSavedLayouts() (called every frame by the menu)
    // doesn't hit disk on every call. (index.txt의 메모리 캐시 - 메뉴가
    // 매 프레임 부르는 GetSavedLayouts()가 매번 디스크를 읽지 않도록
    // LoadIndex()에서 지연 로드함)
    static std::vector<std::string> g_SavedLayoutNames;
    static bool g_IndexLoaded = false;

    // Replaces filesystem-unsafe characters so a layout name typed by the
    // user (which may contain Korean text) can't produce an invalid path.
    // (사용자가 입력한 레이아웃 이름(한글 포함 가능)이 잘못된 경로를
    //  만들지 않도록 파일시스템에서 쓸 수 없는 문자를 치환함)
    static std::string SanitizeFileName(const std::string& name)
    {
        static const std::string invalid = "\\/:*?\"<>|";
        std::string out = name;
        for (char& c : out)
        {
            if (invalid.find(c) != std::string::npos) c = '_';
        }
        return out;
    }

    static std::string NamedLayoutPath(const std::string& name)
    {
        return "Settings/Layouts/" + SanitizeFileName(name) + ".ini";
    }

    static void LoadIndex()
    {
        if (g_IndexLoaded) return;
        g_IndexLoaded = true;

        g_SavedLayoutNames.clear();
        std::string index = FS::ReadFile("Settings/Layouts/index.txt");
        std::stringstream ss(index);
        std::string line;
        while (std::getline(ss, line))
        {
            // Strip a trailing \r in case the file was written/edited on
            // Windows (윈도우에서 쓰거나 편집됐을 때 남는 \r 제거)
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) g_SavedLayoutNames.push_back(line);
        }
    }

    static void SaveIndex()
    {
        std::string content;
        for (const std::string& name : g_SavedLayoutNames)
        {
            content += name + "\n";
        }
        FS::WriteFile("Settings/Layouts/index.txt", content);
    }

    void Init()
    {
        ImGuiIO& io = ImGui::GetIO();

        // Take over ini persistence ourselves instead of letting ImGui
        // write "imgui.ini" into whatever the process's current working
        // directory happens to be (which was the build output folder
        // when launched from Visual Studio - see Layout.h). (ImGui가
        // "imgui.ini"를 프로세스 작업 디렉토리에 알아서 쓰게 두지 않고
        // 직접 관리함)
        io.IniFilename = nullptr;

        std::string savedLayout = FS::ReadFile("Settings/Layout.ini");
        if (savedLayout.empty())
        {
            // No saved layout yet (first run, or Layout.ini was deleted) -
            // Update() will build the default docking arrangement on its
            // first call this frame. (저장된 레이아웃이 없음(첫 실행 또는
            // Layout.ini 삭제됨) - Update()가 이번 프레임에서 기본 배치를 만듦)
            g_NeedsDefaultLayout = true;
            return;
        }

        ImGui::LoadIniSettingsFromMemory(savedLayout.c_str(), savedLayout.size());
        ConsoleLog::Append(LogLevel::Info, "Layout loaded from Settings/Layout.ini (저장된 레이아웃을 불러옴)");
    }

    // Builds the hand-authored default arrangement:
    //   +----------+---------------------------+----------+
    //   |          |    Scene / Game / Code     |          |
    //   | Hierarchy|          Editor (tabs)     | Inspector|
    //   |          |    (same height as Left/Right)        |
    //   +----------+---------------------------+----------+
    //   |          Assets / Console (tabs, full width)     |
    //   +----------------------------------------------------+
    // Hierarchy (leftmost) is deliberately the SAME height as the Scene
    // view, not the full window height - Assets/Console spans the full
    // width underneath all three columns instead. That's why the bottom
    // strip is split off the dockspace FIRST, before Left/Right: Left and
    // Right are then carved out of what's left *above* the bottom strip,
    // not out of the whole dockspace. (가장 왼쪽 Hierarchy는 일부러 창
    // 전체 높이가 아니라 씬 뷰와 같은 높이임 - Assets/Console이 세 칼럼
    // 아래로 전체 너비를 차지함. 그래서 하단 스트립을 Left/Right보다
    // 먼저 분리함: Left/Right는 하단 스트립 위쪽에 남은 영역에서
    // 잘라내는 것이지, 전체 도킹스페이스에서 잘라내는 게 아님)
    static void BuildDefaultLayoutInternal(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        ImGuiID mainId = dockspaceId;
        ImGuiID leftId, rightId, bottomId, centerId;

        // Bottom strip for Assets/Console, split off FIRST so it spans the
        // full width (28% of the whole dockspace height) (에셋/콘솔을 위한
        // 하단 스트립 - 전체 너비를 차지하도록 가장 먼저 분리함, 전체
        // 도킹스페이스 높이의 28%)
        bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.28f, nullptr, &mainId);

        // Left strip for Hierarchy (18% width of the remaining top area) (계층 창을 위한 좌측 18%)
        leftId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.18f, nullptr, &mainId);

        // Right strip for Inspector (22% of what's left after Left) (인스펙터를 위한 우측 22%)
        rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.22f, nullptr, &mainId);

        // Whatever remains in the center - same height as Left/Right - is
        // Scene/Game/CodeEditor (남은 중앙 공간(Left/Right와 높이 동일)이
        // 씬/게임/코드 에디터용)
        centerId = mainId;

        // Left (계층)
        ImGui::DockBuilderDockWindow(L::Get("Hierarchy").c_str(), leftId);

        // Right (인스펙터)
        ImGui::DockBuilderDockWindow(L::Get("Inspector").c_str(), rightId);

        // Bottom, tabbed (에셋/콘솔 - 탭으로 묶임)
        ImGui::DockBuilderDockWindow(L::Get("Assets").c_str(), bottomId);
        ImGui::DockBuilderDockWindow(L::Get("Console").c_str(), bottomId);

        // Center, tabbed - Code Editor only cares about its "###CodeEditor"
        // ID segment, matching what Code_Editor.cpp's Begin() call uses
        // (ImGui hashes only the part after "###"). (중앙 - 탭으로 묶임.
        // 코드 에디터는 "###CodeEditor" ID 부분만 중요함 - ImGui는 "###" 뒤
        // 부분만 해시하므로 Code_Editor.cpp의 Begin() 호출과 일치함)
        ImGui::DockBuilderDockWindow(L::Get("Scene").c_str(), centerId);
        ImGui::DockBuilderDockWindow(L::Get("Game").c_str(), centerId);
        ImGui::DockBuilderDockWindow("###CodeEditor", centerId);

        ImGui::DockBuilderFinish(dockspaceId);

        ConsoleLog::Append(LogLevel::Info, "Default layout built (기본 레이아웃을 생성함)");
    }

    void Update(ImGuiID dockspaceId)
    {
        if (!g_NeedsDefaultLayout || g_DefaultLayoutBuilt) return;

        BuildDefaultLayoutInternal(dockspaceId);
        g_DefaultLayoutBuilt = true;
        g_NeedsDefaultLayout = false;

        // Fresh install starts out on the built-in default preset, so the
        // "Layout" menu can put a checkmark next to "디폴트" right away
        // instead of showing no active entry. (신규 설치는 내장 기본
        // 프리셋으로 시작하므로, "Layout" 메뉴가 활성 항목 없음 대신 바로
        // "디폴트"에 체크 표시를 할 수 있게 함)
        g_ActiveLayoutName = kDefaultLayoutName;
    }

    void Save()
    {
        size_t iniSize = 0;
        const char* iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
        if (!iniData || iniSize == 0) return;

        if (FS::WriteFile("Settings/Layout.ini", std::string(iniData, iniSize)))
        {
            ConsoleLog::Append(LogLevel::Info, "Layout saved to Settings/Layout.ini (레이아웃을 저장함)");
        }
        else
        {
            ConsoleLog::Append(LogLevel::Error, "Failed to save layout (레이아웃 저장 실패)");
        }
    }

    std::vector<std::string> GetSavedLayouts()
    {
        LoadIndex();

        // kDefaultLayoutName always comes first, whether or not the
        // person ever explicitly saved it - see the doc comment in
        // Layout.h. (kDefaultLayoutName은 사용자가 직접 저장한 적이
        // 없어도 항상 맨 앞에 옴 - Layout.h 문서 참고)
        std::vector<std::string> result;
        result.push_back(kDefaultLayoutName);
        for (const std::string& name : g_SavedLayoutNames)
        {
            // Avoid listing it twice if the person did explicitly
            // "Save As" over "디폴트" at some point (사용자가 "디폴트"라는
            // 이름으로 직접 저장한 적이 있어도 목록에 두 번 나오지 않도록)
            if (name != kDefaultLayoutName) result.push_back(name);
        }
        return result;
    }

    void SaveAs(const std::string& name)
    {
        if (name.empty()) return;
        LoadIndex();

        size_t iniSize = 0;
        const char* iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
        if (!iniData || iniSize == 0) return;

        if (!FS::WriteFile(NamedLayoutPath(name), std::string(iniData, iniSize)))
        {
            ConsoleLog::Append(LogLevel::Error, "Failed to save layout '" + name + "' (레이아웃 '" + name + "' 저장 실패)");
            return;
        }

        // Only append to the list if this is a brand new name - saving
        // over an existing one shouldn't duplicate its menu entry.
        // (기존에 없던 이름일 때만 목록에 추가함 - 이미 있는 이름에 다시
        //  저장해도 메뉴 항목이 중복되지 않도록)
        if (std::find(g_SavedLayoutNames.begin(), g_SavedLayoutNames.end(), name) == g_SavedLayoutNames.end())
        {
            g_SavedLayoutNames.push_back(name);
            SaveIndex();
        }

        g_ActiveLayoutName = name;
        ConsoleLog::Append(LogLevel::Info, "Layout saved as '" + name + "' (레이아웃을 '" + name + "'(으)로 저장함)");
    }

    void LoadNamed(const std::string& name, ImGuiID dockspaceId)
    {
        if (name == kDefaultLayoutName)
        {
            // Prefer the person's own re-saved snapshot of "디폴트" if one
            // exists on disk, otherwise fall back to rebuilding the
            // original hand-built arrangement - "디폴트" always resolves
            // to *something*, it's never a "not found" case. (사용자가
            // "디폴트"를 직접 다시 저장해둔 스냅샷이 있으면 그걸 우선
            // 쓰고, 없으면 손수 만든 원래 배치를 다시 만듦 - "디폴트"는
            // 항상 무언가로 연결되고, "찾을 수 없음" 상황이 생기지 않음)
            std::string data = FS::ReadFile(NamedLayoutPath(name));
            if (!data.empty())
            {
                ImGui::LoadIniSettingsFromMemory(data.c_str(), data.size());
            }
            else
            {
                BuildDefaultLayoutInternal(dockspaceId);
                g_DefaultLayoutBuilt = true;
            }

            g_ActiveLayoutName = kDefaultLayoutName;
            ConsoleLog::Append(LogLevel::Info, "Layout '디폴트' applied (레이아웃 '디폴트'를 적용함)");
            return;
        }

        std::string data = FS::ReadFile(NamedLayoutPath(name));
        if (data.empty())
        {
            ConsoleLog::Append(LogLevel::Warning, "Saved layout '" + name + "' not found (저장된 레이아웃 '" + name + "'을(를) 찾을 수 없음)");
            return;
        }

        ImGui::LoadIniSettingsFromMemory(data.c_str(), data.size());
        g_ActiveLayoutName = name;
        ConsoleLog::Append(LogLevel::Info, "Layout '" + name + "' loaded (레이아웃 '" + name + "'을(를) 불러옴)");
    }

    void DeleteNamed(const std::string& name)
    {
        LoadIndex();
        auto it = std::find(g_SavedLayoutNames.begin(), g_SavedLayoutNames.end(), name);
        if (it != g_SavedLayoutNames.end())
        {
            g_SavedLayoutNames.erase(it);
            SaveIndex();
        }

        // Harmless no-op if the file was never actually saved (e.g.
        // deleting "디폴트" when it was only ever the built-in in-memory
        // default and never explicitly re-saved). (파일이 실제로 저장된
        // 적이 없으면(예: "디폴트"가 내장 기본값일 뿐 한 번도 다시
        // 저장된 적 없을 때) 아무 영향 없이 조용히 지나감)
        FS::DeleteFile(NamedLayoutPath(name));

        // Currently-docked windows are left untouched - only the saved
        // preset on disk and its checkmark go away. Note "디폴트" itself
        // is NOT removed from GetSavedLayouts() - it's re-injected every
        // call, see the doc comment in Layout.h. (화면에 현재 도킹된
        // 창들은 그대로 둠 - 디스크의 저장된 프리셋과 체크 표시만 사라짐.
        // "디폴트" 자체는 GetSavedLayouts()에서 사라지지 않음 - 매번
        // 다시 채워짐, Layout.h 문서 참고)
        if (g_ActiveLayoutName == name) g_ActiveLayoutName.clear();

        if (name == kDefaultLayoutName)
        {
            ConsoleLog::Append(LogLevel::Info, "'디폴트'의 저장된 스냅샷을 지움 - 다음 적용부터 손수 만든 기본 배치로 돌아감 (Cleared the saved snapshot for '디폴트' - it will rebuild the hand-built default arrangement next time it's applied)");
        }
        else
        {
            ConsoleLog::Append(LogLevel::Info, "Layout '" + name + "' deleted (레이아웃 '" + name + "'을(를) 삭제함)");
        }
    }

    const std::string& GetActiveLayoutName()
    {
        return g_ActiveLayoutName;
    }
}
