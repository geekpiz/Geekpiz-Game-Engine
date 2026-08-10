cmake_minimum_required(VERSION 3.10)

project("Geekpiz Game Engine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable hot reloading for the MSVC compiler, if supported (지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다)
# NOTE: this MUST come after project(), since the MSVC variable / compiler ID isn't detected until project() runs (이 플래그는 project() 선언 이후에 와야 MSVC 변수를 정상적으로 인식합니다)
if (MSVC)
    # Avoid "program database could not be updated" (C2471) when multiple MSVC compile processes access the same PDB during parallel builds by adding /FS. (다중 컴파일 시 PDB 접근 충돌을 방지하기 위해 /FS 추가)
    add_compile_options(/FS)

    # Force MSVC to treat source AND string literals as UTF-8 (소스와 문자열 리터럴을 UTF-8로 강제)
    add_compile_options(/utf-8)
endif()

# Define engine source files as a variable (엔진 소스 파일들을 변수로 지정)
set(ENGINE_SOURCES
    "src/Main.cpp" 
    "src/Main.h" 
    "src/engine/Render/render_editer.cpp" 
    "src/engine/Render/Render.h"
    "src/engine/Render/Window/Inspector.cpp"
    "src/engine/Render/Window/Scene.cpp"
    "src/engine/Render/Window/Game.cpp"
    "src/engine/Render/Window/Hierarchy.cpp"
    "src/engine/Render/Window/Assets.cpp"
    "src/engine/Render/Window/Console.cpp"
    "src/engine/Render/Window/Window.h"
    "src/engine/Render/topbar.cpp"
    "src/engine/Render/ImGuiStyle.cpp"
    "src/engine/Settings/Settings.cpp"
    "src/engine/Settings/Settings.h"
    "src/engine/Settings/Language/Language.cpp"
    "src/engine/Settings/Language/Language.h"
    "src/engine/Utilities/FileSystem.h"
    "src/engine/Utilities/FileSystem.cpp"
    "src/engine/Render/Window/Code_Editor.cpp"
    "src/engine/Utilities/ConsoleLog.cpp"
    "src/engine/Utilities/ConsoleLog.h"
    "src/engine/Settings/Language/FontLoader.h"
    "src/engine/Settings/Language/FontLoader.cpp"
    "libs/ImGuiColorTextEdit/TextEditor.h"
    "libs/ImGuiColorTextEdit/TextEditor.cpp"
    "src/core/Script/ScriptPreprocessor.h"
    "src/core/Script/ScriptPreprocessor.cpp"
    "src/core/Script/ScriptComponent.h"
    "src/core/Script/ScriptComponent.cpp"
    "src/core/Script/ScriptEngine.h"
    "src/core/Script/ScriptEngine.cpp"
)

# Add the source code to this project's executable file (이 프로젝트의 실행 파일에 소스를 추가합니다)
add_executable("Geekpiz_Editor_1.0" ${ENGINE_SOURCES} "src/engine/Utilities/Squirrel_Binding.h" "src/engine/Utilities/Squirrel_Binding.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET "Geekpiz_Editor_1.0" PROPERTY CXX_STANDARD 20)
endif()

# Define the source files for ImGui as variables (imgui 소스 파일들을 변수로 지정)
set(IMGUI_SOURCES 
    "libs/ImGui/imgui.cpp" 
    "libs/ImGui/imgui_draw.cpp" 
    "libs/ImGui/imgui_tables.cpp" 
    "libs/ImGui/imgui_widgets.cpp"
    "libs/ImGui/backends/imgui_impl_glfw.cpp"
    "libs/ImGui/backends/imgui_impl_opengl3.cpp"
)

# Creating the ImGui Library (imgui 라이브러리 만들기)
add_library(imgui ${IMGUI_SOURCES} "src/engine/Utilities/Squirrel_Binding.h" "src/engine/Utilities/Squirrel_Binding.cpp")

# Tell me where the header file is located for imgui (imgui를 위한 헤더 파일 위치 알려주기)
target_include_directories(imgui PRIVATE
    "libs"
    "libs/ImGui"
    "libs/ImGui/backends"
    "libs/glfw/include"
    "libs/squirrel/include"
)

file(GLOB_RECURSE SQUIRREL_SOURCES 
    "libs/squirrel/*.cpp"
    "libs/squirrel/*.c"
)

# Create Squirrel Static Library (스쿼럴 정적 라이브러리 생성)
add_library(squirrel STATIC ${SQUIRREL_SOURCES} "src/engine/Utilities/Squirrel_Binding.h" "src/engine/Utilities/Squirrel_Binding.cpp")

# Tell me where the header file is located for squirrel (스쿼럴을 위한 헤더 파일 위치 알려주기)
target_include_directories(squirrel PUBLIC
    "libs/squirrel"
    "libs/squirrel/include"
)

# Tell me where the header file is located for engine (엔진을 위한 헤더 파일 위치 알려주기)
target_include_directories("Geekpiz_Editor_1.0" PRIVATE 
    "src"
    "libs"
    "libs/ImGui"
    "libs/ImGui/backends"
    "libs/glfw/include"
    "libs/squirrel"
    "libs/squirrel/include"
    "src/engine/Settings"
    "src/engine/Utilities"
    "src/core"
)

# Add subdirectory for glfw (glfw 하위 디렉토리 추가)
add_subdirectory("libs/glfw")

# Find OpenGL package (OpenGL 패키지 찾기)
find_package(OpenGL REQUIRED)

# Link libraries to the executable (실행 파일에 라이브러리 링크하기)
target_link_libraries("Geekpiz_Editor_1.0"
    PRIVATE
        imgui
        glfw
        squirrel  # Added squirrel library link (스쿼럴 라이브러리 링크 추가)
        OpenGL::GL
)

# Set Windows specific properties (Windows 전용 속성 설정)
if(WIN32)
    set_target_properties("Geekpiz_Editor_1.0" PROPERTIES 
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()