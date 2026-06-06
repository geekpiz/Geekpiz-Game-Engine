cmake_minimum_required (VERSION 3.10)

# Enable hot reloading for the MSVC compiler, if supported.(지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.)
if (MSVC)
  # Avoid "program database could not be updated" (C2471) when multiple MSVC
  # compile processes access the same PDB during parallel builds by adding /FS.
  # See: https://learn.microsoft.com/en-us/cpp/build/reference/fs-force-synchronous-pdb-writes
  add_compile_options(/FS)
endif()

project ("Geekpiz Game Engine")

# Add the source code to this project's executable file.(이 프로젝트의 실행 파일에 소스를 추가합니다.)
add_executable ("Geekpiz_Editor_1.0"
    "src/Main.cpp" 
    "src/Main.h" 
    "src/engine/Render/render_editer.cpp" 
    "src/engine/Render/Render.h"
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET "Geekpiz_Editor_1.0" PROPERTY CXX_STANDARD 20)
endif()

# TODO: If necessary, add tests and install the target.(필요한 경우 테스트를 추가하고 대상을 설치합니다.)

# Define the source files for Imgui as variables(imgui 소스 파일들을 변수로 지정)
set(IMGUI_SOURCES 
    libs/ImGui/imgui.cpp 
    libs/ImGui/imgui_draw.cpp 
    libs/ImGui/imgui_tables.cpp 
    libs/ImGui/imgui_widgets.cpp
    libs/ImGui/backends/imgui_impl_glfw.cpp
    libs/ImGui/backends/imgui_impl_opengl3.cpp
)

# Creating the Im Gui Library(imgui 라이브러리 만들기)
add_library(imgui ${IMGUI_SOURCES} )

target_include_directories(imgui PRIVATE
    libs/ImGui
    libs/ImGui/backends
    libs/glfw/include
)

# Tell me where the header file is located(헤더 파일 위치 알려주기)
target_include_directories("Geekpiz_Editor_1.0" PRIVATE libs/ImGui)

add_subdirectory(libs/glfw)

find_package(OpenGL REQUIRED)

target_link_libraries(Geekpiz_Editor_1.0
    PRIVATE
        imgui
        glfw
        OpenGL::GL
)

target_include_directories("Geekpiz_Editor_1.0" PRIVATE 
    src 
    libs/ImGui 
    libs/ImGui/backends
    libs/glfw/include
)

if(WIN32)
    set_target_properties(Geekpiz_Editor_1.0 PROPERTIES 
        LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()