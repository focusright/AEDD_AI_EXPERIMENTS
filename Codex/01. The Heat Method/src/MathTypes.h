#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace HeatDemo
{
constexpr double Pi = 3.1415926535897932384626433832795;

struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Tri
{
    int a = 0;
    int b = 0;
    int c = 0;
};

Vec3 operator+(const Vec3& lhs, const Vec3& rhs);
Vec3 operator-(const Vec3& lhs, const Vec3& rhs);
Vec3 operator*(const Vec3& lhs, double scalar);
Vec3 operator*(double scalar, const Vec3& rhs);
Vec3 operator/(const Vec3& lhs, double scalar);
Vec3& operator+=(Vec3& lhs, const Vec3& rhs);
Vec3& operator-=(Vec3& lhs, const Vec3& rhs);

double Dot(const Vec3& lhs, const Vec3& rhs);
Vec3 Cross(const Vec3& lhs, const Vec3& rhs);
double Length(const Vec3& value);
double LengthSquared(const Vec3& value);
Vec3 Normalize(const Vec3& value);
double Clamp(double value, double low, double high);
double Clamp01(double value);
bool IsFinite(double value);
bool IsFinite(const Vec3& value);
Color Lerp(const Color& a, const Color& b, double t);
Color SaturatedColor(double r, double g, double b, double a = 1.0);
uint64_t EdgeKey(int a, int b);
}
