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
typedef ScriptInstanceHandle(*CreateScriptFunc)(GameObjectHandle owner, const char* scriptName);
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

    // Serialization
    void Serialize(nlohmann::json& componentObj) const override;
    void Deserialize(const nlohmann::json& componentObj) override;

    void LoadScript(const std::string& dllName, const std::string& scriptClassName);
    void UnloadScript();

    const std::string& GetDllName() const { return dllName; }
    const std::string& GetScriptClassName() const { return scriptClassName; }
    bool IsScriptLoaded() const { return scriptLoaded; }
    bool IsMarkedForRemoval() const { return markedForRemoval; }

    // Scripts getters
    static std::vector<std::string> GetAvailableScripts();
    static std::vector<std::string> GetScriptClassesInDLL(const std::string& dllName);

    // Build system
    static bool BuildScriptingProject();

    static void ImportBuiltScriptsToLibrary();

private:
    bool CheckForDllChange();
    void CleanupTempDLL();

    std::string dllName = "None";
    std::string scriptClassName = "None";
    bool scriptLoaded = false;
    bool startCalled = false;
    bool markedForRemoval = false;

    // Hot-reload support
    std::string tempDllPath;
    std::filesystem::file_time_type lastDllWriteTime;

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