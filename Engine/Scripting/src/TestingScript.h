#pragma once
#include "ScriptBase.h"

class TestingScript : public ScriptBase
{
public:
    TestingScript(GameObjectHandle owner) : ScriptBase(owner) {}
    ~TestingScript() override {}

    void Start() override;
    void Update(float deltaTime) override;
    void CleanUp() override;
};
