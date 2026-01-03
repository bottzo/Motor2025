#pragma once
#include "Component.h"
#include "ScriptingAPI.h"
#include <string>
#include <windows.h>
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

class ComponentScript : public Component
{
public:
    ComponentScript(GameObject* owner);
    ~ComponentScript() override;

    void Update() override;
    void OnEditor() override;

    void LoadScript(const std::string& scriptName);
    void UnloadScript();

    const std::string& GetScriptName() const { return scriptName; }
    bool IsScriptLoaded() const { return scriptLoaded; }

private:
    bool CheckForReload();
    void CleanupTempDLL();

    std::string scriptName = "None";
    bool scriptLoaded = false;
    bool startCalled = false;

    // Hot-reload support
    std::string tempDllPath;
    std::filesystem::file_time_type lastWriteTime;
    static inline int tempDllCounter = 0;

    // DLL management
    HMODULE scriptDLL = nullptr;
    ScriptInstanceHandle scriptInstance = nullptr;

    // Function pointers
    CreateScriptFunc CreateScript = nullptr;
    DestroyScriptFunc DestroyScript = nullptr;
    ScriptStartFunc ScriptStart = nullptr;
    ScriptUpdateFunc ScriptUpdate = nullptr;
    ScriptSetAPIFunc ScriptSetAPI = nullptr;
};