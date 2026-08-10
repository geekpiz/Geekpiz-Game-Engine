// ScriptComponent.cpp
#include "ScriptComponent.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>

namespace Geekpiz
{
    namespace
    {
        // Small friendly-name -> GLFW keycode table, plus a fallback
        // that accepts a raw decimal keycode string (e.g. "263") for
        // anything not listed here yet, and single letters/digits
        // computed directly (GLFW mirrors ASCII for those: 'A'-'Z' are
        // 65-90, '0'-'9' are 48-57 - confirmed against the actual
        // glfw3.h in the project's libs/glfw).
        // (친숙한 이름 -> GLFW 키코드로 바꿔주는 작은 표. 여기 없는
        //  건 10진수 키코드 문자열(예: "263")을 그대로 받는 걸로
        //  폴백하고, 한 글자 알파벳/숫자는 직접 계산함(GLFW가
        //  ASCII를 그대로 따라감: 'A'-'Z'는 65-90, '0'-'9'는 48-57 -
        //  프로젝트의 libs/glfw 안 실제 glfw3.h로 확인함))
        //
        // EXTEND THIS as more named keys are needed - it's a plain
        // if-chain on purpose, so adding one is a one-line change.
        // (이름 있는 키가 더 필요해지면 여기를 늘리면 됨 - 하나
        //  추가하는 게 한 줄로 끝나게 일부러 평범한 if 체인으로 만듦)
        int ResolveKeyName(const std::string& rawName)
        {
            std::string name = rawName;
            for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            if (name == "LEFT") return 263;
            if (name == "RIGHT") return 262;
            if (name == "UP") return 265;
            if (name == "DOWN") return 264;
            if (name == "SPACE") return 32;
            if (name == "ENTER") return 257;
            if (name == "ESCAPE") return 256;
            if (name == "LEFT_SHIFT" || name == "SHIFT") return 340;

            if (name.size() == 1)
            {
                char c = name[0];
                if (c >= 'A' && c <= 'Z') return 65 + (c - 'A');
                if (c >= '0' && c <= '9') return 48 + (c - '0');
            }

            // Fallback: treat it as a raw numeric GLFW keycode written
            // directly, e.g. getkey(263, down).
            // (폴백: getkey(263, down)처럼 GLFW 키코드를 숫자로 직접
            //  적은 것으로 취급)
            if (!rawName.empty() && (std::isdigit(static_cast<unsigned char>(rawName[0])) || rawName[0] == '-'))
                return std::atoi(rawName.c_str());

            fprintf(stderr, "[Geekpiz] unknown key name in getkey(): %s (add it to ResolveKeyName)\n", rawName.c_str());
            return -1; // never matches any real key - safe no-op (실제 키와 절대 안 맞음 - 안전한 무동작)
        }
    }

    ScriptComponent::ScriptComponent(HSQUIRRELVM vm, ScriptLifecycleInfo lifecycle)
        : m_Vm(vm)
        , m_Lifecycle(std::move(lifecycle))
        , m_KeyWasDownLastFrame(m_Lifecycle.keyBindings.size(), false)
    {
    }

    ScriptComponent::~ScriptComponent()
    {
        // Release every still-running coroutine's generator reference -
        // otherwise we'd leak a Squirrel refcount for each one still
        // pending when this component is destroyed.
        // (아직 실행 중인 코루틴들의 제너레이터 참조를 전부 해제 -
        //  안 그러면 이 컴포넌트가 파괴될 때 남아있던 것마다 스쿼럴
        //  레퍼런스 카운트가 샘)
        for (RunningCoroutine& coroutine : m_RunningCoroutines)
        {
            sq_release(m_Vm, &coroutine.generatorRef);
        }
    }

    void ScriptComponent::RunStartFunctions()
    {
        for (const std::string& name : m_Lifecycle.startFunctionNames)
        {
            CallStartFunctionAndTrackGenerator(name);
        }
    }

