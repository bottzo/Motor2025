#pragma once

#include "ModuleResources.h"
#include "ScriptingAPI.h"
#include <windows.h>
#include <string>
#include <filesystem>

// Forward declarations
typedef void* GameObjectHandle;
typedef void* ScriptInstanceHandle;

// Function pointer types
typedef ScriptInstanceHandle(*CreateScriptFunc)(GameObjectHandle owner);
typedef void (*DestroyScriptFunc)(ScriptInstanceHandle instance);
typedef void (*ScriptStartFunc)(ScriptInstanceHandle instance);
typedef void (*ScriptUpdateFunc)(ScriptInstanceHandle instance, float deltaTime);
typedef void (*ScriptSetAPIFunc)(ScriptingAPI* api);

class ResourceScript : public Resource
{
public:
    ResourceScript(UID uid);
    ~ResourceScript() override;

    bool LoadInMemory() override;
    void UnloadFromMemory() override;

    // Create script instance for a GameObject
    ScriptInstanceHandle CreateInstance(GameObjectHandle owner);
    void DestroyInstance(ScriptInstanceHandle instance);

    // Script lifecycle
    void CallStart(ScriptInstanceHandle instance);
    void CallUpdate(ScriptInstanceHandle instance, float deltaTime);

    // Check if DLL has been recompiled (for hot-reload)
    bool HasDLLChanged() const;

    const std::string& GetScriptName() const { return scriptName; }
    bool IsLoaded() const { return scriptDLL != nullptr; }

private:
    void SetupAPI();

    std::string scriptName;
    HMODULE scriptDLL;

    // Function pointers from DLL
    CreateScriptFunc CreateScript;
    DestroyScriptFunc DestroyScript;
    ScriptStartFunc ScriptStart;
    ScriptUpdateFunc ScriptUpdate;
    ScriptSetAPIFunc ScriptSetAPI;

    // For hot-reload detection
    std::filesystem::file_time_type lastDllWriteTime;
};