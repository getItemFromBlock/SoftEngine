#include "Color.h"

const Color Color::White = {1, 1, 1};
const Color Color::Black = {0, 0, 0};
const Color Color::Red = {1, 0, 0};
const Color Color::Green = {0, 1, 0};
const Color Color::Blue = {0, 0, 1};
const Color Color::Yellow = {1, 1, 0};
const Color Color::Cyan = {0, 1, 1};
const Color Color::Magenta = {1, 0, 1};

Color Color::FromHSV(const HSV& hsv)
{
    return FromHSV(hsv.hue, hsv.saturation, hsv.value);
}

Color Color::FromHSV(float h, float s, float v) 
{
    Color result;
    float c = v * s;
    float hp = h / 60.f;
    float x = c * (1 - std::abs(std::fmodf(hp, 2) - 1));
    float m = v - c;
    
    if (0 <= hp && hp < 1)
    {
        result = Color{c, x, 0, 1.0f};
    }
    else if (1 <= hp && hp < 2)
    {
        result = Color{x, c, 0, 1.0f};
    }
    else if (2 <= hp && hp < 3)
    {
        result = Color{0, c, x, 1.0f};
    }
    else if (3 <= hp && hp < 4)
    {
        result = Color{0, x, c, 1.0f};
    }
    else if (4 <= hp && hp < 5)
    {
        result = Color{x, 0, c, 1.0f};
    }
    else if (5 <= hp && hp < 6)
    {
        result = Color{c, 0, x, 1.0f};
    }
    
    result += m;
    return result;
}

HSV Color::ToHSV() const
{
    float cMax = std::max(r, std::max(g, b));
    float cMin = std::min(r, std::min(g, b));
    float delta = cMax - cMin;
    
    float h = 0;
    if (delta != 0)
    {
        if (cMax == r)
        {
            h = 60.f * (std::fmodf((g - b) / delta, 6));
        }
        else if (cMax == g)
        {
            h = 60.f * (((b - r) / delta) + 2);
        }
        else if (cMax == b)
        {
            h = 60.f * (((r - g) / delta) + 4);
        }
    }
    
    float s = cMax == 0 ? 0 : delta / cMax;
    float v = cMax;
    
    return {.hue = h, .saturation = s, .value = v};
}
