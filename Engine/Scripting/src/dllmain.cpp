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
    else
    {
        std::cout << "[ScriptDLL] ERROR: Unknown script: " << name << std::endl;
        return nullptr;
    }

    if (script)
    {
        script->RegisterProperties();
        std::cout << "[ScriptDLL] Properties registered for: " << name << std::endl;
    }

    return static_cast<ScriptInstanceHandle>(script);
}

extern "C" __declspec(dllexport) int GetPropertyCount(ScriptInstanceHandle instance)
{
    if (!instance) return 0;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    return static_cast<int>(script->GetProperties().size());
}

extern "C" __declspec(dllexport) const char* GetPropertyName(ScriptInstanceHandle instance, int index)
{
    if (!instance) return "";
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return "";
    return props[index].name.c_str();
}

extern "C" __declspec(dllexport) int GetPropertyType(ScriptInstanceHandle instance, int index)
{
    if (!instance) return -1;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return -1;
    return static_cast<int>(props[index].type);
}

extern "C" __declspec(dllexport) float GetPropertyFloat(ScriptInstanceHandle instance, int index)
{
    if (!instance) return 0.0f;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return 0.0f;
    if (props[index].type != PropertyType::FLOAT) return 0.0f;
    return *static_cast<float*>(props[index].dataPtr);
}

extern "C" __declspec(dllexport) void SetPropertyFloat(ScriptInstanceHandle instance, int index, float value)
{
    if (!instance) return;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return;
    if (props[index].type != PropertyType::FLOAT) return;
    *static_cast<float*>(props[index].dataPtr) = value;
}

extern "C" __declspec(dllexport) int GetPropertyInt(ScriptInstanceHandle instance, int index)
{
    if (!instance) return 0;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return 0;
    if (props[index].type != PropertyType::INT) return 0;
    return *static_cast<int*>(props[index].dataPtr);
}

extern "C" __declspec(dllexport) void SetPropertyInt(ScriptInstanceHandle instance, int index, int value)
{
    if (!instance) return;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return;
    if (props[index].type != PropertyType::INT) return;
    *static_cast<int*>(props[index].dataPtr) = value;
}

extern "C" __declspec(dllexport) bool GetPropertyBool(ScriptInstanceHandle instance, int index)
{
    if (!instance) return false;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return false;
    if (props[index].type != PropertyType::BOOL) return false;
    return *static_cast<bool*>(props[index].dataPtr);
}

extern "C" __declspec(dllexport) void SetPropertyBool(ScriptInstanceHandle instance, int index, bool value)
{
    if (!instance) return;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return;
    if (props[index].type != PropertyType::BOOL) return;
    *static_cast<bool*>(props[index].dataPtr) = value;
}

extern "C" __declspec(dllexport) void GetPropertyVec3(ScriptInstanceHandle instance, int index, float* x, float* y, float* z)
{
    if (!instance || !x || !y || !z) return;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return;
    if (props[index].type != PropertyType::VEC3) return;
    Vec3* vec = static_cast<Vec3*>(props[index].dataPtr);
    *x = vec->x;
    *y = vec->y;
    *z = vec->z;
}

extern "C" __declspec(dllexport) void SetPropertyVec3(ScriptInstanceHandle instance, int index, float x, float y, float z)
{
    if (!instance) return;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return;
    if (props[index].type != PropertyType::VEC3) return;
    Vec3* vec = static_cast<Vec3*>(props[index].dataPtr);
    vec->x = x;
    vec->y = y;
    vec->z = z;
}

extern "C" __declspec(dllexport) float GetPropertyMin(ScriptInstanceHandle instance, int index)
{
    if (!instance) return 0.0f;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return 0.0f;
    return props[index].minValue;
}

extern "C" __declspec(dllexport) float GetPropertyMax(ScriptInstanceHandle instance, int index)
{
    if (!instance) return 100.0f;
    ScriptBase* script = static_cast<ScriptBase*>(instance);
    const auto& props = script->GetProperties();
    if (index < 0 || index >= props.size()) return 100.0f;
    return props[index].maxValue;
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


