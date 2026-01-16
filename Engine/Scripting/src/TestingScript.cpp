#include "pch.h"
#include "TestingScript.h"
#include <iostream>

extern ScriptingAPI* g_API;

void TestingScript::RegisterProperties()
{
    // Registrar las propiedades para que aparezcan en el Inspector
    RegisterFloat("Move Speed", &moveSpeed, 0.0f, 20.0f);
    RegisterInt("Player Health", &playerHealth);
    RegisterBool("Is Enabled", &isEnabled);
}

void TestingScript::Start()
{
    std::cout << "[TestingScript] Script started!" << std::endl;
    std::cout << "Move Speed: " << moveSpeed << std::endl;
    std::cout << "Player Health: " << playerHealth << std::endl;
    std::cout << "Is Enabled: " << (isEnabled ? "YES" : "NO") << std::endl;
}

void TestingScript::Update(float deltaTime)
{
    if (!isEnabled) return;  // Si está deshabilitado, no hacer nada

    // Space key is pressed 
    if (g_API && g_API->GetKeyDown(44))
    {
        std::cout << "[TestingScript] SPACE PRESSED!" << std::endl;
        std::cout << "Move Speed: " << moveSpeed << std::endl;
        std::cout << "Player Health: " << playerHealth << std::endl;
        std::cout << "Is Enabled: " << (isEnabled ? "YES" : "NO") << std::endl;

        // Ejemplo: mover el objeto
        TransformHandle transform = g_API->GetTransform(owner);
        if (transform)
        {
            float x, y, z;
            g_API->GetPosition(transform, &x, &y, &z);

            std::cout << "Current Position: X=" << x << " Y=" << y << " Z=" << z << std::endl;

            // Mover hacia adelante
            z += moveSpeed * deltaTime;

            g_API->SetPosition(transform, x, y, z);
            std::cout << "New Position: X=" << x << " Y=" << y << " Z=" << z << std::endl;
        }

    }
}

void TestingScript::CleanUp()
{
    std::cout << "[TestingScript] Script cleaned up" << std::endl;
}