#include "ComponentScript.h"
#include "GameObject.h"
#include "Application.h"
#include "Log.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>

ComponentScript::ComponentScript(GameObject* owner)
    : Component(owner, ComponentType::SCRIPT)
{
    name = "Script";
}

ComponentScript::~ComponentScript()
{
    UnloadScript();
    CleanupTempDLL();
}

void ComponentScript::LoadScript(const std::string& scriptName)
{
    // Unload previous script if any
    UnloadScript();

    this->scriptName = scriptName;

    // Original DLL path
    std::string originalDllPath = "x64\\Debug\\" + scriptName + ".dll";

    LOG_DEBUG("Loading script DLL: %s", originalDllPath.c_str());

    tempDllPath = "x64\\Debug\\" + scriptName + "_temp_" +
        std::to_string(GetCurrentProcessId()) +
        "_" + std::to_string(tempDllCounter++) + ".dll";

    try {
        std::filesystem::copy_file(originalDllPath, tempDllPath,
            std::filesystem::copy_options::overwrite_existing);
        LOG_DEBUG("Created temp DLL: %s", tempDllPath.c_str());
    }
    catch (const std::exception& e) {
        LOG_CONSOLE("ERROR: Failed to copy DLL: %s", e.what());
        scriptLoaded = false;
        return;
    }

    // Guardar la última fecha de modificación del archivo ORIGINAL
    try {
        lastWriteTime = std::filesystem::last_write_time(originalDllPath);
    }
    catch (...) {
        lastWriteTime = std::filesystem::file_time_type::min();
    }

    // Cargar la DLL TEMPORAL (no el original)
    scriptDLL = LoadLibraryA(tempDllPath.c_str());

    if (scriptDLL == nullptr)
    {
        DWORD error = GetLastError();
        LOG_CONSOLE("ERROR: Failed to load %s (Error: %d)", tempDllPath.c_str(), error);
        scriptLoaded = false;
        CleanupTempDLL();
        return;
    }

    // Get function pointers
    CreateScript = (CreateScriptFunc)GetProcAddress(scriptDLL, "CreateScript");
    DestroyScript = (DestroyScriptFunc)GetProcAddress(scriptDLL, "DestroyScript");
    ScriptStart = (ScriptStartFunc)GetProcAddress(scriptDLL, "ScriptStart");
    ScriptUpdate = (ScriptUpdateFunc)GetProcAddress(scriptDLL, "ScriptUpdate");
    ScriptSetAPI = (ScriptSetAPIFunc)GetProcAddress(scriptDLL, "ScriptSetAPI");

    if (!CreateScript || !DestroyScript || !ScriptStart || !ScriptUpdate || !ScriptSetAPI)
    {
        LOG_CONSOLE("ERROR: Failed to get function pointers from %s", scriptName.c_str());
        UnloadScript();
        return;
    }

    // Setup Engine API
    ScriptingAPI* engineAPI = Application::GetInstance().scripting->GetEngineAPI();
    if (engineAPI && ScriptSetAPI)
    {
        ScriptSetAPI(engineAPI);
        LOG_DEBUG("Engine API passed to script '%s'", scriptName.c_str());
    }

    // Create script instance
    scriptInstance = CreateScript(static_cast<GameObjectHandle>(owner));

    if (scriptInstance == nullptr)
    {
        LOG_CONSOLE("ERROR: Failed to create script instance for %s", scriptName.c_str());
        UnloadScript();
        return;
    }

    scriptLoaded = true;
    startCalled = false;

    LOG_CONSOLE(" Script '%s' loaded successfully for '%s'",
        scriptName.c_str(),
        owner->GetName().c_str());
}

void ComponentScript::UnloadScript()
{
    if (scriptInstance && DestroyScript)
    {
        DestroyScript(scriptInstance);
        scriptInstance = nullptr;
    }

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

    scriptLoaded = false;
    startCalled = false;

    CleanupTempDLL();
}

void ComponentScript::CleanupTempDLL()
{
    if (!tempDllPath.empty())
    {
        try {
            if (std::filesystem::exists(tempDllPath))
            {
                std::filesystem::remove(tempDllPath);
                LOG_DEBUG("Cleaned up temp DLL: %s", tempDllPath.c_str());
            }
        }
        catch (...) {
            // Silently ignore cleanup errors
        }
        tempDllPath.clear();
    }
}

void ComponentScript::Update()
{
    if (!active) return;

    // HOT-RELOAD: Verificar si el archivo DLL ORIGINAL ha cambiado
    if (scriptLoaded && CheckForReload())
    {
        // Recargar el script automáticamente
        LOG_CONSOLE(" Hot-Reload: Reloading '%s'...", scriptName.c_str());
        std::string tempName = scriptName;
        LoadScript(tempName);
        return; // Saltar este frame durante la recarga
    }

    if (!scriptLoaded || !scriptInstance) return;

    // Call Start once
    if (!startCalled && ScriptStart)
    {
        ScriptStart(scriptInstance);
        startCalled = true;
    }

    // Call Update every frame
    if (ScriptUpdate)
    {
        float deltaTime = Application::GetInstance().time->GetDeltaTime();
        ScriptUpdate(scriptInstance, deltaTime);
    }
}

bool ComponentScript::CheckForReload()
{
    if (scriptName.empty() || !scriptLoaded) return false;

    std::string originalDllPath = "x64\\Debug\\" + scriptName + ".dll";

    try {
        auto currentWriteTime = std::filesystem::last_write_time(originalDllPath);

        // Si el archivo ORIGINAL ha cambiado
        if (currentWriteTime != lastWriteTime)
        {
            lastWriteTime = currentWriteTime;
            return true; // Necesita recarga
        }
    }
    catch (...) {
        // Error al leer el archivo
    }

    return false;
}

void ComponentScript::OnEditor()
{
    ImGui::Text("Script Component");
    ImGui::Separator();

    ImGui::Text("Script Name:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", scriptName.c_str());

    ImGui::Spacing();

    if (scriptLoaded)
    {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), " Script Loaded");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), " Hot-Reload: Active");
        ImGui::Text("Recompile the DLL to see changes!");

        ImGui::Spacing();

        if (ImGui::Button("Unload Script", ImVec2(-1, 0)))
        {
            UnloadScript();
            LOG_CONSOLE("Script unloaded from '%s'", owner->GetName().c_str());
        }

        if (ImGui::Button("Force Reload", ImVec2(-1, 0)))
        {
            LOG_CONSOLE(" Manual reload requested for '%s'", scriptName.c_str());
            std::string tempName = scriptName;
            LoadScript(tempName);
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), " No Script Loaded");

        // Input para nombre del script
        static char scriptNameBuffer[128] = "Scripting";
        ImGui::InputText("##ScriptName", scriptNameBuffer, sizeof(scriptNameBuffer));

        if (ImGui::Button("Load Script", ImVec2(-1, 0)))
        {
            LoadScript(scriptNameBuffer);
        }
    }
}