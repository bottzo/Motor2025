#include "pch.h"
#include "TestingScript.h"
#include <iostream>

extern ScriptingAPI* g_API;

void TestingScript::Start()
{
    std::cout << "[TestingScript] Script started!" << std::endl;
}

void TestingScript::Update(float deltaTime)
{
    // Space key is pressed 
    if (g_API && g_API->GetKeyDown(44))
    {
        std::cout << "Space pressed" << std::endl;
    }
}

void TestingScript::CleanUp()
{
    std::cout << "[TestingScript] Script cleaned up" << std::endl;
}
