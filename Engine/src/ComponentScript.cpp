#include "ComponentScript.h"
#include "GameObject.h"
#include "Application.h"
#include "Log.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

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
    UnloadScript();

    this->scriptName = scriptName;
    std::string originalDllPath = "x64\\Release\\" + scriptName + ".dll";

    LOG_CONSOLE("Loading DLL: %s", originalDllPath.c_str());

    if (!std::filesystem::exists(originalDllPath))
    {
        LOG_CONSOLE("ERROR: DLL not found: %s", originalDllPath.c_str());
        scriptLoaded = false;
        return;
    }

    // Guardar el timestamp del DLL para detectar recompilaciones
    try {
        lastDllWriteTime = std::filesystem::last_write_time(originalDllPath);
    }
    catch (...) {
        lastDllWriteTime = std::filesystem::file_time_type::min();
    }

    // Crear copia temporal (esto es liada ahora mismo y se debe borrar en futuro porque esta ya pasado)
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    tempDllPath = "x64\\Release\\" + scriptName + "_temp_" + std::to_string(timestamp) + ".dll";
    std::string tempPdbPath = "x64\\Release\\" + scriptName + "_temp_" + std::to_string(timestamp) + ".pdb";
    std::string originalPdbPath = "x64\\Release\\" + scriptName + ".pdb";

    // esto tambien habra que borrarlo se hizo cuando no funcionaba el hot reloading
    // Copiar DLL Y PDB con espera adecuada
    try {
        // Esperar a que el compilador termine completamente
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Intentar multiples veces en caso de que el archivo esté bloqueado
        int maxRetries = 10;
        bool success = false;

        for (int retry = 0; retry < maxRetries && !success; retry++)
        {
            if (retry > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }

            try {
                // Copiar DLL
                std::filesystem::copy_file(originalDllPath, tempDllPath,
                    std::filesystem::copy_options::overwrite_existing);

                // Copiar PDB si existe (puede no existir en Release)
                if (std::filesystem::exists(originalPdbPath)) {
                    std::filesystem::copy_file(originalPdbPath, tempPdbPath,
                        std::filesystem::copy_options::overwrite_existing);
                }

                success = true;
            }
            catch (const std::exception& e) {
                if (retry == maxRetries - 1) {
                    throw;
                }
                LOG_DEBUG("Copy attempt %d failed, retrying...", retry + 1);
            }
        }
    }
    catch (const std::exception& e) {
        LOG_CONSOLE("ERROR: Failed to copy DLL: %s", e.what());
        scriptLoaded = false;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Cargar DLL temporal
    scriptDLL = LoadLibraryA(tempDllPath.c_str());

    if (scriptDLL == nullptr)
    {
        DWORD error = GetLastError();
        LOG_CONSOLE("ERROR: Failed to load DLL (Error: %d)", error);
        scriptLoaded = false;
        CleanupTempDLL();
        return;
    }

    // Obtener funciones
    CreateScript = (CreateScriptFunc)GetProcAddress(scriptDLL, "CreateScript");
    DestroyScript = (DestroyScriptFunc)GetProcAddress(scriptDLL, "DestroyScript");
    ScriptStart = (ScriptStartFunc)GetProcAddress(scriptDLL, "ScriptStart");
    ScriptUpdate = (ScriptUpdateFunc)GetProcAddress(scriptDLL, "ScriptUpdate");
    ScriptSetAPI = (ScriptSetAPIFunc)GetProcAddress(scriptDLL, "ScriptSetAPI");

    if (!CreateScript || !DestroyScript || !ScriptStart || !ScriptUpdate || !ScriptSetAPI)
    {
        LOG_CONSOLE("ERROR: Failed to get function pointers");
        UnloadScript();
        return;
    }

    // Pasar API del engine
    ScriptingAPI* engineAPI = Application::GetInstance().scripting->GetEngineAPI();
    if (engineAPI && ScriptSetAPI)
    {
        ScriptSetAPI(engineAPI);
    }

    // Crear instancia del script
    scriptInstance = CreateScript(static_cast<GameObjectHandle>(owner));

    if (scriptInstance == nullptr)
    {
        LOG_CONSOLE("ERROR: Failed to create script instance");
        UnloadScript();
        return;
    }

    scriptLoaded = true;
    startCalled = false;

    LOG_CONSOLE("Script '%s' loaded!", scriptName.c_str());
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
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
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
        // Limpiar tanto el DLL como el PDB
        std::string tempPdbPath = tempDllPath;
        size_t pos = tempPdbPath.find(".dll");
        if (pos != std::string::npos) {
            tempPdbPath.replace(pos, 4, ".pdb");
        }

        for (int i = 0; i < 5; i++)
        {
            try {
                if (std::filesystem::exists(tempDllPath))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::filesystem::remove(tempDllPath);
                }
                if (std::filesystem::exists(tempPdbPath))
                {
                    std::filesystem::remove(tempPdbPath);
                }
                break;
            }
            catch (...) {}
        }
        tempDllPath.clear();
    }
}

void ComponentScript::Update()
{
    if (!active) return;

    // Solo detecta cuando recompilas manualmente el DLL
    if (scriptLoaded && CheckForDllChange())
    {
        LOG_CONSOLE("DLL RECOMPILED DETECTED!");
        LOG_CONSOLE("Reloading script...");

        std::string tempName = scriptName;
        LoadScript(tempName);
        return;
    }

    if (!scriptLoaded || !scriptInstance) return;

    // Llamar Start una vez
    if (!startCalled && ScriptStart)
    {
        ScriptStart(scriptInstance);
        startCalled = true;
    }

    // Llamar Update cada frame
    if (ScriptUpdate)
    {
        float deltaTime = Application::GetInstance().time->GetDeltaTime();
        ScriptUpdate(scriptInstance, deltaTime);
    }
}

bool ComponentScript::CheckForDllChange()
{
    if (scriptName.empty() || !scriptLoaded) return false;

    std::string originalDllPath = "x64\\Release\\" + scriptName + ".dll";

    try {
        if (!std::filesystem::exists(originalDllPath)) {
            LOG_DEBUG("DLL not found: %s", originalDllPath.c_str());
            return false;
        }

        auto currentWriteTime = std::filesystem::last_write_time(originalDllPath);

        // Si el DLL cambió (fue recompilado)
        if (currentWriteTime != lastDllWriteTime)
        {
            LOG_CONSOLE(" DLL timestamp changed!");
            // Esperar a que termine de escribirse
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return true;
        }
    }
    catch (const std::exception& e) {
        LOG_DEBUG("Exception checking DLL: %s", e.what());
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
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Hot-Reload: Active");
        ImGui::TextWrapped("Recompile the DLL (rebuild Scripting( and it will auto-reload");

        ImGui::Spacing();

        if (ImGui::Button("Unload Script", ImVec2(-1, 0)))
        {
            UnloadScript();
            LOG_CONSOLE("Script unloaded");
        }

        if (ImGui::Button("Force Reload", ImVec2(-1, 0)))
        {
            LOG_CONSOLE("Manual reload...");
            std::string tempName = scriptName;
            LoadScript(tempName);
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No Script Loaded");

        static char scriptNameBuffer[128] = "Scripting";
        ImGui::InputText("##ScriptName", scriptNameBuffer, sizeof(scriptNameBuffer));

        if (ImGui::Button("Load Script", ImVec2(-1, 0)))
        {
            LoadScript(scriptNameBuffer);
        }
    }
}