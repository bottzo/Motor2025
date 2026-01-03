#pragma once

struct ScriptingAPI
{
    void* (*CreateGameObject)(const char* name);
    void (*DestroyGameObject)(void* obj);
    void* (*GetTransform)(void* obj);
    void (*GetPosition)(void* transform, float* x, float* y, float* z);
    void (*SetPosition)(void* transform, float x, float y, float z);
    void (*GetRotation)(void* transform, float* x, float* y, float* z);
    void (*SetRotation)(void* transform, float x, float y, float z);
    bool (*GetKeyDown)(int keyCode);
    bool (*GetKey)(int keyCode);
    bool (*GetKeyUp)(int keyCode);
    float (*GetDeltaTime)();
    float (*GetGameTime)();
};