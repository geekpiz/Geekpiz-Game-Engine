// ScriptComponent.h
//
// ✅ VERIFICATION STATUS: built and run against the real Squirrel
// source you provided - including the generator/cupdate and getkey
// mechanisms below, verified with dedicated test programs (see
// README.md for the exact output).
#pragma once

#include <squirrel.h>
#include <string>
#include <vector>
#include <functional>
#include "ScriptPreprocessor.h"

namespace Geekpiz
{
    // One start-function call that turned out to be a generator (i.e.
    // its body used cupdate()/yield) and is still running. See the
    // long design note above RunStartFunctions() in the .cpp for the
    // full explanation of why/how.
    // (제너레이터로 밝혀진(본문에 cupdate()/yield가 있던) start 함수
    // 호출 중 아직 안 끝난 것 하나. 전체 설명은 .cpp의
    // RunStartFunctions() 위 설계 노트 참고)
    struct RunningCoroutine
    {
        HSQOBJECT generatorRef;
    };

    // A tiny abstraction so this scripting layer doesn't need to
    // depend on GLFW (or any specific windowing library) directly -
    // the engine's main loop supplies a function that answers "is this
    // GLFW keycode currently held down?" for whichever window is
    // active. Keeping this as a callback (instead of, say, #including
    // GLFW/glfw3.h here) means ScriptComponent/ScriptEngine stay
    // testable/compilable without a window or even a display at all -
    // exactly how they were verified in this sandbox.
    // (이 스크립팅 레이어가 GLFW(나 다른 특정 윈도잉 라이브러리)에
    //  직접 의존하지 않도록 만든 작은 추상화 - 엔진의 메인 루프가
    //  "이 GLFW 키코드가 지금 눌려있는지"를 답해주는 함수를 넘겨줌.
    //  (예를 들어 여기서 GLFW/glfw3.h를 직접 #include하는 대신) 콜백
    //  형태로 두면 ScriptComponent/ScriptEngine이 창이나 디스플레이
    //  없이도 계속 컴파일/테스트 가능함 - 실제로 이 샌드박스에서
    //  검증된 방식이 바로 이거임)
    using KeyStateQuery = std::function<bool(int glfwKeyCode)>;

    // Represents one loaded .nut component script. Owns everything
    // needed to run its start/update/getkey functions at the right
    // time.
    // (로드된 .nut 컴포넌트 스크립트 하나를 나타냄. start/update/getkey
    //  함수들을 적절한 시점에 실행하는 데 필요한 모든 것을 갖고 있음)
    class ScriptComponent
    {
    public:
        ScriptComponent(HSQUIRRELVM vm, ScriptLifecycleInfo lifecycle);

        // A ScriptComponent can be holding live HSQOBJECT references
        // (running coroutines) - copying it would double-release them
        // on destruction, so it's non-copyable, same reasoning as
        // ScriptEngine.
        // (ScriptComponent은 살아있는 HSQOBJECT 참조(실행 중인
        //  코루틴)를 들고 있을 수 있음 - 복사하면 소멸 시 이중 해제가
        //  일어나서, ScriptEngine과 같은 이유로 복사 불가로 함)
        ScriptComponent(const ScriptComponent&) = delete;
        ScriptComponent& operator=(const ScriptComponent&) = delete;
        ~ScriptComponent();

        // Calls every "start"-tagged function once, in file order. If a
        // call returns a Squirrel generator (because its body contains
        // cupdate()/yield), the generator is kept and driven forward
        // one step per Tick() from then on - see the .cpp for details.
        // Call this once, right after the component's script loads.
        // ("start"로 태그된 함수들을 파일 순서대로 한 번씩 호출. 호출
        //  결과가 스쿼럴 제너레이터면(본문에 cupdate()/yield가
        //  있었다는 뜻) 그 제너레이터를 보관해뒀다가 그 뒤로 Tick()마다
        //  한 스텝씩 진행시킴 - 자세한 건 .cpp 참고. 컴포넌트 스크립트
        //  로드 직후 한 번만 호출할 것)
        void RunStartFunctions();

        // Meant to be called once per frame by the engine's main loop.
        // Does two independent things every call:
        //   1. advances every still-running start-function-turned-generator
        //      by exactly one yield step (the cupdate()/"repeat forever"
        //      mechanism)
        //   2. calls every "update"-tagged function once (the plain,
        //      always-on per-frame mechanism - unconditional, no
        //      trigger needed)
        // isKeyDown is used for getkey(...) bindings - see CheckKeyBindings.
        // (엔진 메인 루프가 매 프레임 한 번씩 호출하도록 만들어짐. 호출될
        //  때마다 서로 독립된 두 가지 일을 함: 1. 아직 실행 중인
        //  start-함수-였던-제너레이터들을 정확히 한 yield만큼씩 진행시킴
        //  (cupdate()/"무한반복" 메커니즘) 2. "update"로 태그된 함수들을
        //  한 번씩 호출(평범하게 항상 도는 매 프레임 메커니즘 - 트리거
        //  필요 없이 무조건 실행). isKeyDown은 getkey(...) 바인딩에
        //  쓰임 - CheckKeyBindings 참고)
        void Tick(const KeyStateQuery& isKeyDown);

    private:
        void CallNamedFunction(const std::string& name);

        // Runs one function by name and, if it returns a generator,
        // adds it to m_RunningCoroutines instead of discarding it.
        // (이름으로 함수 하나를 실행하고, 결과가 제너레이터면 버리지
        //  않고 m_RunningCoroutines에 추가함)
        void CallStartFunctionAndTrackGenerator(const std::string& name);

        // Advances every entry in m_RunningCoroutines by one step,
        // removing any that finished (or errored - see the .cpp note
        // on why those two cases are treated the same way here).
        // (m_RunningCoroutines의 모든 항목을 한 스텝씩 진행시키고, 끝난
        //  것(또는 에러난 것 - 이 둘을 여기서 같이 취급하는 이유는 .cpp
        //  노트 참고)은 목록에서 제거함)
        void TickRunningCoroutines();

        // For each getkey(...) binding, asks isKeyDown for the key's
        // CURRENT state, compares it against what it was last frame,
        // and calls the bound function if the requested condition
        // (down/up/hold) is met this frame.
        // (getkey(...) 바인딩 각각에 대해 isKeyDown으로 그 키의 "지금"
        //  상태를 물어보고, 지난 프레임 상태랑 비교해서, 요청된
        //  조건(down/up/hold)이 이번 프레임에 맞으면 연결된 함수를
        //  호출함)
        void CheckKeyBindings(const KeyStateQuery& isKeyDown);

        HSQUIRRELVM m_Vm;
        ScriptLifecycleInfo m_Lifecycle;
        std::vector<RunningCoroutine> m_RunningCoroutines;

        // Last-known pressed/released state per key binding, in the
        // same order as m_Lifecycle.keyBindings - needed to detect
        // down/up EDGES (not just current state). Sized/filled in the
        // constructor.
        // (키 바인딩별로 지난 프레임에 눌려있었는지 기록,
        //  m_Lifecycle.keyBindings랑 같은 순서 - 그냥 "지금" 상태가
        //  아니라 down/up "전환 순간"을 감지하려면 필요함. 생성자에서
        //  크기 맞춰서 채움)
        std::vector<bool> m_KeyWasDownLastFrame;
    };
}
