#include "ResourceScript.h"
#include "Application.h"
#include "LibraryManager.h"
#include "Log.h"
#include <filesystem>

ResourceScript::ResourceScript(UID uid)
    : Resource(uid, Resource::Type::SCRIPT)
    , scriptDLL(nullptr)
    , CreateScript(nullptr)
    , DestroyScript(nullptr)
    , ScriptStart(nullptr)
    , ScriptUpdate(nullptr)
    , ScriptSetAPI(nullptr)
{
}

ResourceScript::~ResourceScript()
{
    UnloadFromMemory();
}

bool ResourceScript::LoadInMemory()
{
    if (loadedInMemory) return true;

    if (libraryFile.empty())
    {
        LOG_CONSOLE("[ResourceScript] ERROR: No library file path set for UID %llu", uid);
        return false;
    }

    if (!std::filesystem::exists(libraryFile))
    {
        LOG_CONSOLE("[ResourceScript] ERROR: DLL not found: %s", libraryFile.c_str());
        return false;
    }

    LOG_CONSOLE("[ResourceScript] Loading DLL: %s", libraryFile.c_str());

    // Save timestamp for hot-reload detection
    try {
        lastDllWriteTime = std::filesystem::last_write_time(libraryFile);
    }
    catch (...) {
        lastDllWriteTime = std::filesystem::file_time_type::min();
    }

    // Load DLL
    scriptDLL = LoadLibraryA(libraryFile.c_str());

    if (scriptDLL == nullptr)
    {
        DWORD error = GetLastError();
        LOG_CONSOLE("[ResourceScript] ERROR: Failed to load DLL (Error: %d)", error);
        return false;
    }

    // Get function pointers
    CreateScript = (CreateScriptFunc)GetProcAddress(scriptDLL, "CreateScript");
    DestroyScript = (DestroyScriptFunc)GetProcAddress(scriptDLL, "DestroyScript");
    ScriptStart = (ScriptStartFunc)GetProcAddress(scriptDLL, "ScriptStart");
    ScriptUpdate = (ScriptUpdateFunc)GetProcAddress(scriptDLL, "ScriptUpdate");
    ScriptSetAPI = (ScriptSetAPIFunc)GetProcAddress(scriptDLL, "ScriptSetAPI");

    if (!CreateScript || !DestroyScript || !ScriptStart || !ScriptUpdate || !ScriptSetAPI)
    {
        LOG_CONSOLE("[ResourceScript] ERROR: Failed to get function pointers");
        UnloadFromMemory();
        return false;
    }

    // Setup engine API
    SetupAPI();

    loadedInMemory = true;

    // Extract script name from asset file if available
    if (!assetsFile.empty()) {
        std::filesystem::path p(assetsFile);
        scriptName = p.stem().string();
    }

    LOG_CONSOLE("[ResourceScript] Script loaded successfully: %s", scriptName.c_str());
    return true;
}

void ResourceScript::UnloadFromMemory()
{
    if (!loadedInMemory) return;

    if (scriptDLL)
    {
        FreeLibrary(scriptDLL);
        scriptDLL = nullptr;
    }

    CreateScript = nullptr;
    DestroyScript = nullptr;
    ScriptStart = nullptr;
    ScriptUpdate = nullptr;
    ScriptSetAPI = nullptr;

    loadedInMemory = false;

    LOG_CONSOLE("[ResourceScript] Script unloaded: %s", scriptName.c_str());
}

void ResourceScript::SetupAPI()
{
    ScriptingAPI* engineAPI = Application::GetInstance().scripting->GetEngineAPI();
    if (engineAPI && ScriptSetAPI)
    {
        ScriptSetAPI(engineAPI);
    }
}

ScriptInstanceHandle ResourceScript::CreateInstance(GameObjectHandle owner)
{
    if (!loadedInMemory || !CreateScript)
    {
        LOG_CONSOLE("[ResourceScript] ERROR: Cannot create instance - script not loaded");
        return nullptr;
    }

    ScriptInstanceHandle instance = CreateScript(owner);

    if (!instance)
    {
        LOG_CONSOLE("[ResourceScript] ERROR: Failed to create script instance");
    }

    return instance;
}

void ResourceScript::DestroyInstance(ScriptInstanceHandle instance)
{
    if (instance && DestroyScript)
    {
        DestroyScript(instance);
    }
}

void ResourceScript::CallStart(ScriptInstanceHandle instance)
{
    if (instance && ScriptStart)
    {
        ScriptStart(instance);
    }
}

void ResourceScript::CallUpdate(ScriptInstanceHandle instance, float deltaTime)
{
    if (instance && ScriptUpdate)
    {
        ScriptUpdate(instance, deltaTime);
    }
}

bool ResourceScript::HasDLLChanged() const
{
    if (!loadedInMemory || libraryFile.empty()) return false;

    try {
        if (!std::filesystem::exists(libraryFile)) {
            return false;
        }

        auto currentWriteTime = std::filesystem::last_write_time(libraryFile);
        return currentWriteTime != lastDllWriteTime;
    }
    catch (const std::exception& e) {
        LOG_DEBUG("[ResourceScript] Exception checking DLL: %s", e.what());
        return false;
    }
}