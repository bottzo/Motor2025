#pragma once

#ifdef SCRIPTING_EXPORTS
#define GAMESCRIPT_API __declspec(dllexport)
#else
#define GAMESCRIPT_API __declspec(dllimport)
#endif

extern "C" {
    GAMESCRIPT_API void ScriptStart();

    GAMESCRIPT_API void ScriptUpdate(float deltaTime);

    GAMESCRIPT_API void ScriptCleanUp();
}
