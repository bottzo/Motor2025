#pragma once

#ifdef SCRIPTING_EXPORTS
#define GAMESCRIPT_API __declspec(dllexport)
#else
#define GAMESCRIPT_API __declspec(dllimport)
#endif

// Forward declarations
typedef void* GameObjectHandle;
typedef void* ScriptInstanceHandle;
struct ScriptingAPI;

extern "C" {
    GAMESCRIPT_API ScriptInstanceHandle CreateScript(GameObjectHandle owner);
    GAMESCRIPT_API void DestroyScript(ScriptInstanceHandle instance);
    GAMESCRIPT_API void ScriptStart(ScriptInstanceHandle instance);
    GAMESCRIPT_API void ScriptUpdate(ScriptInstanceHandle instance, float deltaTime);
    GAMESCRIPT_API void ScriptSetAPI(ScriptingAPI* api);
}