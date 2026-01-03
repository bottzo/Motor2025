#include "ModuleScripting.h"
#include "Log.h"
#include "Application.h"
#include "ModuleScene.h"
#include "GameObject.h"
#include "Transform.h"
#include "Input.h"

void* API_CreateGameObject(const char* name)
{
    try {
        ModuleScene* scene = Application::GetInstance().scene.get();
        if (!scene) {
            LOG_CONSOLE("ERROR: Scene is null when creating GameObject");
            return nullptr;
        }

        GameObject* obj = scene->CreateGameObject(name);

        if (!obj) {
            LOG_CONSOLE("ERROR: Failed to create GameObject: %s", name);
            return nullptr;
        }

        // Verificar que el GameObject tiene al menos un Transform
        if (!obj->GetComponent(ComponentType::TRANSFORM)) {
            LOG_CONSOLE("WARNING: GameObject '%s' created without Transform", name);
        }

        LOG_CONSOLE("Script created GameObject: %s (ptr: %p)", name, obj);
        return static_cast<void*>(obj);
    }
    catch (const std::exception& e) {
        LOG_CONSOLE("ERROR: Exception creating GameObject '%s': %s", name, e.what());
        return nullptr;
    }
    catch (...) {
        LOG_CONSOLE("ERROR: Unknown exception creating GameObject '%s'", name);
        return nullptr;
    }
}

void API_DestroyGameObject(void* obj)
{
    if (obj)
    {
        GameObject* gameObj = static_cast<GameObject*>(obj);
        gameObj->MarkForDeletion();
        LOG_CONSOLE("Script marked GameObject for deletion: %s", gameObj->GetName().c_str());
    }
}

void* API_GetTransform(void* obj)
{
    if (!obj) return nullptr;
    GameObject* gameObj = static_cast<GameObject*>(obj);
    return gameObj->GetComponent(ComponentType::TRANSFORM);
}

void API_GetPosition(void* transform, float* x, float* y, float* z)
{
    if (!transform) return;
    Transform* t = static_cast<Transform*>(transform);
    glm::vec3 pos = t->GetPosition();
    *x = pos.x;
    *y = pos.y;
    *z = pos.z;
}

void API_SetPosition(void* transform, float x, float y, float z)
{
    if (!transform) return;
    Transform* t = static_cast<Transform*>(transform);
    t->SetPosition(glm::vec3(x, y, z));
}

void API_GetRotation(void* transform, float* x, float* y, float* z)
{
    if (!transform) return;
    Transform* t = static_cast<Transform*>(transform);
    glm::vec3 rot = t->GetRotation();
    *x = rot.x;
    *y = rot.y;
    *z = rot.z;
}

void API_SetRotation(void* transform, float x, float y, float z)
{
    if (!transform) return;
    Transform* t = static_cast<Transform*>(transform);
    t->SetRotation(glm::vec3(x, y, z));
}

bool API_GetKeyDown(int keyCode)
{
    return Application::GetInstance().input.get()->GetKey(keyCode) == KEY_DOWN;
}

bool API_GetKey(int keyCode)
{
    return Application::GetInstance().input.get()->GetKey(keyCode) == KEY_REPEAT;
}

bool API_GetKeyUp(int keyCode)
{
    return Application::GetInstance().input.get()->GetKey(keyCode) == KEY_UP;
}

float API_GetDeltaTime()
{
    return Application::GetInstance().time.get()->GetDeltaTime();
}

float API_GetGameTime()
{
    return Application::GetInstance().time.get()->GetGameTime();
}

// modulo

ModuleScripting::ModuleScripting() : Module()
{
    name = "ModuleScripting";
}

ModuleScripting::~ModuleScripting()
{
}

bool ModuleScripting::Start()
{
    LOG_CONSOLE("Initializing Scripting System...");

    // Setup Engine API
    engineAPI.CreateGameObject = API_CreateGameObject;
    engineAPI.DestroyGameObject = API_DestroyGameObject;
    engineAPI.GetTransform = API_GetTransform;
    engineAPI.GetPosition = API_GetPosition;
    engineAPI.SetPosition = API_SetPosition;
    engineAPI.GetRotation = API_GetRotation;
    engineAPI.SetRotation = API_SetRotation;
    engineAPI.GetKeyDown = API_GetKeyDown;
    engineAPI.GetKey = API_GetKey;
    engineAPI.GetKeyUp = API_GetKeyUp;
    engineAPI.GetDeltaTime = API_GetDeltaTime;
    engineAPI.GetGameTime = API_GetGameTime;

    LOG_CONSOLE("Scripting System initialized!");

    return true;
}

bool ModuleScripting::Update()
{
    // los scripts individuales se actualizan a traves de ComponentScript
    return true;
}

bool ModuleScripting::CleanUp()
{
    LOG_CONSOLE("Cleaning up Scripting System...");
    return true;
}