#pragma once
#include "pch.h"
#include "GameScriptAPI.h"

typedef void* GameObjectHandle;
typedef void* TransformHandle;

struct ScriptingAPI
{
    GameObjectHandle(*CreateGameObject)(const char* name);
    void (*DestroyGameObject)(GameObjectHandle obj);
    TransformHandle(*GetTransform)(GameObjectHandle obj);
    void (*GetPosition)(TransformHandle transform, float* x, float* y, float* z);
    void (*SetPosition)(TransformHandle transform, float x, float y, float z);
    void (*GetRotation)(TransformHandle transform, float* x, float* y, float* z);
    void (*SetRotation)(TransformHandle transform, float x, float y, float z);
    bool (*GetKeyDown)(int keyCode);
    bool (*GetKey)(int keyCode);
    bool (*GetKeyUp)(int keyCode);
    float (*GetDeltaTime)();
    float (*GetGameTime)();
};

// Global API pointer - accessible from all scripts
extern ScriptingAPI* g_API;

// Base class for all scripts
class ScriptBase
{
public:
    GameObjectHandle owner;

    ScriptBase(GameObjectHandle owner) : owner(owner) {}
    virtual ~ScriptBase() {}

    virtual void Start() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void CleanUp() = 0;
};