    // DESIGN NOTE (설계 노트):
    // We call every start function with retval=SQTrue (unlike a plain
    // "update" call, where we don't care about the return value) so we
    // can inspect what came back. If the function's preprocessed body
    // contained a cupdate() (-> yield), Squirrel doesn't run the body
    // at all when called - it immediately returns a generator object
    // in a "not started" state; the FIRST resume() is what actually
    // runs from the top up to the first yield. Confirmed empirically:
    // see the gen_test verification referenced in README.md. So the
    // very first bit of a cupdate()-using start function's code
    // doesn't actually execute until Tick() is called at least once -
    // if that first bit needs to run in the SAME frame the component
    // loads, call Tick() once right after RunStartFunctions().
    // (모든 start 함수를 retval=SQTrue로 호출함(그냥 update 호출과
    //  달리 반환값에 관심 없는 게 아니라, 뭐가 왔는지 확인해야 하니까).
    //  전처리된 본문에 cupdate()가(-> yield) 있었다면, 스쿼럴은 호출
    //  시점에 본문을 아예 실행하지 않고 "아직 시작 안 한" 상태의
    //  제너레이터 객체를 바로 반환함 - 실제로 처음부터 첫 yield까지
    //  실행하는 건 "첫 resume()" 호출임. gen_test로 실제 검증함(자세한
    //  건 README.md 참고). 그래서 cupdate()를 쓰는 start 함수의 맨 첫
    //  부분조차 RunStartFunctions() 시점에는 실행 안 되고 Tick()이 최소
    //  한 번은 불려야 실행됨 - 컴포넌트가 로드된 "그 프레임"에 첫 부분이
    //  바로 실행돼야 한다면, RunStartFunctions() 직후에 Tick()을 한 번
    //  더 불러주면 됨)
    void ScriptComponent::CallStartFunctionAndTrackGenerator(const std::string& name)
    {
        sq_pushroottable(m_Vm);                 // stack: [root]
        sq_pushstring(m_Vm, name.c_str(), -1);   // stack: [root, "name"]

        if (SQ_FAILED(sq_get(m_Vm, -2)))         // stack on success: [root, function]
        {
            sq_pop(m_Vm, 1);                     // stack: []
            fprintf(stderr, "[Geekpiz] start function not found: %s\n", name.c_str());
            return;
        }

        sq_remove(m_Vm, -2);                     // stack: [function]
        sq_pushroottable(m_Vm);                  // stack: [function, this]

        if (SQ_FAILED(sq_call(m_Vm, 1, SQTrue, SQTrue))) // retval=SQTrue - we need to inspect it
        {
            fprintf(stderr, "[Geekpiz] error while running start function: %s\n", name.c_str());
            sq_pop(m_Vm, 1); // pop leftover closure -> stack: []
            return;
        }
        // stack: [leftover closure, returnValue]

        if (sq_gettype(m_Vm, -1) == OT_GENERATOR)
        {
            HSQOBJECT genRef;
            sq_getstackobj(m_Vm, -1, &genRef);
            sq_addref(m_Vm, &genRef); // keep it alive across frames (프레임 넘어서도 살아있게 참조 카운트 올림)
            m_RunningCoroutines.push_back(RunningCoroutine{genRef});
        }
        // else: it was a plain function, already ran to completion above - nothing more to do
        // (그게 아니면 그냥 평범한 함수였고 위에서 이미 끝까지 실행됨 - 더 할 일 없음)

        sq_pop(m_Vm, 2); // pop returnValue + leftover closure -> stack: []
    }

