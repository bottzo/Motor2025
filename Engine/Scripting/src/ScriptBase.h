#pragma once
#include "pch.h"
#include "GameScriptAPI.h"
#include "ScriptProperty.h"
#include <vector>

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

extern ScriptingAPI* g_API;

class ScriptBase
{
public:
    GameObjectHandle owner;
    std::vector<ScriptProperty> properties;

    ScriptBase(GameObjectHandle owner) : owner(owner) {}
    virtual ~ScriptBase() {}

    virtual void Start() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void CleanUp() = 0;

    virtual void RegisterProperties() {}

    const std::vector<ScriptProperty>& GetProperties() const { return properties; }

protected:
    // Helpers para registrar propiedades fácilmente
    void RegisterFloat(const std::string& name, float* ptr, float min = 0.0f, float max = 100.0f)
    {
        properties.push_back(ScriptProperty(name, ptr, min, max));
    }

    void RegisterInt(const std::string& name, int* ptr)
    {
        properties.push_back(ScriptProperty(name, PropertyType::INT, ptr));
    }

    void RegisterBool(const std::string& name, bool* ptr)
    {
        properties.push_back(ScriptProperty(name, PropertyType::BOOL, ptr));
    }

    void RegisterVec3(const std::string& name, Vec3* ptr)
    {
        properties.push_back(ScriptProperty(name, PropertyType::VEC3, ptr));
    }
};