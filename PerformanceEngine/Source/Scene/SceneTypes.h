#pragma once

#include <limits>
#include <memory>

#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "Types/PlatformTypes.h"
#include "Types/String.h"

class FStaticMesh;

struct FRay
{
	// simd 연산을 사용하다 보니 union을 썼는데 내부적으로 사용자 정의 타입이 존재하면 기본 생성자가 자동으로 삭제됨
	FRay() : Origin(FVector::ZeroVector), Direction(FVector::ForwardVector), InvDirection(), T(FLT_MAX) {};

	union
	{
		struct { FVector Origin; float Dummy0; };
		__m128 Origin4;
	};

	union
	{
		struct { FVector Direction; float Dummy1; };
		__m128 Direction4;
	};

	union
	{
		struct { FVector InvDirection; float Dummy2; };
		__m128 InvDirection4;
	};

	float T = FLT_MAX;

};

struct FPickHit
{
	int32 PrimitiveId = -1;
	int32 PrimitiveIndex = -1;
	float DistanceSquared = std::numeric_limits<float>::max();
	FVector WorldPosition = FVector::ZeroVector;
};

struct FSceneCameraInitData
{
	FTransform Transform = FTransform::Identity;
	float FovDegrees = 60.0f;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;
};

struct FRenderItem
{
	int32 PrimitiveId = -1;
	FString MeshAssetPath;
	FTransform Transform = FTransform::Identity;
	FVector WorldBoundsMin = FVector::ZeroVector;
	FVector WorldBoundsMax = FVector::ZeroVector;
	std::shared_ptr<FStaticMesh> StaticMesh;

	int BVHLeafIndex = -1;
	FVector LooseBoundsMin = FVector::ZeroVector;
	FVector LooseBoundsMax = FVector::ZeroVector;
};

struct FScenePrimitiveColdData
{
	FString MeshAssetPath;
	std::shared_ptr<FStaticMesh> StaticMeshOwner;
};

struct FScenePrimitiveRuntimeData
{
	int32 PrimitiveId = -1;
	FMatrix WorldMatrix = FMatrix::Identity;
	FMatrix InverseWorldMatrix = FMatrix::Identity;

	FVector WorldBoundsMin = FVector::ZeroVector;
	FVector WorldBoundsMax = FVector::ZeroVector;
	FStaticMesh* StaticMesh = nullptr;
};
