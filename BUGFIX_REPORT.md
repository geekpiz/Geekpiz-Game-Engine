# Geekpiz Game Engine — Code Editor 버그 수정 리포트

`Code_Editor.cpp`와 그게 직접 의존하는 파일들(`ScriptPreprocessor`, `ConsoleLog`,
`FileSystem`, `Squirrel_Binding`, `ImGuiColorTextEdit/TextEditor`)을 전부
훑어봤어. `TextEditor.cpp`의 `mTextChanged`가 절대 `false`로 안 돌아가던 문제랑
`ScriptPreprocessor`, `ConsoleLog`의 로그 도배 문제는 이미 예전에 고쳐져 있길래
그건 그대로 뒀고, 이번에 새로 찾아서 고친 것들만 정리했어.

## 1. 스쿼럴 VM(`g_ScriptVM`)이 절대 해제 안 되던 문제
**파일:** `src/engine/Render/Window/Code_Editor.cpp`, `src/Main.cpp`

`g_ScriptVM`은 문법 검사(`CheckSyntaxFull`)나 F5(`CompileAndRunScript`)를 처음
누를 때 `InitGeekpizSquirrelVM()`으로 생성되는데, `DestroyGeekpizSquirrelVM()`을
호출하는 곳이 프로젝트 전체에 단 한 군데도 없었어. `Squirrel_Binding.cpp`에
해제 함수는 이미 있었는데 아무도 안 불렀던 거야. 프로그램이 켜져 있는 동안
VM과 그 안의 모든 스쿼럴 객체가 계속 메모리에 남아있었던 셈.

- `Window::ShutdownCodeEditor()` 함수를 새로 추가하고, `Main.cpp`의 종료 처리
  구간(`ImGui_ImplOpenGL3_Shutdown()` 바로 위)에서 한 번 호출하도록 연결함.

## 2. 콘솔 에러/경고 카운터가 로그 정리 후에도 계속 늘어나기만 하던 문제
**파일:** `src/engine/Utilities/ConsoleLog.cpp`

로그가 5000줄을 넘으면 오래된 25%를 지우는 로직은 있었는데, 지워지는 로그들의
개수를 `g_InfoCount`/`g_WarningCount`/`g_ErrorCount`에서 빼주질 않았어. 코드
에디터가 키 입력마다 `CheckSyntaxFull()`을 돌리기 때문에, 문법 에러가 있는 채로
오래 타이핑하면 로그가 금방 5000줄을 넘고, 그 순간부터 콘솔 창의
"Errors: N" 카운터가 실제 로그 개수랑 안 맞고 영원히 커지기만 하는 버그였음.

- 트림할 때 지워지는 각 항목의 `count`(중복 합산값)를 레벨별로 정확히 빼주도록
  수정함.

## 3. `FileSystem.cpp`에 `<sstream>` 헤더 누락
**파일:** `src/engine/Utilities/FileSystem.cpp`

`ReadFile()`이 `std::stringstream`을 쓰는데 `#include <sstream>`이 없었어.
지금까지는 `<fstream>`이 내부적으로 `<sstream>`을 우연히 끌고 와서 컴파일이
됐던 것뿐이고, 이건 표준이 보장하는 동작이 아니야. Visual Studio 2026(MSVC)
STL 버전이 바뀌면 이 부분에서 컴파일이 깨질 수 있는 잠재적 버그였음 - 지금
바로 확실하게 고쳐둠.

- `#include <sstream>` 명시적으로 추가.
- 덤으로 같은 파일에 있던 `#if defined(__apple__)` 오타도 고침(실제 매크로는
  대문자로 시작하는 `__APPLE__`). 지금은 Windows 빌드만 하니까 당장 체감되는
  버그는 아니지만, 원래 의도대로면 macOS 분기가 절대 실행이 안 되는
  상태였어서 같이 정리함.

## 4. 폰트 스케일을 전역 공유 객체에 직접 써서 위험했던 부분
**파일:** `src/engine/Render/Window/Code_Editor.cpp`

에디터 폰트 크기를 바꿀 때 `ImGui::GetFont()->Scale`을 직접 수정하고 있었어.
문제는 이 `ImFont` 객체가 앱 전체가 공유하는 객체라는 것 - 수정한 다음 원래
값으로 복구하는 코드가 항상 실행된다는 보장이 있어야만 안전한 방식이었음
(만약 나중에 이 사이에 early return이라도 하나 추가되면 앱의 다른 모든 창
폰트 크기가 코드 에디터 스케일로 영구히 고정돼버림).

- ImGui가 정확히 이 용도로 제공하는 `ImGui::SetWindowFontScale()`로 교체함.
  창 단위로만 적용되고 프레임이 끝나면 자동으로 원상복구되니까 수동 복구
  코드(`originalScale` 저장 → `PopFont` → 복구)가 통째로 필요 없어짐.

## 5. 설정 팝업 창이 접혀있어도 내용물을 계속 그리던 문제
**파일:** `src/engine/Render/Window/Code_Editor.cpp`

`EditorSettings` 팝업만 `ImGui::Begin()`의 반환값을 확인 안 하고 있었어(같은
파일의 메인 코드 에디터 창은 제대로 확인하고 있었음). 창이 접혀있어도
슬라이더/체크박스/버튼이 매 프레임 계속 빌드됐다는 뜻 - 8GB RAM / 1GB VRAM
환경에서는 이런 불필요한 작업도 누적되면 체감될 수 있어서 같이 정리함.

- `if (ImGui::Begin(...)) { ... }`로 감싸서 창이 접혀있을 땐 내용물을 안
  그리게 수정함.

---

이번에 손댄 파일은 이 4개뿐이야:
- `src/engine/Render/Window/Code_Editor.cpp`
- `src/engine/Render/Window/Window.h`
- `src/engine/Utilities/ConsoleLog.cpp`
- `src/engine/Utilities/FileSystem.cpp`
- `src/Main.cpp` (VM 종료 호출 연결)

각 수정 부분에는 "BUG FIX" 주석(영어+한글)으로 뭐가 문제였는지 남겨뒀으니까
나중에 다시 봐도 왜 고쳤는지 바로 알아볼 수 있을 거야.
