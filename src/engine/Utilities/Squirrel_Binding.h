#pragma once

#include <squirrel.h>
#include <sqstdio.h>
#include <sqstdaux.h>
#include "ConsoleLog.h"

// Initialize Squirrel VM and register custom APIs (스쿼럴 VM 초기화 및 커스텀 API 등록)
HSQUIRRELVM InitGeekpizSquirrelVM();

// Clean up Squirrel VM instance (스쿼럴 VM 인스턴스 해제)
void DestroyGeekpizSquirrelVM(HSQUIRRELVM v);