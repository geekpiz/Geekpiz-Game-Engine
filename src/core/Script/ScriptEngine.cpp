// ScriptEngine.cpp
#include "ScriptEngine.h"
#include "ScriptPreprocessor.h"
#include <cstdio>
#include <cstdarg>
#include <fstream>
#include <sstream>

namespace Geekpiz
{
    void ScriptEngine::SquirrelPrintCallback(HSQUIRRELVM /*v*/, const SQChar* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    void ScriptEngine::SquirrelErrorCallback(HSQUIRRELVM /*v*/, const SQChar* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
    }

    ScriptEngine::ScriptEngine()
    {
        // 1024 is the VM's INITIAL stack size in slots, not bytes, and
        // not a hard cap - Squirrel grows it automatically if a script
        // needs more.
        m_Vm = sq_open(1024);
        sq_setprintfunc(m_Vm, SquirrelPrintCallback, SquirrelErrorCallback);
        // No native functions to register anymore - cupdate() is
        // handled entirely by the preprocessor now (-> yield). See
        // ScriptPreprocessor.h.
    }

    ScriptEngine::~ScriptEngine()
    {
        if (m_Vm)
        {
            sq_close(m_Vm);
            m_Vm = nullptr;
        }
    }

    std::unique_ptr<ScriptComponent> ScriptEngine::LoadComponent(const std::string& nutFilePath)
    {
        std::ifstream file(nutFilePath, std::ios::binary);
        if (!file)
        {
            fprintf(stderr, "[Geekpiz] could not open script: %s\n", nutFilePath.c_str());
            return nullptr;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string rawSource = buffer.str();

        ScriptLifecycleInfo lifecycle;
        const std::string processedSource = PreprocessLifecycleAttributes(rawSource, lifecycle);

        sq_pushroottable(m_Vm); // stack: [root]  (compile target below AND "this" for running it)

        if (SQ_FAILED(sq_compilebuffer(
                m_Vm,
                processedSource.c_str(),
                static_cast<SQInteger>(processedSource.size()),
                nutFilePath.c_str(),
                SQTrue)))
        {
            // Compile errors are printed by Squirrel itself via
            // SquirrelErrorCallback (raiseerror=SQTrue above).
            sq_pop(m_Vm, 1); // pop root table -> stack: []
            return nullptr;
        }
        // stack: [root, compiledClosure]

        sq_push(m_Vm, -2); // duplicate root onto top, to use as "this" -> stack: [root, compiledClosure, root]

        // sq_call(v, 1, ...) only pops the 1 "this" it was given - the
        // compiledClosure below it is untouched and still on the stack
        // afterward either way (verified against the real sqapi.cpp).
        if (SQ_FAILED(sq_call(m_Vm, 1, SQFalse, SQTrue)))
        {
            sq_pop(m_Vm, 2); // stack: []  (compiledClosure + root, both leftover)
            return nullptr;
        }
        // stack: [root, compiledClosure] - neither needed anymore, the
        // functions we care about are already registered as globals.
        sq_pop(m_Vm, 2); // stack: []

        return std::make_unique<ScriptComponent>(m_Vm, std::move(lifecycle));
    }
}
