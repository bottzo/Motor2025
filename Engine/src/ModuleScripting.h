#pragma once

#define _HAS_STD_BYTE 0

#include "Module.h"
#include "ScriptingAPI.h"

class ModuleScripting : public Module
{
public:
    ModuleScripting();
    ~ModuleScripting();

    bool Start() override;
    bool Update() override;
    bool CleanUp() override;

    ScriptingAPI* GetEngineAPI() { return &engineAPI; }

private:
    ScriptingAPI engineAPI;
};