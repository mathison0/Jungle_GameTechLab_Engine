#pragma once

#include <limits>
#include <memory>

#include "Math/Matrix.h"
#include "Math/Transform.h"
#include "StaticMesh/StaticMesh.h"
#include "Types/PlatformTypes.h"
#include "Types/String.h"

class FStaticMesh;

struct FRay
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
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

struct FVisibilityCluster
{
	FVector BoundsMin = FVector::ZeroVector;
	FVector BoundsMax = FVector::ZeroVector;
	uint32 PrimitiveOffset = 0;
	uint32 PrimitiveCount = 0;
	int32 SourceBvhNodeIndex = -1;
	bool bUsesFramePrimitiveIndices = false;
	bool bDynamic = false;
};

struct FRenderItem
{
	int32 PrimitiveId = -1;
	FString MeshAssetPath;
	FTransform Transform = FTransform::Identity;
	FVector WorldBoundsMin = FVector::ZeroVector;
	FVector WorldBoundsMax = FVector::ZeroVector;
	FBoundingSphere WorldBoundsSphere;
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
	FBoundingSphere WorldBoundsSphere;
	FStaticMesh* StaticMesh = nullptr;
};
