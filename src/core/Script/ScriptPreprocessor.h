// ScriptPreprocessor.h
//
// Converts Geekpiz's custom script-attribute syntax into plain Squirrel
// source that the standard Squirrel compiler can compile with ZERO
// modifications to Squirrel itself, and records what was tagged so the
// engine knows what to do with each function later.
//
// (긱피즈의 커스텀 스크립트 속성 문법을, 스쿼럴 본체를 전혀 건드리지
//  않고도 표준 스쿼럴 컴파일러가 그대로 컴파일할 수 있는 평범한
//  스쿼럴 소스로 바꿔주고, 나중에 엔진이 각 함수를 가지고 뭘 할지
//  판단할 수 있도록 태그된 내용을 기록해주는 전처리기)
//
// THREE ATTRIBUTES HANDLED HERE (여기서 처리하는 세 가지 속성):
//
// 1. `start function NAME()` - run once, right after load. Several
//    allowed per script.
// 2. `update function NAME()` - run automatically every frame, from
//    the frame after start functions finish, no trigger needed
//    ("일반 업데이트").
// 3. `getkey(KEY, MODE) function NAME()` - run when a keyboard
//    condition is met (KEY down this frame / up this frame / held).
//
// SEPARATELY (as of this version), any `cupdate()` call anywhere in
// the source is rewritten to Squirrel's native `yield` statement -
// see the long comment above ReplaceCupdateCalls below for why this
// is a totally different mechanism from "update" and what it's for.
#pragma once

#include <string>
#include <vector>

namespace Geekpiz
{
    // One `getkey(...)` binding: which key, which trigger mode, and
    // which function to call when the condition is met.
    // (getkey(...) 바인딩 하나: 어떤 키, 어떤 트리거 모드, 조건이
    //  맞을 때 부를 함수 이름)
    struct KeyBinding
    {
        // Either a friendly name ("LEFT", "SPACE", "A"...) looked up in
        // ScriptEngine's key-name table, or a decimal GLFW keycode
        // written directly (e.g. "263") for anything not in the table
        // yet - see ScriptEngine::ResolveKeyName.
        // (친숙한 이름("LEFT","SPACE","A"...)이거나, 아직 표에 없는
        //  키는 GLFW 키코드 숫자를 그냥 적어도 됨(예: "263") -
        //  ScriptEngine::ResolveKeyName 참고)
        std::string keyName;

        // One of "down" (pressed this frame), "up" (released this
        // frame), "hold" (currently held, fires every frame it's down).
        // (down(이번 프레임에 눌림)/up(이번 프레임에 뗌)/hold(누르고
        //  있는 동안 매 프레임) 중 하나)
        std::string mode;

        std::string functionName;
    };

    // Holds everything found while preprocessing one script. The engine
    // uses this to know which functions exist and when to call them -
    // it never has to re-scan the source text again.
    // (전처리 중 발견된 모든 정보를 담는 구조체. 엔진은 이것만 보고
    //  어떤 함수가 있고 언제 불러야 하는지 판단하며, 소스 텍스트를
    //  다시 스캔할 필요가 없음)
    struct ScriptLifecycleInfo
    {
        // "start"-tagged functions - called once each, in file order,
        // right after the script loads (see ScriptComponent::RunStartFunctions).
        // If a start function's body contains a cupdate() call (which
        // becomes `yield` - see below), calling it produces a Squirrel
        // GENERATOR instead of running to completion immediately; the
        // engine detects this automatically and drives it forward one
        // step per frame from then on. You don't have to declare that
        // anywhere - it falls out of whether the (preprocessed) body
        // contains a yield or not.
        // (start로 태그된 함수들 - 스크립트 로드 직후 파일에 나온
        //  순서대로 각각 한 번씩 호출됨(ScriptComponent::RunStartFunctions
        //  참고). start 함수 본문에 cupdate() 호출이(아래에서 `yield`로
        //  바뀜) 있으면, 그 함수를 호출한 결과가 즉시 끝까지 실행되는
        //  대신 스쿼럴 제너레이터가 됨 - 엔진이 이걸 자동으로 감지해서
        //  그 뒤로 매 프레임 한 단계씩 진행시킴. 어디 따로 선언할 필요
        //  없이, (전처리된) 본문에 yield가 있는지 없는지에 따라
        //  자연스럽게 결정됨)
        std::vector<std::string> startFunctionNames;

