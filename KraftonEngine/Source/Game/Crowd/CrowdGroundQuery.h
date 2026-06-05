#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class UStaticMeshComponent;
class UWorld;

struct FCrowdGroundBuildParams
{
	FName GroundActorTag = FName("CrowdGround");
	bool bAllowFallbackWithoutTag = true;
	float CellSize = 4.0f;
	float WalkableSlopeAngle = 60.0f;
};

struct FCrowdGroundSampleParams
{
	float TraceUp = 5.0f;
	float TraceDown = 50.0f;
	float HeightOffset = 0.0f;
};

struct FCrowdGroundHit
{
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	UStaticMeshComponent* Component = nullptr;
};

class FCrowdGroundQuery
{
public:
	void Rebuild(UWorld* World, const FCrowdGroundBuildParams& Params);
	void Clear();

	bool SampleGround(const FVector& Position, const FCrowdGroundSampleParams& Params, FCrowdGroundHit& OutHit) const;

	bool HasData() const { return !Triangles.empty(); }
	bool UsedFallbackWithoutTag() const { return bUsedFallbackWithoutTag; }
	int32 GetTriangleCount() const { return static_cast<int32>(Triangles.size()); }
	int32 GetCandidateComponentCount() const { return CandidateComponentCount; }

private:
	struct FGroundTriangle
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector C = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		UStaticMeshComponent* Component = nullptr;
	};

	static int64 MakeCellKey(int32 CellX, int32 CellY);
	int32 GetCellCoord(float Value) const;

	void AddStaticMeshComponent(UStaticMeshComponent* Component, float MinNormalZ);
	void RegisterTriangle(uint32 TriangleIndex);

private:
	TArray<FGroundTriangle> Triangles;
	TMap<int64, TArray<uint32>> Grid;
	float CellSize = 4.0f;
	bool bUsedFallbackWithoutTag = false;
	int32 CandidateComponentCount = 0;
};
