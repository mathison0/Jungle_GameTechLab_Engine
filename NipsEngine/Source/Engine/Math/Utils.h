#pragma once

#include <algorithm>

namespace MathUtil
{
    static constexpr float Epsilon{1e-6f};

    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float InvPI = 0.31830988618379067154f;
    static constexpr float HalfPI = 1.57079632679489661923f;
    static constexpr float TwoPi = 6.28318530717958647692f;
    static constexpr float DEG_TO_RAD = PI / 180;
    static constexpr float RAD_TO_DEG = 180 / PI;

    static constexpr float SmallNumber = 1.e-8f;
    static constexpr float KindaSmallNumber = 1.e-4f;

    static constexpr float DegreesToRadians(float Degrees) { return Degrees * (PI / 180.0f); }

    static constexpr float RadiansToDegrees(float Radians) { return Radians * (180.0f / PI); }

    static constexpr float Abs(float Value) { return (Value < 0.0f) ? -Value : Value; }

    static constexpr bool IsNearlyZero(float Value, float Tolerance = Epsilon) { return Abs(Value) <= Tolerance; }

    static constexpr bool IsNearlyEqual(float A, float B, float Tolerance = Epsilon) { return Abs(A - B) <= Tolerance; }

    template <typename T> static inline T Clamp(const T Value, const T Min, const T Max)
    {
        return (Value < Min) ? Min : (Value > Max) ? Max : Value;
    }

    template <typename T> static inline T Max3(const T A, const T B, const T C)
    {
        return std::max(A, std::max(B, C));
    }

    template <typename T> static inline T Min3(const T A, const T B, const T C)
    {
        return std::min(A, std::min(B, C));
    }

	template <typename T, typename U> static inline T Lerp(const T& A, const T& B, const U& Alpha)
    {
        return A + (B - A) * Alpha;
    }

	// CameraShake
	//static inline float Quintic(float t)
	//{
 //       return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	//}

	//static inline float Hash1D(int32_t x)
 //   {
 //       x = (x << 13) ^ x;
 //       return (1.0f - ((x * (x * x * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
 //   }

	//static inline float PerlinNoise1D(float x)
 //   {
 //       int32_t i = static_cast<int32_t>(std::floor(x));
 //       float f = x - static_cast<float>(i);

 //       float g0 = Hash1D(i);
 //       float g1 = Hash1D(i + 1);

 //       float v0 = g0 * f;
 //       float v1 = g1 * (f - 1.0f);

 //       return Lerp(v0, v1, Quintic(f));
 //   }

} // namespace MathUtil
