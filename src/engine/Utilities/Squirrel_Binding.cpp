#include "Squirrel_Binding.h"

// C++ Implementation of print.Log (print.Log 네이티브 C++ 구현)
SQInteger SQ_Print_Log(HSQUIRRELVM v)
{
    const SQChar* msg;
    if (SQ_SUCCEEDED(sq_getstring(v, 2, &msg))) {
        ConsoleLog::Append(LogLevel::Info, std::string(msg));
    }
    return 0;
}

// C++ Implementation of print.Warning (print.Warning 네이티브 C++ 구현)
SQInteger SQ_Print_Warning(HSQUIRRELVM v)
{
    const SQChar* msg;
    if (SQ_SUCCEEDED(sq_getstring(v, 2, &msg))) {
        ConsoleLog::Append(LogLevel::Warning, std::string(msg));
    }
    return 0;
}

// C++ Implementation of print.Error (print.Error 네이티브 C++ 구현)
SQInteger SQ_Print_Error(HSQUIRRELVM v)
{
    const SQChar* msg;
    if (SQ_SUCCEEDED(sq_getstring(v, 2, &msg))) {
        ConsoleLog::Append(LogLevel::Error, std::string(msg));
    }
    return 0;
}

// Initialize Squirrel VM and bind print table (스쿼럴 VM 생성 및 print 테이블 바인딩)
HSQUIRRELVM InitGeekpizSquirrelVM()
{
    HSQUIRRELVM v = sq_open(1024); // Open VM with initial stack size 1024 (스택 크기 1024로 VM 생성)

    // Register 'print' table to root table (루트 테이블에 'print' 테이블 등록)
    sq_pushroottable(v);
    sq_pushstring(v, _SC("print"), -1);
    sq_newtable(v);

    // Register print.Log (print.Log 함수 슬롯 추가)
    sq_pushstring(v, _SC("Log"), -1);
    sq_newclosure(v, SQ_Print_Log, 0);
    sq_newslot(v, -3, SQFalse);

    // Register print.Warning (print.Warning 함수 슬롯 추가)
    sq_pushstring(v, _SC("Warning"), -1);
    sq_newclosure(v, SQ_Print_Warning, 0);
    sq_newslot(v, -3, SQFalse);

    // Register print.Error (print.Error 함수 슬롯 추가)
    sq_pushstring(v, _SC("Error"), -1);
    sq_newclosure(v, SQ_Print_Error, 0);
    sq_newslot(v, -3, SQFalse);

    // Bind 'print' table to root table (루트 테이블에 print 테이블 추가 마무리)
    sq_newslot(v, -3, SQFalse);
    sq_pop(v, 1); // Pop root table (루트 테이블 스택에서 제거)

    return v;
}

// Close Squirrel VM (스쿼럴 VM 종료)
void DestroyGeekpizSquirrelVM(HSQUIRRELVM v)
{
    if (v) {
        sq_close(v);
    }
}