#pragma once
#include <cmath>

struct FVector3
{
	float x, y, z;
	FVector3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

	FVector3 operator-(FVector3 other)
	{
		FVector3 result;
		result = { x - other.x, y - other.y, z - other.z };
		return result;
	}

	FVector3 operator+(FVector3 other)
	{
		FVector3 result;
		result = { x + other.x, y + other.y, z + other.z };
		return result;
	}

	FVector3 operator*(float tmp) const {
		FVector3 result;
		result = { x * tmp, y * tmp, z * tmp };
		return result;
	}

	float Dot(FVector3 other) // 내적
	{
		return x * other.x + y * other.y + z * other.z;
	}

	float Length() // 길이
	{
		return sqrt(x * x + y * y + z * z);
	}

	FVector3 Normalize() // 정규화
	{
		FVector3 normal;
		float len = Length();
		if (len > 0.0f)
		{
			normal = { x / len, y / len, z / len };
		}
		else
		{
			normal = { 0, 0, 0 };
		}

		return normal;
	}
};