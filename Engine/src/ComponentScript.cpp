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

void ComponentScript::LoadScript(const std::string& dllName, const std::string& scriptClassName)
{
    UnloadScript();

    this->dllName = dllName;
    this->scriptClassName = scriptClassName;
    std::string originalDllPath = "x64\\Debug\\" + dllName + ".dll";

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

    tempDllPath = "x64\\Debug\\" + dllName + "_temp_" + std::to_string(timestamp) + ".dll";
    std::string tempPdbPath = "x64\\Debug\\" + dllName + "_temp_" + std::to_string(timestamp) + ".pdb";
    std::string originalPdbPath = "x64\\Debug\\" + dllName + ".pdb";

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
    scriptInstance = CreateScript(static_cast<GameObjectHandle>(owner), scriptClassName.c_str());

    if (scriptInstance == nullptr)
    {
        LOG_CONSOLE("ERROR: Failed to create script instance");
        UnloadScript();
        return;
    }

    scriptLoaded = true;
    startCalled = false;

    LOG_CONSOLE("Script '%s::%s' loaded!", dllName.c_str(), scriptClassName.c_str());
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

// ==================== SERIALIZATION ====================

void ComponentScript::Serialize(nlohmann::json& componentObj) const
{
    componentObj["dllName"] = dllName;
    componentObj["scriptClassName"] = scriptClassName;
    componentObj["scriptLoaded"] = scriptLoaded;
}

void ComponentScript::Deserialize(const nlohmann::json& componentObj)
{
    if (scriptLoaded)
    {
        UnloadScript();
    }

    // Load saved script data
    if (componentObj.contains("dllName") && componentObj.contains("scriptClassName"))
    {
        std::string savedDllName = componentObj["dllName"];
        std::string savedClassName = componentObj["scriptClassName"];
        bool wasLoaded = componentObj.value("scriptLoaded", false);

        // Only reload if it was previously loaded
        if (wasLoaded && savedDllName != "None" && savedClassName != "None")
        {
            LOG_CONSOLE("[ComponentScript] Restoring script: %s::%s", savedDllName.c_str(), savedClassName.c_str());
            LoadScript(savedDllName, savedClassName);
        }
    }
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

        std::string tempDll = dllName;
        std::string tempClass = scriptClassName;
        LoadScript(tempDll, tempClass);
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
    if (dllName.empty() || !scriptLoaded) return false;

    std::string originalDllPath = "x64\\Debug\\" + dllName + ".dll";

    try {
        if (!std::filesystem::exists(originalDllPath)) {
            LOG_DEBUG("DLL not found: %s", originalDllPath.c_str());
            return false;
        }

        auto currentWriteTime = std::filesystem::last_write_time(originalDllPath);

        // Si el DLL cambió (fue recompilado)
        if (currentWriteTime != lastDllWriteTime)
        {
            LOG_CONSOLE("DLL timestamp changed!");
            // Esperar a que termine de escribirse
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Reload script
            std::string tempDll = dllName;
            std::string tempClass = scriptClassName;
            UnloadScript();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            LoadScript(tempDll, tempClass);

            return false; // Already handled reload
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

    ImGui::Text("DLL:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s", dllName.c_str());

    ImGui::Text("Script Class:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", scriptClassName.c_str());

    ImGui::Spacing();

    if (scriptLoaded)
    {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), " Script Loaded");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Hot-Reload: Active");
        ImGui::TextWrapped("Click 'Rebuild Script' to compile and auto-reload changes");

        ImGui::Spacing();

        if (ImGui::Button("Rebuild Script", ImVec2(-1, 0)))
        {
            BuildScriptingProject();
        }

        if (ImGui::Button("Unload Script", ImVec2(-1, 0)))
        {
            UnloadScript();
            LOG_CONSOLE("Script unloaded");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0)))
        {
            markedForRemoval = true;
        }
        ImGui::PopStyleColor(3);
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No Script Loaded");

		// Refresh available scripts 
        static std::vector<std::string> availableScripts;
        static int selectedScriptIndex = 0;

        availableScripts = GetAvailableScripts();

        ImGui::Spacing();

        if (availableScripts.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "No compiled scripts found!");
            ImGui::Spacing();
            ImGui::TextWrapped("Create .h/.cpp files in Scripting folder, then compile to generate DLL");
        }
        else
        {
            ImGui::Text("Select DLL:");

            // DLL selection combo box
            const char* currentDll = availableScripts[selectedScriptIndex].c_str();
            if (ImGui::BeginCombo("##DllCombo", currentDll))
            {
                for (int i = 0; i < availableScripts.size(); i++)
                {
                    bool isSelected = (selectedScriptIndex == i);
                    if (ImGui::Selectable(availableScripts[i].c_str(), isSelected))
                    {
                        selectedScriptIndex = i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();

            // Script class selection
            static std::vector<std::string> availableClasses;
            static int selectedClassIndex = 0;
            static std::string lastSelectedDll = "";

            // Update classes when DLL changes
            if (lastSelectedDll != availableScripts[selectedScriptIndex])
            {
                lastSelectedDll = availableScripts[selectedScriptIndex];
                availableClasses = GetScriptClassesInDLL(lastSelectedDll);
                selectedClassIndex = 0;
            }

            if (!availableClasses.empty())
            {
                ImGui::Text("Select Script Class:");

                const char* currentClass = availableClasses[selectedClassIndex].c_str();
                if (ImGui::BeginCombo("##ClassCombo", currentClass))
                {
                    for (int i = 0; i < availableClasses.size(); i++)
                    {
                        bool isSelected = (selectedClassIndex == i);
                        if (ImGui::Selectable(availableClasses[i].c_str(), isSelected))
                        {
                            selectedClassIndex = i;
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Load Script", ImVec2(-1, 0)))
                {
                    LOG_CONSOLE("=== Loading Script ===");
                    LOG_CONSOLE("DLL: %s", availableScripts[selectedScriptIndex].c_str());
                    LOG_CONSOLE("Class: %s", availableClasses[selectedClassIndex].c_str());
                    LoadScript(availableScripts[selectedScriptIndex], availableClasses[selectedClassIndex]);
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No script classes found in this DLL");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Remove Component", ImVec2(-1, 0)))
        {
            markedForRemoval = true;
        }
        ImGui::PopStyleColor(3);
    }
}

// ==================== SCRIPT DISCOVERY ====================

std::vector<std::string> ComponentScript::GetAvailableScripts()
{
    std::vector<std::string> scripts;

    // Scan x64/Debug for compiled .dll files 
    std::string dllFolder = "x64/Debug";
    if (std::filesystem::exists(dllFolder))
    {
        for (const auto& entry : std::filesystem::directory_iterator(dllFolder))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".dll")
            {
                std::string dllName = entry.path().stem().string();

                // Avoid system DLLs and temporary DLLs
                if (dllName != "Engine" && dllName.find("_temp_") == std::string::npos)
                {
                    scripts.push_back(dllName);
                }
            }
        }
    }

    // Sort alphabetically
    std::sort(scripts.begin(), scripts.end());

    return scripts;
}

std::vector<std::string> ComponentScript::GetScriptClassesInDLL(const std::string& dllName)
{
    std::vector<std::string> classes;

    if (dllName == "Scripting")
    {
        classes.push_back("TestingScript");

		// Dont delete this comment - it's a placeholder
        // ADD MORE SCRIPTS HERE AS YOU CREATE THEM
    }
    // Add more DLLs here as needed

    return classes;
}

bool ComponentScript::BuildScriptingProject()
{
    LOG_CONSOLE("========================================");
    LOG_CONSOLE("Rebuilding Scripting project...");
    LOG_CONSOLE("========================================");

    // Using CMake with clean-first to force rebuild
    // CMake automatically detects all .h/.cpp files in Scripting folder
    std::string command = "cd \"C:\\Users\\haosh\\Documents\\GitHub\\Motor2025\\Engine\\build\" && " 
                          "cmake --build . --config Debug --target Scripting --clean-first";

    // Execute command 
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(NULL, const_cast<char*>(("cmd.exe /C " + command).c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        LOG_CONSOLE("Build started... (this may take a few seconds)");

        // Wait for process to complete 
        DWORD result = WaitForSingleObject(pi.hProcess, 30000); // 30 second timeout

        if (result == WAIT_OBJECT_0)
        {
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode == 0)
            {
                LOG_CONSOLE("========================================");
                LOG_CONSOLE("Build SUCCESS!");
                LOG_CONSOLE("========================================");
                return true;
            }
            else
            {
                LOG_CONSOLE("========================================");
                LOG_CONSOLE("Build FAILED (exit code: %d)", exitCode);
                LOG_CONSOLE("Check Visual Studio for errors");
                LOG_CONSOLE("========================================");
                return false;
            }
        }
        else
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            LOG_CONSOLE("Build timed out after 30 seconds");
            return false;
        }
    }
    else
    {
        LOG_CONSOLE("ERROR: Failed to start build process");
        return false;
    }
}

