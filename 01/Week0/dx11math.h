#pragma once
/// <summary>
/// STL 관련 헤더들은 여기에 포함하는 걸로 통일하겠습니다. (관련 사항 상훈씨에게 문의)
/// </summary>
/// 
///여기서 오류가 발생한다면 상단 프로젝트-Nuget패키지관리-찾아보기에서 directxtk_desktop_win10을 검색해 설치해주세요.
#include <WICTextureLoader.h>
#include <DirectXMath.h>
using namespace DirectX;
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>
#include <string>

struct FVector3
{
public:
	float x, y, z;
	FVector3(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}

	FVector3 operator-() const
	{
		return FVector3(-x, -y, -z);
	}

	//산술 연산
	FVector3 operator+(const FVector3& other) const { return FVector3(x + other.x, y + other.y, z + other.z); }
	FVector3 operator-(const FVector3& other) const { return FVector3(x - other.x, y - other.y, z - other.z); }
	FVector3 operator*(float scalar) const { return FVector3(x * scalar, y * scalar, z * scalar); }
	FVector3 operator/(float scalar) const
	{
		//divided by zero 방지
		float inv = 1.0f / scalar;
		return FVector3(x * inv, y * inv, z * inv);
	}

	//대입 연산
	FVector3& operator=(const FVector3& other) { x = other.x; y = other.y; z = other.z; return *this; }

	FVector3& operator+=(const FVector3& other) { x += other.x; y += other.y; z += other.z; return *this; }
	FVector3& operator-=(const FVector3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
	FVector3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
	FVector3& operator/=(float scalar)
	{
		float inv = 1.0f / scalar;
		x *= inv; y *= inv; z *= inv;
		return *this;
	}

	//비교 연산
	bool operator==(const FVector3& other) const { return (x == other.x && y == other.y && z == other.z); }
	bool operator!=(const FVector3& other) const { return !(*this == other); }

	//내적
	float Dot(const FVector3& other) const
	{
		return (x * other.x) + (y * other.y) + (z * other.z);
	}
	static float Dot(const FVector3& v1, const FVector3& v2) { return v1.Dot(v2); }

	//외적
	FVector3 Cross(const FVector3& other) const
	{
		return FVector3(
			(y * other.z) - (z * other.y),
			(z * other.x) - (x * other.z),
			(x * other.y) - (y * other.x)
		);
	}
	static FVector3 Cross(const FVector3& v1, const FVector3& v2) { return v1.Cross(v2); }

	//벡터의 크기
	float LengthSquared() const { return (x * x) + (y * y) + (z * z); }
	float Length() const { return std::sqrt(LengthSquared()); }

	//정규화(벡터의 크기만 반영)
	void Normalize()
	{
		float lenSq = LengthSquared();
		if (lenSq > 1e-8f)
		{
			float invLen = 1.0f / std::sqrt(lenSq);
			x *= invLen; y *= invLen; z *= invLen;
		}
	}

	//정규화된 벡터 반환
	FVector3 GetNormalized() const
	{
		FVector3 result = *this;
		result.Normalize();
		return result;
	}

	static constexpr float PI = 3.14159265358979323846f;
	static float DegToRad(float degrees) { return degrees * (PI / 180.0f); }
	static float RadToDeg(float radians) { return radians * (180.0f / PI); }

	//x,y,z축 회전
	FVector3 RotateX(float angleRad) const
	{
		float c = std::cos(angleRad);
		float s = std::sin(angleRad);
		return FVector3(x, (y * c) - (z * s), (y * s) + (z * c));
	}
	FVector3 RotateY(float angleRad) const
	{
		float c = std::cos(angleRad);
		float s = std::sin(angleRad);
		return FVector3((x * c) + (z * s), y, (-x * s) + (z * c));
	}
	FVector3 RotateZ(float angleRad) const
	{
		float c = std::cos(angleRad);
		float s = std::sin(angleRad);
		return FVector3((x * c) - (y * s), (x * s) + (y * c), z);
	}

	//임의의 축(axis) 회전
	FVector3 RotateAroundAxis(const FVector3& axis, float angleRad) const
	{
		FVector3 k = axis.GetNormalized();
		float c = std::cos(angleRad);
		float s = std::sin(angleRad);

		FVector3 cross_kv = k.Cross(*this);
		float dot_kv = k.Dot(*this);

		return (*this * c) + (cross_kv * s) + (k * dot_kv * (1.0f - c));
	}

	//두 점 사이의 거리(FVector3를 point의 좌표로 사용할 때 homogeneous coordinate 변환 시 주의)
	float Distance(const FVector3& other) const
	{
		return (*this - other).Length();
	}
};

inline FVector3 operator*(float scalar, const FVector3& v)
{
	return v * scalar;
}