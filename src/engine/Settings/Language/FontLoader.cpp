#include "FontLoader.h"
#include "../../Utilities/FileSystem.h"
#include "../../Utilities/ConsoleLog.h"
#include <vector>
#include <cstring>
#include <cstdint>

namespace FontLoader {

    // Simplified load implementation with single-line call (단일 줄 호출을 위한 단축 로드 구현)
    ImFont* Load(const std::wstring& fontName, float fontSize) {
        ImGuiIO& io = ImGui::GetIO(); // Get ImGuiIO instance (ImGuiIO 인스턴스 가져오기)
        return Load(io, fontName, fontSize, nullptr, nullptr); // Let the detailed overload pick a sane default range (상세 오버로드가 적절한 기본 범위를 선택하도록 함)
    }

    ImFont* Load(const std::string& fontName, float fontSize) {
        std::wstring wFontName(fontName.begin(), fontName.end()); // Convert string to wstring (string을 wstring으로 변환)
        return Load(wFontName, fontSize);
    }

    // Detailed font load implementation (상세 폰트 로드 구현)
    ImFont* Load(ImGuiIO& io, const std::wstring& fontName, float fontSize, const ImWchar* glyphRanges, ImFontConfig* fontConfig) {
        // Convert wstring font name to relative path string (wstring 폰트 이름을 상대 경로 string으로 변환)
        std::string fileName(fontName.begin(), fontName.end());

        // Target relative file path candidates for languages and store mods (언어 및 스토어 모드를 위한 대상 상대 경로 후보들)
        std::vector<std::string> targetPaths = {
            "Settings/Language/Fonts/" + fileName,
            "Settings/Language/Fonts/" + fileName + ".ttf",
            "Settings/Language/Fonts/" + fileName + ".otf"
        };

        std::string fontData = "";

        // Read binary font data using custom file system (커스텀 파일 시스템을 사용하여 바이너리 폰트 데이터 읽기)
        for (const auto& path : targetPaths) {
            fontData = FS::ReadFile(path);
            if (!fontData.empty()) {
                break;
            }
        }

        // Return null and log why if font file is missing, instead of failing silently
        // (폰트 파일이 없으면 null 반환 + 이유를 로그로 남김. 예전엔 그냥 조용히 실패해서 원인 파악이 불가능했음)
        if (fontData.empty()) {
            ConsoleLog::Append(LogLevel::Error,
                "Font not found: Settings/Language/Fonts/" + fileName +
                " (AppData\\Geekpiz\\Game Engine\\Settings\\Language\\Fonts 폴더에 해당 폰트 파일이 있는지 확인해줘)");
            return nullptr;
        }

        // Allocate memory buffer for ImGui font loader (ImGui 폰트 로더를 위한 메모리 버퍼 할당)
        void* bufferCopy = IM_ALLOC(fontData.size());
        std::memcpy(bufferCopy, fontData.data(), fontData.size());

        // Full-Unicode glyph range (surrogate gap only excluded) for the store's user-uploaded fonts/language packs
        // (스토어에 사용자가 직접 올리는 폰트/언어팩을 위한 전체 유니코드 범위 — 서로게이트 영역만 제외)
        //
        // This project lets users upload their OWN font + language pack to the store, so the loader can't assume
        // "Korean + Latin only" (that was my mistake in the previous fix) — it has to be ready for any language
        // AND emoji, since we don't control what a given uploaded font/language pack will contain.
        // (이 프로젝트는 사용자가 직접 폰트 + 언어팩을 스토어에 올리는 구조라서, 로더가 "한글+라틴만"이라고
        //  가정하면 안 됨(이전 수정에서 이 부분을 잘못 좁혀놨었음) — 어떤 언어나 이모지가 올라올지 알 수
        //  없으니 전체 범위를 지원해야 함.)
        //
        // Practically this costs nothing extra at runtime here: this project's OpenGL3 backend enables
        // ImGuiBackendFlags_RendererHasTextures (dynamic font atlas), and under that mode ImGui only
        // rasterizes/uploads a glyph the first time it's actually drawn — it does NOT pre-bake every codepoint
        // in this range up front. GlyphRanges only gets fully pre-baked on the legacy path (used when the
        // backend does NOT support dynamic textures), so declaring the full range here is just cheap insurance
        // for that fallback case, not a real memory/VRAM cost on the normal path.
        // (실제로는 별도 비용이 거의 없음: 이 프로젝트의 OpenGL3 백엔드는 ImGuiBackendFlags_RendererHasTextures
        //  (동적 폰트 아틀라스)를 켜고 있어서, 이 모드에서는 실제로 화면에 그려지는 글리프만 그때그때
        //  래스터화/업로드함 — 이 범위 안의 코드포인트를 전부 미리 굽지 않음. GlyphRanges가 통째로
        //  미리 구워지는 건 백엔드가 동적 텍스처를 지원하지 않는 레거시 경로일 때뿐이라, 여기서 전체
        //  범위를 선언해두는 건 그 대비용 안전장치일 뿐 평소 경로에서는 실질적인 메모리/VRAM 비용이 없음.)
        static const ImWchar universalRange[] = {
            0x0001, 0xD7FF,    // Basic Latin through Hangul Syllables, i.e. everything before the surrogate gap (서로게이트 영역 직전까지 전체)
            0xE000, 0x10FFFF,  // Private Use Area through the end of Unicode — covers remaining CJK, symbols, and all emoji/supplementary planes (사설 대역부터 유니코드 끝까지 — 나머지 CJK, 기호, 이모지/보조 평면 전부 포함)
            0,
        };

        const ImWchar* finalRanges = glyphRanges ? glyphRanges : universalRange;

        // Add font to ImGui memory atlas and return ImFont pointer (ImGui 메모리 아틀라스에 폰트 추가 및 ImFont 포인터 반환)
        ImFont* font = io.Fonts->AddFontFromMemoryTTF(
            bufferCopy,
            static_cast<int>(fontData.size()),
            fontSize,
            fontConfig,
            finalRanges
        );

        if (!font) {
            ConsoleLog::Append(LogLevel::Error, "ImGui rejected font data while adding to atlas: " + fileName);
        }

        return font;
    }

}