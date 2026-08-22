#pragma once

#include "Types/Array.h"
#include "Math/Matrix.h"
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

struct FOcclusionTimingStats
{
	float DepthPrepassGpuTimeMs = 0.0f;
	float HzbBuildGpuTimeMs = 0.0f;
	float OcclusionCullGpuTimeMs = 0.0f;
	float ReadbackLatencyTimeMs = 0.0f;
	float ReadbackCopyCpuTimeMs = 0.0f;
};

struct FVisibilityStats
{
	uint32 TotalPrimitiveCount = 0;
	uint32 TotalClusterCount = 0;
	uint32 FrustumVisibleClusterCount = 0;
	uint32 CandidateClusterCount = 0;
	uint32 VisibleClusterCount = 0;
	uint32 OccludedClusterCount = 0;
	uint32 ExpandedVisiblePrimitiveCount = 0;
	uint32 VisibilityResultAgeFrames = 0;
	bool bHzbValid = false;
	bool bUsedOcclusion = false;
	float ReadbackTimeMs = 0.0f;
	FOcclusionTimingStats OcclusionTimings = {};
};

struct FVisibilityFrameInput
{
	uint64 FrameNumber = 0;
	uint32 TotalClusterCount = 0;
	bool bOcclusionValid = false;
	TArray<FVisibilityCluster> FrustumVisibleClusters;
	TArray<uint32> RefinedStaticClusterPrimitiveIndices;
	TArray<uint32> DynamicClusterPrimitiveIndices;
	TArray<uint32> CandidateClusterIndices;
};

struct FVisibilityResults
{
	uint64 FrameNumber = 0;
	TArray<uint32> VisiblePrimitiveIndices;
	TArray<uint32> VisibleLODIndices;
	TArray<uint32> VisibleClusterIndices;
	FVisibilityStats Stats;
};

class FVisibilitySystem
{
public:
	~FVisibilitySystem();

	void Reset();
	void InvalidateHistory();
	void Invalidate();
	void Build(const FScene& InScene, const FCamera& InCamera, FVisibilityResults& OutResults);
	void PrepareFrame(const FScene& InScene, const FCamera& InCamera, FVisibilityFrameInput& OutFrameInput, FVisibilityResults& OutResults);
	void FinalizeFrame(
		const FScene& InScene,
		const FCamera& InCamera,
		const FVisibilityFrameInput& InFrameInput,
		const TArray<uint32>& InVisibleClusterIndices,
		const FOcclusionTimingStats& InOcclusionTimings,
		bool bUsedOcclusion,
		FVisibilityResults& OutResults);

private:
	FFrustum BuildFrustum(const FCamera& InCamera) const;
	bool IntersectsAABB(const FFrustum& InFrustum, const FVector& InBoxMin, const FVector& InBoxMax) const;
	void ComputeFrustumVisibleClusters(const FScene& InScene, const FCamera& InCamera, FVisibilityFrameInput& OutFrameInput);
	bool ShouldRefineClusterForOcclusion(const FCamera& InCamera, const FVisibilityCluster& InCluster) const;
	void AppendFrustumVisibleCluster(const FScene& InScene, const FCamera& InCamera, const FVisibilityCluster& InCluster, FVisibilityFrameInput& OutFrameInput);
	void ExpandClustersToPrimitives(const FScene& InScene, const FVisibilityFrameInput& InFrameInput, const TArray<uint32>& InVisibleClusterIndices, TArray<uint32>& OutVisiblePrimitiveIndices);
	void EnsurePrimitiveScratchMaskSize(size_t PrimitiveCount);
	void LogBuildResult(const FVisibilityResults& InResults) const;

private:
	uint64 NextFrameNumber = 1;
	FFrustum CachedFrustum;
	FMatrix CachedViewMatrix = FMatrix::Identity;
	FMatrix CachedViewProjectionMatrix = FMatrix::Identity;
	TArray<uint8> PrimitiveScratchVisibilityMask;
	bool bHistoryValid = false;
};
