#pragma once
#include <string>
#include <vector>

enum class PropertyType
{
    FLOAT,
    INT,
    BOOL,
    VEC3
};

struct ScriptProperty
{
    std::string name;
    PropertyType type;
    void* dataPtr;

    // Límites para floats/ints
    float minValue = 0.0f;
    float maxValue = 100.0f;

    ScriptProperty(const std::string& n, PropertyType t, void* ptr)
        : name(n), type(t), dataPtr(ptr) {
    }

    ScriptProperty(const std::string& n, float* ptr, float min = 0.0f, float max = 100.0f)
        : name(n), type(PropertyType::FLOAT), dataPtr(ptr), minValue(min), maxValue(max) {
    }
};

struct Vec3
{
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};