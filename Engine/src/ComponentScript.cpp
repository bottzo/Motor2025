#include "ComponentScript.h"
#include "GameObject.h"
#include "Application.h"
#include "Log.h"
#include "BuildConfig.h"
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include "LibraryManager.h"
#include "MetaFile.h"

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

    try {
        lastDllWriteTime = std::filesystem::last_write_time(originalDllPath);
    }
    catch (...) {
        lastDllWriteTime = std::filesystem::file_time_type::min();
    }

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    tempDllPath = "x64\\Debug\\" + dllName + "_temp_" + std::to_string(timestamp) + ".dll";
    std::string tempPdbPath = "x64\\Debug\\" + dllName + "_temp_" + std::to_string(timestamp) + ".pdb";
    std::string originalPdbPath = "x64\\Debug\\" + dllName + ".pdb";

    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        int maxRetries = 10;
        bool success = false;

        for (int retry = 0; retry < maxRetries && !success; retry++)
        {
            if (retry > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }

            try {
                std::filesystem::copy_file(originalDllPath, tempDllPath,
                    std::filesystem::copy_options::overwrite_existing);

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

    scriptDLL = LoadLibraryA(tempDllPath.c_str());

    if (scriptDLL == nullptr)
    {
        DWORD error = GetLastError();
        LOG_CONSOLE("ERROR: Failed to load DLL (Error: %d)", error);
        scriptLoaded = false;
        CleanupTempDLL();
        return;
    }

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

    ScriptingAPI* engineAPI = Application::GetInstance().scripting->GetEngineAPI();
    if (engineAPI && ScriptSetAPI)
    {
        ScriptSetAPI(engineAPI);
    }

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

    if (componentObj.contains("dllName") && componentObj.contains("scriptClassName"))
    {
        std::string savedDllName = componentObj["dllName"];
        std::string savedClassName = componentObj["scriptClassName"];
        bool wasLoaded = componentObj.value("scriptLoaded", false);

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

    if (!startCalled && ScriptStart)
    {
        ScriptStart(scriptInstance);
        startCalled = true;
    }

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
            return false;
        }

        auto currentWriteTime = std::filesystem::last_write_time(originalDllPath);

        if (currentWriteTime != lastDllWriteTime)
        {
            LOG_CONSOLE("DLL timestamp changed!");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            std::string tempDll = dllName;
            std::string tempClass = scriptClassName;
            UnloadScript();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            LoadScript(tempDll, tempClass);

            return false;
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

        static std::vector<std::string> availableScripts;
        static int selectedScriptIndex = 0;

        availableScripts = GetAvailableScripts();

        ImGui::Spacing();

        if (availableScripts.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "No compiled scripts found!");
            ImGui::Spacing();
            ImGui::TextWrapped("Create .h/.cpp files in Assets/Scripts folder, then compile to generate DLL");
        }
        else
        {
            ImGui::Text("Select DLL:");

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

            static std::vector<std::string> availableClasses;
            static int selectedClassIndex = 0;
            static std::string lastSelectedDll = "";

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

std::vector<std::string> ComponentScript::GetAvailableScripts()
{
    std::vector<std::string> scripts;

    std::string dllFolder = "x64/Debug";
    if (std::filesystem::exists(dllFolder))
    {
        for (const auto& entry : std::filesystem::directory_iterator(dllFolder))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".dll")
            {
                std::string dllName = entry.path().stem().string();

                if (dllName != "Engine" && dllName.find("_temp_") == std::string::npos)
                {
                    scripts.push_back(dllName);
                }
            }
        }
    }

    std::sort(scripts.begin(), scripts.end());
    return scripts;
}

std::vector<std::string> ComponentScript::GetScriptClassesInDLL(const std::string& dllName)
{
    std::vector<std::string> classes;

    if (dllName == "Scripting")
    {
        std::string scriptsPath = LibraryManager::GetScriptingRoot();

        if (std::filesystem::exists(scriptsPath))
        {
            for (const auto& entry : std::filesystem::directory_iterator(scriptsPath))
            {
                if (!entry.is_regular_file()) continue;

                std::string extension = entry.path().extension().string();
                if (extension == ".h")
                {
                    std::string className = entry.path().stem().string();
                    classes.push_back(className);
                }
            }
        }
    }

    return classes;
}

bool ComponentScript::BuildScriptingProject()
{
    LOG_CONSOLE("Syncing scripts from Assets/Scripts...");

    std::string assetsScripts = LibraryManager::GetScriptingRoot();
    std::string buildScriptsDir = "Scripting/src";

    if (!std::filesystem::exists(buildScriptsDir))
    {
        std::filesystem::create_directories(buildScriptsDir);
    }

    int copiedFiles = 0;
    if (std::filesystem::exists(assetsScripts))
    {
        for (const auto& entry : std::filesystem::directory_iterator(assetsScripts))
        {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            if (ext != ".h" && ext != ".cpp") continue;

            std::string dest = buildScriptsDir + "/" + entry.path().filename().string();

            try {
                std::filesystem::copy_file(entry.path(), dest,
                    std::filesystem::copy_options::overwrite_existing);
                copiedFiles++;
                LOG_DEBUG("Copied: %s", entry.path().filename().string().c_str());
            }
            catch (const std::exception& e) {
                LOG_CONSOLE("Failed to copy %s: %s",
                    entry.path().filename().string().c_str(), e.what());
            }
        }
    }

    LOG_CONSOLE("Copied %d script files to build directory", copiedFiles);
    LOG_CONSOLE("========================================");
    LOG_CONSOLE("Rebuilding Scripting project...");
    LOG_CONSOLE("========================================");

    std::string command = "cd \"C:\\Users\\haosh\\Documents\\GitHub\\Motor2025\\Engine\\build\" && "
        "cmake --build . --config Debug --target Scripting --clean-first";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(NULL, const_cast<char*>(("cmd.exe /C " + command).c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        LOG_CONSOLE("Build started... (this may take a few seconds)");

        DWORD result = WaitForSingleObject(pi.hProcess, 30000);

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

                ImportBuiltScriptsToLibrary();

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

void ComponentScript::ImportBuiltScriptsToLibrary()
{
    LOG_CONSOLE("[ComponentScript] Importing built scripts to Library...");

    std::string scriptsPath = LibraryManager::GetScriptingRoot();
    std::string dllPath = "x64/Debug/Scripting.dll";

    if (!std::filesystem::exists(dllPath)) {
        LOG_CONSOLE("[ComponentScript] WARNING: Built DLL not found at: %s", dllPath.c_str());
        return;
    }

    int imported = 0;

    for (const auto& entry : std::filesystem::directory_iterator(scriptsPath)) {
        if (!entry.is_regular_file()) continue;

        std::string extension = entry.path().extension().string();
        if (extension != ".h") continue;

        std::string headerPath = entry.path().string();

        if (LibraryManager::ImportScriptToLibrary(headerPath, dllPath)) {
            imported++;
        }
    }

    LOG_CONSOLE("[ComponentScript] Imported %d scripts to Library", imported);
}