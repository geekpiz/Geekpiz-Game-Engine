// ScriptEngine.h
//
// ✅ VERIFICATION STATUS: built and run against the real Squirrel
// source you provided - including the generator/cupdate and getkey
// mechanisms, verified with dedicated test programs. See README.md.
#pragma once

#include <squirrel.h>
#include <string>
#include <memory>
#include "ScriptComponent.h"

namespace Geekpiz
{
    // Owns exactly one Squirrel VM and knows how to turn a .nut file on
    // disk into a ready-to-run ScriptComponent (preprocess -> compile ->
    // run top-level code -> wrap the result).
    //
    // NOTE: as of this version, there is no native function to register
    // for cupdate() anymore - it's rewritten to Squirrel's own `yield`
    // keyword entirely at the preprocessing stage (see
    // ScriptPreprocessor.h), so there's nothing for the engine to bind.
    // ScriptComponent detects the resulting generator automatically.
    // (참고: 이번 버전부터는 cupdate()를 위해 등록할 네이티브 함수가
    //  없음 - 전처리 단계에서 스쿼럴 자체의 `yield` 키워드로 완전히
    //  바뀌어버려서(ScriptPreprocessor.h 참고) 엔진이 따로 바인딩할 게
    //  없음. 그 결과로 생기는 제너레이터는 ScriptComponent가 자동으로
    //  감지함)
    //
    // ONE VM FOR NOW (지금은 VM 하나만) - unchanged reasoning from
    // before: simplest, cheapest memory-wise given the 200MB/400MB
    // targets. See git history / previous README revision if curious.
    class ScriptEngine
    {
    public:
        ScriptEngine();
        ~ScriptEngine();

        ScriptEngine(const ScriptEngine&) = delete;
        ScriptEngine& operator=(const ScriptEngine&) = delete;

        // Reads `nutFilePath`, runs it through PreprocessLifecycleAttributes,
        // compiles and executes the result on this engine's VM (which is
        // what makes its top-level start/update/getkey functions become
        // callable globals), and returns a ScriptComponent wrapping it.
        // Returns nullptr (and logs to stderr) if the file couldn't be
        // read or failed to compile/run - callers should check for that.
        std::unique_ptr<ScriptComponent> LoadComponent(const std::string& nutFilePath);

        HSQUIRRELVM GetVm() const { return m_Vm; }

    private:
        static void SquirrelPrintCallback(HSQUIRRELVM v, const SQChar* fmt, ...);
        static void SquirrelErrorCallback(HSQUIRRELVM v, const SQChar* fmt, ...);

        HSQUIRRELVM m_Vm = nullptr;
    };
}
