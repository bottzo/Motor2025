// dllmain.cpp - Multi-Script DLL System
#include "pch.h"
#include "GameScriptAPI.h"
#include "TestingScript.h"
#include <iostream>
#include <string>

typedef void* ScriptInstanceHandle;

// GLOBAL API POINTER (accessible from all scripts)
ScriptingAPI* g_API = nullptr;

// ==================== DLL ENTRY POINT ====================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        std::cout << "[ScriptDLL] DLL Loaded" << std::endl;
        break;
    case DLL_PROCESS_DETACH:
        std::cout << "[ScriptDLL] DLL Unloaded" << std::endl;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

// ==================== EXPORTED FUNCTIONS ====================

extern "C" __declspec(dllexport) void ScriptSetAPI(ScriptingAPI* api)
{
    g_API = api;
    std::cout << "[ScriptDLL] Engine API connected!" << std::endl;
}

extern "C" __declspec(dllexport) ScriptInstanceHandle CreateScript(GameObjectHandle owner, const char* scriptName)
{
    if (!scriptName)
    {
        std::cout << "[ScriptDLL] ERROR: No script name provided!" << std::endl;
        return nullptr;
    }

    std::string name(scriptName);
    std::cout << "[ScriptDLL] Creating script: " << name << std::endl;

    ScriptBase* script = nullptr;

    // FACTORY: Create the appropriate script based on name
    if (name == "TestingScript")
    {
        script = new TestingScript(owner);
    }

	// Dont delete this comment - it's a placeholder
    // ADD MORE SCRIPTS HERE:
    // else if (name == "MyScript")
    // {
    //     script = new MyScript(owner);
    // }
    else
    {
        std::cout << "[ScriptDLL] ERROR: Unknown script: " << name << std::endl;
        return nullptr;
    }

    return static_cast<ScriptInstanceHandle>(script);
}

extern "C" __declspec(dllexport) void DestroyScript(ScriptInstanceHandle instance)
{
    if (instance)
    {
        ScriptBase* script = static_cast<ScriptBase*>(instance);
        script->CleanUp();
        delete script;
        std::cout << "[ScriptDLL] Script destroyed" << std::endl;
    }
}

extern "C" __declspec(dllexport) void ScriptStart(ScriptInstanceHandle instance)
{
    if (instance)
    {
        ScriptBase* script = static_cast<ScriptBase*>(instance);
        script->Start();
    }
}

extern "C" __declspec(dllexport) void ScriptUpdate(ScriptInstanceHandle instance, float deltaTime)
{
    if (instance)
    {
        ScriptBase* script = static_cast<ScriptBase*>(instance);
        script->Update(deltaTime);
    }
}
