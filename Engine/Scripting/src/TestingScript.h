#pragma once
#include "ScriptBase.h"

class TestingScript : public ScriptBase
{
public:
    // VARIABLES PÚBLICAS - Aparecerán en el Inspector
    float moveSpeed = 5.0f;
    int playerHealth = 100;
    bool isEnabled = true;

    TestingScript(GameObjectHandle owner) : ScriptBase(owner) {}
    ~TestingScript() override {}

    void Start() override;
    void Update(float deltaTime) override;
    void CleanUp() override;
    void RegisterProperties() override;  // NUEVO
};