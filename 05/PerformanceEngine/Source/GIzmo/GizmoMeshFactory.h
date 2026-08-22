#pragma once

#include "Math/Vector.h"
#include "Types/Array.h"
#include "Types/PlatformTypes.h"

struct FGizmoColor
{
	float R = 1.0f;
	float G = 1.0f;
	float B = 1.0f;
	float A = 1.0f;
};

struct FGizmoVertex
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FGizmoColor Color = {};
};

struct FGizmoMesh
{
	TArray<FGizmoVertex> Vertices;
	TArray<uint32> Indices;

	bool IsValid() const
	{
		return !Vertices.empty() && !Indices.empty();
	}
};

enum class EGizmoAxisId : uint8
{
	None = 0,
	X,
	Y,
	Z,
	XYZ,
	Screen,
};

struct FTranslationGizmoDesc
{
	float UniformScale = 1.0f;
	int32 TransformGizmoSize = 0;
	bool bIncludeScreenHandle = true;
	bool bLeftUpForward = false;
};

struct FRotationGizmoDesc
{
	float UniformScale = 1.0f;
	int32 TransformGizmoSize = 0;
	FVector CameraDirection = FVector(-1.0f, -1.0f, -1.0f);
	FVector ViewUp = FVector(0.0f, 1.0f, 0.0f);
	FVector ViewRight = FVector(1.0f, 0.0f, 0.0f);
	bool bOrthographic = false;
	bool bFullAxisRings = false;
	bool bIncludeInnerDisk = false;
	bool bIncludeScreenRing = true;
	bool bIncludeArcball = false;
	bool bDragging = false;
	EGizmoAxisId ActiveAxis = EGizmoAxisId::None;
	float DeltaRotationDegrees = 0.0f;
};

struct FScaleGizmoDesc
{
	float UniformScale = 1.0f;
	int32 TransformGizmoSize = 0;
	bool bIncludeCenterCube = true;
	bool bLeftUpForward = false;
};

struct FTranslationGizmoMeshes
{
	FGizmoMesh AxisX;
	FGizmoMesh AxisY;
	FGizmoMesh AxisZ;
	FGizmoMesh PlaneXY;
	FGizmoMesh PlaneXZ;
	FGizmoMesh PlaneYZ;
	FGizmoMesh ScreenSphere;
};

struct FRotationGizmoMeshes
{
	FGizmoMesh RingX;
	FGizmoMesh RingY;
	FGizmoMesh RingZ;
	FGizmoMesh ScreenRing;
	FGizmoMesh Arcball;
};

struct FScaleGizmoMeshes
{
	FGizmoMesh AxisX;
	FGizmoMesh AxisY;
	FGizmoMesh AxisZ;
	FGizmoMesh PlaneXY;
	FGizmoMesh PlaneXZ;
	FGizmoMesh PlaneYZ;
	FGizmoMesh CenterCube;
};

FTranslationGizmoMeshes GenerateTranslationGizmoMeshes(const FTranslationGizmoDesc& InDesc = {});
FRotationGizmoMeshes GenerateRotationGizmoMeshes(const FRotationGizmoDesc& InDesc = {});
FScaleGizmoMeshes GenerateScaleGizmoMeshes(const FScaleGizmoDesc& InDesc = {});
