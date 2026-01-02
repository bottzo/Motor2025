#include "ModuleScripting.h"
#include "Log.h"
#include "Application.h"

ModuleScripting::ModuleScripting() : Module()
{
    name = "ModuleScripting";
    scriptDLL = nullptr;
    ScriptStart = nullptr;
    ScriptUpdate = nullptr;
    ScriptCleanUp = nullptr;
    scriptStartCalled = false;
}

ModuleScripting::~ModuleScripting()
{
}

bool ModuleScripting::Start()
{
    LOG_CONSOLE("Initializing Scripting System...");

    const char* dllPath = "x64\\Debug\\Scripting.dll";

    // Load the DLL
    scriptDLL = LoadLibraryA(dllPath);

    if (scriptDLL == nullptr)
    {
        DWORD error = GetLastError();
        LOG_CONSOLE("ERROR: Failed to load Scripting.dll (Error code: %d)", error);
        LOG_CONSOLE("Make sure the DLL is compiled and located at: %s", dllPath);
        return false;
    }

    LOG_CONSOLE("Scripting.dll loaded successfully!");

    // Get the function pointers
    ScriptStart = (ScriptStartFunc)GetProcAddress(scriptDLL, "ScriptStart");
    ScriptUpdate = (ScriptUpdateFunc)GetProcAddress(scriptDLL, "ScriptUpdate");
    ScriptCleanUp = (ScriptCleanUpFunc)GetProcAddress(scriptDLL, "ScriptCleanUp");

    // Call ScriptStart once
    if (ScriptStart != nullptr && !scriptStartCalled)
    {
        LOG_CONSOLE("Calling ScriptStart()...");
        ScriptStart();
        scriptStartCalled = true;
    }

    return true;
}

bool ModuleScripting::Update()
{
    // Call ScriptUpdate every frame
    if (ScriptUpdate != nullptr)
    {
        float deltaTime = Application::GetInstance().time->GetDeltaTime();
        ScriptUpdate(deltaTime);
    }

    return true;
}

bool ModuleScripting::CleanUp()
{
    LOG_CONSOLE("Cleaning up Scripting System...");

    // Call ScriptCleanUp 
    if (ScriptCleanUp != nullptr)
    {
        ScriptCleanUp();
    }

    // Unload the DLL
    if (scriptDLL != nullptr)
    {
        FreeLibrary(scriptDLL);
        scriptDLL = nullptr;
        LOG_CONSOLE("Scripting.dll unloaded");
    }

    return true;
}