#pragma once

#include "Types/Array.h"
#include "Math/Vector.h"
#include "Scene/SceneTypes.h"

class FCamera;
class FScene;

struct FPlane
{
	FVector Normal = FVector::ForwardVector;
	float Distance = 0.0f;

	float GetSignedDistanceToPoint(const FVector& Point) const
	{
		return FVector::DotProduct(Normal, Point) + Distance;
	}
};

struct FFrustum
{
	FPlane Planes[6];
};

struct FVisibilityStats
{
	uint32 TotalPrimitiveCount = 0;
	uint32 FrustumVisibleCount = 0;
	uint32 SeedCount = 0;
	uint32 VisibleCount = 0;
	uint32 OccludedCount = 0;
	uint32 GpuCandidateCount = 0;
	uint32 GpuVisibleCount = 0;
	float ReadbackTimeMs = 0.0f;
};

struct FVisibilityFrameInput
{
	uint64 FrameNumber = 0;
	TArray<uint32> FrustumVisiblePrimitiveIndices;
	TArray<uint32> SeedPrimitiveIndices;
	TArray<uint32> CandidatePrimitiveIndices;
};

struct FVisibilityResults
{
	uint64 FrameNumber = 0;
	TArray<uint32> VisiblePrimitiveIndices;
	TArray<uint32> SeedPrimitiveIndices;
	FVisibilityStats Stats;
};

class FVisibilitySystem
{
public:
	~FVisibilitySystem();

	void Reset();
	void InvalidateHistory();
	void PrepareFrame(const FScene& InScene, const FCamera& InCamera, FVisibilityFrameInput& OutFrameInput, FVisibilityResults& OutResults);
	void FinalizeGpuResults(
		const FScene& InScene,
		const FVisibilityFrameInput& InFrameInput,
		const TArray<uint32>& InVisiblePrimitiveIndices,
		uint32 InGpuCandidateCount,
		uint32 InGpuVisibleCount,
		float InGpuReadbackTimeMs,
		FVisibilityResults& OutResults);

private:
	FFrustum BuildFrustum(const FCamera& InCamera) const;
	bool IntersectsAABB(const FFrustum& InFrustum, const FVector& InBoxMin, const FVector& InBoxMax) const;
	void ComputeFrustumVisiblePrimitives(const TArray<FRenderItem>& RenderItems, const FCamera& InCamera, TArray<uint32>& OutVisibleIndices);
	void ComputeSeedVisibilityPrimitives(const TArray<FRenderItem>& RenderItems, const TArray<uint32>& InFrustumVisiblePrimitiveIndices, TArray<uint32>& OutSeedPrimitiveIndices);
	void UpdatePreviousVisibilityMask(size_t PrimitiveCount, const FVisibilityResults& InResults);
	void EnsurePreviousVisibilityMaskSize(size_t PrimitiveCount);
	void LogBuildResult(const FVisibilityResults& InResults) const;

private:
	uint64 NextFrameNumber = 1;
	FFrustum CachedFrustum;
	TArray<uint8> PreviousFrameVisibilityMask;
	bool bHistoryValid = false;
};