    void ScriptComponent::TickRunningCoroutines()
    {
        for (size_t i = 0; i < m_RunningCoroutines.size(); )
        {
            sq_pushobject(m_Vm, m_RunningCoroutines[i].generatorRef); // stack: [generator]
            SQRESULT r = sq_resume(m_Vm, SQFalse, SQTrue);

            // CORRECTED after a 2000-tick stress test crashed with
            // "target>=0 && target<=255" deep inside Squirrel's own
            // SQGenerator::Resume (sqobject.cpp) - that assertion is
            // about how far the resume destination sits above the
            // current call frame's base, and it was creeping up by
            // exactly 1 extra stack slot every single Tick(). Reading
            // the real sq_resume() source explains why: on SUCCESS, it
            // internally pops its own temporary result slot when
            // retval=SQFalse - but NOT the generator object *we*
            // pushed to seed the call, so that's still 1 leftover value
            // we owe a pop. On FAILURE, sq_resume returns EARLY
            // (before it even reaches that internal pop), so BOTH the
            // generator we pushed AND its internal temporary are still
            // there - 2 leftover values. Confirmed by re-running the
            // same 2000-tick stress test after this fix with zero
            // crashes (see README.md).
            // (2000틱 스트레스 테스트에서 스쿼럴 자기 자신의
            //  SQGenerator::Resume(sqobject.cpp) 안 "target>=0 &&
            //  target<=255" 어서션이 터지면서 발견함 - 이 어서션은
            //  resume 결과를 쓸 위치가 현재 콜 프레임 base로부터 얼마나
            //  떨어져 있는지에 관한 건데, Tick()마다 정확히 스택 슬롯
            //  1개씩 계속 쌓이고 있었음. 실제 sq_resume() 소스를 읽어보니
            //  이유가 나옴: "성공" 시엔 retval=SQFalse일 때 내부적으로
            //  자기가 만든 임시 결과 슬롯은 팝해주는데, "우리가" 호출을
            //  위해 push한 제너레이터 객체는 안 팝해줘서 그건 우리가
            //  팝해야 함(1개). "실패" 시엔 sq_resume이 그 내부 팝에
            //  도달하기도 전에 일찍 리턴해버려서, 우리가 push한
            //  제너레이터랑 내부 임시 슬롯 둘 다 남음(2개). 이 수정
            //  후 같은 2000틱 스트레스 테스트를 다시 돌려서 한 번도
            //  안 죽는 것까지 확인함(README.md 참고))
            if (SQ_FAILED(r))
            {
                sq_pop(m_Vm, 2); // generator + sq_resume's internal temp, both leftover on the failure path
                sq_release(m_Vm, &m_RunningCoroutines[i].generatorRef);
                m_RunningCoroutines.erase(m_RunningCoroutines.begin() + static_cast<long>(i));
                // don't advance i - the next element just shifted into this slot
            }
            else
            {
                sq_pop(m_Vm, 1); // just the generator reference we pushed - sq_resume already popped its own temp
                ++i;
            }
        }
    }

    void ScriptComponent::CheckKeyBindings(const KeyStateQuery& isKeyDown)
    {
        for (size_t i = 0; i < m_Lifecycle.keyBindings.size(); ++i)
        {
            const KeyBinding& binding = m_Lifecycle.keyBindings[i];
            int glfwCode = ResolveKeyName(binding.keyName);

            bool isDown = isKeyDown ? isKeyDown(glfwCode) : false;
            bool wasDown = m_KeyWasDownLastFrame[i];
            m_KeyWasDownLastFrame[i] = isDown;

            bool shouldFire = false;
            if (binding.mode == "down") shouldFire = isDown && !wasDown;
            else if (binding.mode == "up") shouldFire = !isDown && wasDown;
            else if (binding.mode == "hold") shouldFire = isDown;
            else fprintf(stderr, "[Geekpiz] unknown getkey mode '%s' for %s (expected down/up/hold)\n",
                         binding.mode.c_str(), binding.functionName.c_str());

            if (shouldFire)
                CallNamedFunction(binding.functionName);
        }
    }

    void ScriptComponent::Tick(const KeyStateQuery& isKeyDown)
    {
        TickRunningCoroutines();

        for (const std::string& name : m_Lifecycle.updateFunctionNames)
        {
            CallNamedFunction(name);
        }

        CheckKeyBindings(isKeyDown);
    }

    void ScriptComponent::CallNamedFunction(const std::string& name)
    {
        sq_pushroottable(m_Vm);                 // stack: [root]
        sq_pushstring(m_Vm, name.c_str(), -1);   // stack: [root, "name"]

        if (SQ_FAILED(sq_get(m_Vm, -2)))         // stack on success: [root, function]
        {
            sq_pop(m_Vm, 1);                     // stack: []
            fprintf(stderr, "[Geekpiz] lifecycle function not found: %s\n", name.c_str());
            return;
        }

        sq_remove(m_Vm, -2);                     // stack: [function]
        sq_pushroottable(m_Vm);                  // stack: [function, this]

        if (SQ_FAILED(sq_call(m_Vm, 1, SQFalse, SQTrue)))
        {
            fprintf(stderr, "[Geekpiz] error while running: %s\n", name.c_str());
        }
        // sq_call only pops `params` (just "this" here) - the closure
        // itself is left on the stack either way (verified against the
        // real sqapi.cpp - see README.md).
        // (sq_call은 `params`만(여기선 "this" 하나) 팝하고, 클로저
        //  자체는 성공/실패 상관없이 스택에 남음(실제 sqapi.cpp로
        //  검증함 - README.md 참고))
        sq_pop(m_Vm, 1); // pop the leftover function/closure -> stack: []
    }
}
