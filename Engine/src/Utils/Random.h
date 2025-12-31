#pragma once
#include <random>
#include <cstdint>
#include <galaxymath/Maths.h>
#include "Color.h"

using Seed = uint32_t;

class Random
{
public:
    Random();
    explicit Random(Seed seed);

    void SeedWith(Seed seed);

    float Range(float min, float max);
    int   Range(int min, int max);
    Vec2f Range(Vec2f min, Vec2f max);
    Vec3f Range(Vec3f min, Vec3f max);
    Vec4f Range(Vec4f min, Vec4f max);
    Color Range(Color a, Color b);
    
    Vec3f PointOnSphere(float radius = 1.f);
    Vec3f PointInSphere(float radius = 1.f);

    static Random& Global();
    
    static float Range(float min, float max, Seed seed);
    static int   Range(int min, int max, Seed seed);
    static Vec2f Range(Vec2f min, Vec2f max, Seed seed);
    static Vec3f Range(Vec3f min, Vec3f max, Seed seed);
    static Vec4f Range(Vec4f min, Vec4f max, Seed seed);
    static Color Range(Color a, Color b, Seed seed);

    static Vec3f PointOnSphere(float radius, Seed seed);
    static Vec3f PointInSphere(float radius, Seed seed);
private:
    std::mt19937 m_generator;
};
