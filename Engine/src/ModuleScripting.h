#pragma once

#define _HAS_STD_BYTE 0

#include "Module.h"
#include <windows.h>

typedef void (*ScriptStartFunc)();
typedef void (*ScriptUpdateFunc)(float);
typedef void (*ScriptCleanUpFunc)();

class ModuleScripting : public Module
{
public:
    ModuleScripting();
    ~ModuleScripting();

    bool Start() override;
    bool Update() override;
    bool CleanUp() override;

private:
    HMODULE scriptDLL;              // Handle loaded DLL
    ScriptStartFunc ScriptStart;    // pointer to the Start function of the script
    ScriptUpdateFunc ScriptUpdate;  // pointer to the Update function of the script
    ScriptCleanUpFunc ScriptCleanUp; // pointer to the CleanUp ..
    bool scriptStartCalled;
};
