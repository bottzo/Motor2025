// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "GameScriptAPI.h"
#include <iostream>
#include <vector>

typedef void* GameObjectHandle;
typedef void* TransformHandle;
typedef void* ScriptInstanceHandle;

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

//GLOBAL API POINTER
static ScriptingAPI* g_API = nullptr;

// BULLET STRUCTURE 
struct Bullet
{
    GameObjectHandle gameObject;
    float spawnTime;
    float lifetime;
};


class TankScript
{
public:
    GameObjectHandle owner;
    std::vector<Bullet> bullets;
    float timeSinceLastShot = 0.0f;
    float fireRate = 0.5f;
    float bulletLifetime = 3.0f;
    float bulletSpeed = 10.0f;

    TankScript(GameObjectHandle owner) : owner(owner)
    {
        std::cout << "[TankScript] Instance created for GameObject" << std::endl;
    }

    void Start()
    {
        std::cout << "[TankScript] Tank Script Initialized!" << std::endl;
        std::cout << "[TankScript] Press SPACE to shoot bullets" << std::endl;
        std::cout << "[TankScript] Bullet lifetime: " << bulletLifetime << "s" << std::endl;
    }

    void Update(float deltaTime)
    {
        if (!g_API) return;

        timeSinceLastShot += deltaTime;
        float currentTime = g_API->GetGameTime();

        // FIRE BULLETS
        if (g_API->GetKeyDown(44) && timeSinceLastShot >= fireRate)  // SPACE = 44
        {
            std::cout << "[TankScript] FIRE! Creating bulletYO SOY GIGANTEEEEEEEEEEEEEEEEEEEEE..." << std::endl;

            // Get tank position
            TransformHandle tankTransform = g_API->GetTransform(owner);
            float tankX, tankY, tankZ;
            g_API->GetPosition(tankTransform, &tankX, &tankY, &tankZ);

            // Create bullet GameObject
            GameObjectHandle bulletObj = g_API->CreateGameObject("Bullet");

            if (bulletObj)
            {
                // Position bullet at tank's position + offset (barrel)
                TransformHandle bulletTransform = g_API->GetTransform(bulletObj);
                g_API->SetPosition(bulletTransform, tankX, tankY + 1.0f, tankZ + 2.0f);

                // Store bullet info
                Bullet bullet;
                bullet.gameObject = bulletObj;
                bullet.spawnTime = currentTime;
                bullet.lifetime = bulletLifetime;
                bullets.push_back(bullet);

                std::cout << "[TankScript] Bullet created! Total bullets: " << bullets.size() << std::endl;

                timeSinceLastShot = 0.0f;
            }
        }

        // UPDATE BULLETS
        for (int i = bullets.size() - 1; i >= 0; --i)
        {
            Bullet& bullet = bullets[i];
            float age = currentTime - bullet.spawnTime;

            // Check if bullet should be destroyed
            if (age >= bullet.lifetime)
            {
                std::cout << "[TankScript] Bullet expired, destroying..." << std::endl;
                g_API->DestroyGameObject(bullet.gameObject);
                bullets.erase(bullets.begin() + i);
                continue;
            }

            // Move bullet forward (along Z axis)
            TransformHandle transform = g_API->GetTransform(bullet.gameObject);
            if (transform)
            {
                float x, y, z;
                g_API->GetPosition(transform, &x, &y, &z);

                // Move forward (positive Z direction)
                z += bulletSpeed * deltaTime;

                g_API->SetPosition(transform, x, y, z);
            }
        }
    }

    void CleanUp()
    {
        std::cout << "[TankScript] Cleaning up..." << std::endl;

        // Destroy all remaining bullets
        if (g_API)
        {
            for (auto& bullet : bullets)
            {
                g_API->DestroyGameObject(bullet.gameObject);
            }
        }

        bullets.clear();
    }
};

// DLL ENTRY POINT
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// EXPORTED FUNCTIONS 

void ScriptSetAPI(ScriptingAPI* api)
{
    g_API = api;
    std::cout << "[TankScript] Engine API connected!" << std::endl;
}

ScriptInstanceHandle CreateScript(GameObjectHandle owner)
{
    std::cout << "[TankScript] CreateScript called" << std::endl;
    TankScript* instance = new TankScript(owner);
    return static_cast<ScriptInstanceHandle>(instance);
}

void DestroyScript(ScriptInstanceHandle instance)
{
    if (instance)
    {
        std::cout << "[TankScript] DestroyScript called" << std::endl;
        TankScript* script = static_cast<TankScript*>(instance);
        script->CleanUp();
        delete script;
    }
}

void ScriptStart(ScriptInstanceHandle instance)
{
    if (instance)
    {
        TankScript* script = static_cast<TankScript*>(instance);
        script->Start();
    }
}

void ScriptUpdate(ScriptInstanceHandle instance, float deltaTime)
{
    if (instance)
    {
        TankScript* script = static_cast<TankScript*>(instance);
        script->Update(deltaTime);
    }
}