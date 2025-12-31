#include "Random.h"
#include <cmath>

Random::Random()
    : m_generator(std::random_device{}())
{
}

Random::Random(Seed seed)
    : m_generator(seed)
{
}

void Random::SeedWith(Seed seed)
{
    m_generator.seed(seed);
}

float Random::Range(float min, float max)
{
    if (min > max) std::swap(min, max);
    std::uniform_real_distribution<float> d(min, std::nextafter(max, std::numeric_limits<float>::max()));
    return d(m_generator);
}

int Random::Range(int min, int max)
{
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int> d(min, max);
    return d(m_generator);
}

Vec2f Random::Range(Vec2f min, Vec2f max)
{
    return { Range(min.x, max.x), Range(min.y, max.y) };
}

Vec3f Random::Range(Vec3f min, Vec3f max)
{
    return { Range(min.x, max.x), Range(min.y, max.y), Range(min.z, max.z) };
}

Vec4f Random::Range(Vec4f min, Vec4f max)
{
    return { Range(min.x, max.x), Range(min.y, max.y), Range(min.z, max.z), Range(min.w, max.w) };
}

Color Random::Range(Color a, Color b)
{
    float t = Range(0.f, 1.f);
    return a + (b - a) * t;
}

Vec3f Random::PointOnSphere(float radius)
{
    std::normal_distribution<float> d(0.f, 1.f);
    
    Vec3f point(d(m_generator), d(m_generator), d(m_generator));
    float length = std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
    
    if (length < 1e-10f) {
        return Vec3f(radius, 0.f, 0.f);
    }
    
    return point * (radius / length);
}

Vec3f Random::PointInSphere(float radius)
{
    std::uniform_real_distribution<float> d(-1.f, 1.f);
    
    Vec3f point;
    float lengthSq;
    
    do {
        point = Vec3f(d(m_generator), d(m_generator), d(m_generator));
        lengthSq = point.x * point.x + point.y * point.y + point.z * point.z;
    } while (lengthSq > 1.f || lengthSq < 1e-10f);
    
    return point * radius;
}

Random& Random::Global()
{
    static Random instance;
    return instance;
}

float Random::Range(float min, float max, Seed seed)
{
    Random rng(seed);
    return rng.Range(min, max);
}

int Random::Range(int min, int max, Seed seed)
{
    Random rng(seed);
    return rng.Range(min, max);
}

Vec2f Random::Range(Vec2f min, Vec2f max, Seed seed)
{
    Random rng(seed);
    return rng.Range(min, max);
}

Vec3f Random::Range(Vec3f min, Vec3f max, Seed seed)
{
    Random rng(seed);
    return rng.Range(min, max);
}

Vec4f Random::Range(Vec4f min, Vec4f max, Seed seed)
{
    Random rng(seed);
    return rng.Range(min, max);
}

Color Random::Range(Color a, Color b, Seed seed)
{
    Random rng(seed);
    return rng.Range(a, b);
}

Vec3f Random::PointOnSphere(float radius, Seed seed)
{
    Random rng(seed);
    return rng.PointOnSphere(radius);
}

Vec3f Random::PointInSphere(float radius, Seed seed)
{
    Random rng(seed);
    return rng.PointInSphere(radius);
}