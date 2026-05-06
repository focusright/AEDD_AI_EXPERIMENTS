#include "MathTypes.h"

namespace HeatDemo
{
Vec3 operator+(const Vec3& lhs, const Vec3& rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
{
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

Vec3 operator*(const Vec3& lhs, double scalar)
{
    return {lhs.x * scalar, lhs.y * scalar, lhs.z * scalar};
}

Vec3 operator*(double scalar, const Vec3& rhs)
{
    return rhs * scalar;
}

Vec3 operator/(const Vec3& lhs, double scalar)
{
    return {lhs.x / scalar, lhs.y / scalar, lhs.z / scalar};
}

Vec3& operator+=(Vec3& lhs, const Vec3& rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    return lhs;
}

Vec3& operator-=(Vec3& lhs, const Vec3& rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    return lhs;
}

double Dot(const Vec3& lhs, const Vec3& rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 Cross(const Vec3& lhs, const Vec3& rhs)
{
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

double Length(const Vec3& value)
{
    return std::sqrt(LengthSquared(value));
}

double LengthSquared(const Vec3& value)
{
    return Dot(value, value);
}

Vec3 Normalize(const Vec3& value)
{
    const double length = Length(value);
    if (length <= 1e-15) {
        return {};
    }
    return value / length;
}

double Clamp(double value, double low, double high)
{
    return std::max(low, std::min(high, value));
}

double Clamp01(double value)
{
    return Clamp(value, 0.0, 1.0);
}

bool IsFinite(double value)
{
    return std::isfinite(value);
}

bool IsFinite(const Vec3& value)
{
    return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

Color Lerp(const Color& a, const Color& b, double t)
{
    const float u = static_cast<float>(Clamp01(t));
    const float v = 1.0f - u;
    return {
        a.r * v + b.r * u,
        a.g * v + b.g * u,
        a.b * v + b.b * u,
        a.a * v + b.a * u
    };
}

Color SaturatedColor(double r, double g, double b, double a)
{
    return {
        static_cast<float>(Clamp01(r)),
        static_cast<float>(Clamp01(g)),
        static_cast<float>(Clamp01(b)),
        static_cast<float>(Clamp01(a))
    };
}

uint64_t EdgeKey(int a, int b)
{
    const uint32_t lo = static_cast<uint32_t>(std::min(a, b));
    const uint32_t hi = static_cast<uint32_t>(std::max(a, b));
    return (static_cast<uint64_t>(lo) << 32u) | static_cast<uint64_t>(hi);
}
}