        // "update"-tagged functions - plain, ordinary per-frame update.
        // Runs automatically every Tick() once start functions have
        // been run - no trigger, no cupdate() needed. This is a
        // SEPARATE mechanism from cupdate()/yield below; see the note
        // on cupdate for why they're not the same thing.
        // (update로 태그된 함수들 - 평범한 일반 업데이트. start 함수들이
        //  실행된 뒤부터 매 Tick()마다 자동으로 실행됨 - 트리거도,
        //  cupdate()도 필요 없음. 아래 cupdate()/yield와는 완전히
        //  별개의 메커니즘 - 왜 둘이 다른 건지는 cupdate 관련 설명 참고)
        std::vector<std::string> updateFunctionNames;

        // "getkey(...)"-tagged functions - see KeyBinding above.
        std::vector<KeyBinding> keyBindings;
    };

    // Rewrites "start function NAME(...)", "update function NAME(...)"
    // and "getkey(KEY, MODE) function NAME(...)" into plain
    // "function NAME(...)", AND rewrites every `cupdate()` call
    // (anywhere in the file) into the Squirrel keyword `yield`.
    // Preserves every other character exactly (including indentation on
    // rewritten lines), so Squirrel compiler error line numbers still
    // line up with the ORIGINAL file the user is looking at.
    //
    // WHY cupdate() IS SEPARATE FROM update (왜 cupdate가 update랑
    // 다른가):
    // `update` is a plain per-frame callback - like Unity's Update().
    // `cupdate()` is something else entirely: it's how a `start`
    // function's own SEQUENTIAL code (the kind Entry/Scratch blocks
    // produce - "move 10 steps, repeat forever { turn 20, move 5 }")
    // spreads a loop across multiple frames instead of running it all
    // in zero time (which would just freeze the program - an infinite
    // loop with no yield never returns control to the engine at all).
    // Squirrel already has a mechanism built exactly for this:
    // generators (the `yield` keyword) - a function containing `yield`
    // doesn't run when called, it returns a paused "generator" object
    // that the engine resumes one step at a time. So rather than
    // inventing our own coroutine machinery, `cupdate()` is simply
    // rewritten to `yield` here, and ScriptComponent (see its header
    // comment) detects when a start function's call returned a
    // generator and drives it forward once per frame automatically.
    // Written directly inside a start function, e.g.:
    //     start function main() {
    //         move(10)
    //         while (true) { turn(20); move(5); cupdate() }
    //     }
    // this does exactly what "이동10, 무한반복{20돌기,5이동}" describes.
    //
    // (update는 그냥 매 프레임 불리는 콜백 - 유니티의 Update()랑
    //  똑같음. cupdate()는 완전히 다른 것: start 함수 "자체"의 순차
    //  코드(엔트리/스크래치 블록이 만들어내는 "10만큼 움직이기,
    //  무한 반복하기{20도 돌기, 5만큼 움직이기}" 같은 것)가 루프를
    //  한 프레임 안에 다 돌아버리는 게 아니라(그러면 그냥 프로그램이
    //  멈춤 - yield 없는 무한루프는 엔진한테 제어권을 절대 안 돌려줌)
    //  여러 프레임에 걸쳐 나눠 돌게 해주는 방법. 스쿼럴엔 정확히 이걸
    //  위한 기능이 이미 있음: 제너레이터(`yield` 키워드) - yield가
    //  들어있는 함수는 호출해도 바로 실행되는 게 아니라 멈춰있는
    //  "제너레이터" 객체를 반환하고, 엔진이 그걸 한 스텝씩 진행시킴.
    //  그래서 코루틴을 직접 구현하는 대신, cupdate()를 여기서 그냥
    //  `yield`로 바꿔치기하고, ScriptComponent가(헤더 주석 참고) start
    //  함수 호출 결과가 제너레이터인지 감지해서 매 프레임 자동으로
    //  한 단계씩 진행시킴)
    //
    // LIMITATION (알아둘 한계, start/update와 동일):
    // Simple line/text-based scanning, not a full Squirrel lexer - see
    // the original limitation note this file used to have. Also:
    // cupdate() written inside a HELPER function (one that a start
    // function calls, rather than being written directly in the start
    // function's own body) becomes a *nested* generator that the
    // top-level driver here does NOT automatically resume - keeping
    // loops with cupdate() directly inside the start function itself
    // is the supported pattern for now.
    // (단순 줄/텍스트 스캔이지 완전한 스쿼럴 렉서 아님 - 기존 한계
    //  설명과 동일. 추가로: cupdate()를 start 함수가 "직접" 호출하는
    //  헬퍼 함수 안에 넣으면 중첩 제너레이터가 되는데, 지금 버전의
    //  드라이버는 이걸 자동으로 진행시켜주지 않음 - 지금은 cupdate()가
    //  들어간 루프를 start 함수 본문에 직접 쓰는 패턴만 지원함)
    std::string PreprocessLifecycleAttributes(
        const std::string& source,
        ScriptLifecycleInfo& outInfo);
}
