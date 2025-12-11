#pragma once
#include <galaxymath/Maths.h>

struct HSV
{
    float hue;
    float saturation;
    float value;
};

// Color class with range [0, 1]
class Color
{
public:
    float r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
    Color(Vec3f rgb, float a = 1.0f) : r(rgb.x), g(rgb.y), b(rgb.z), a(a) {}
    Color(Vec4f rgba) : r(rgba.x), g(rgba.y), b(rgba.z), a(rgba.w) {}
    
    Color operator+(const Color& other) const { return Color(r + other.r, g + other.g, b + other.b, a + other.a); }
    Color operator-(const Color& other) const { return Color(r - other.r, g - other.g, b - other.b, a - other.a); }
    Color operator*(const Color& other) const { return Color(r * other.r, g * other.g, b * other.b, a * other.a); }
    Color operator/(const Color& other) const { return Color(r / other.r, g / other.g, b / other.b, a / other.a); }
    
    void operator+=(float scalear) { r += scalear; g += scalear; b += scalear; a += scalear; }
    void operator-=(float scalear) { r -= scalear; g -= scalear; b -= scalear; a -= scalear; }
    void operator*=(float scalear) { r *= scalear; g *= scalear; b *= scalear; a *= scalear; }
    void operator/=(float scalear) { r /= scalear; g /= scalear; b /= scalear; a /= scalear; }
    
    Color operator*(float scalar) const { return Color(r * scalar, g * scalar, b * scalar, a * scalar); }
    Color operator/(float scalar) const { return Color(r / scalar, g / scalar, b / scalar, a / scalar); }
    
    operator Vec4f() const { return {r, g, b, a}; }
    operator Vec3f() const { return {r, g, b}; }

    static Color FromHSV(const HSV& hsv);
    static Color FromHSV(float h, float s, float v);
    
    HSV ToHSV() const;

    Color operator=(Vec4f rgba) { r = rgba.x; g = rgba.y; b = rgba.z; a = rgba.w; return *this; }
    Color operator=(Vec3f rgb) { r = rgb.x; g = rgb.y; b = rgb.z; return *this; }
    
    static const Color White;
    static const Color Black;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Cyan;
    static const Color Magenta;
};
