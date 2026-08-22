#include "Visibility/VisibilitySystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

#include <Windows.h>

#include "Camera/Camera.h"
#include "Math/MathUtility.h"
#include "Math/Vector.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"

namespace
{
	constexpr float ClusterRefineMinScreenFraction = 0.2f;
	constexpr float ClusterRefineNearDepthEpsilon = 1.0e-3f;
	constexpr std::array<float, 2> ScreenSizeLODCutoffs = { 0.2f, 0.08f };

	FVector BuildAabbCorner(const FVisibilityCluster& InCluster, uint32 InCornerIndex)
	{
		return FVector(
			(InCornerIndex & 1u) != 0u ? InCluster.BoundsMax.X : InCluster.BoundsMin.X,
			(InCornerIndex & 2u) != 0u ? InCluster.BoundsMax.Y : InCluster.BoundsMin.Y,
			(InCornerIndex & 4u) != 0u ? InCluster.BoundsMax.Z : InCluster.BoundsMin.Z);
	}

	uint32 SelectLODIndex(const FScenePrimitiveRuntimeData& InPrimitiveData, const FCamera& InCamera)
	{
		const FBoundingSphere& WorldSphere = InPrimitiveData.WorldBoundsSphere;
		if (WorldSphere.Radius <= 0.0f)
		{
			return 0;
		}

		const FVector CameraForward = InCamera.GetTransform().GetUnitAxis(EAxis::X).GetSafeNormal();
		const FVector CameraToCenter = WorldSphere.Center - InCamera.GetLocation();
		const float Depth = FVector::DotProduct(CameraToCenter, CameraForward);
		const float SafeDepth = std::max(Depth, InCamera.GetNearClip());
		if (SafeDepth <= 0.0f)
		{
			return 0;
		}

		const float ProjectionScaleY = 1.0f / std::tan(FMath::DegreesToRadians(InCamera.GetFOV()) * 0.5f);
		const float ScreenDiameterRatio = (2.0f * WorldSphere.Radius * ProjectionScaleY) / SafeDepth;
		if (ScreenDiameterRatio >= ScreenSizeLODCutoffs[0])
		{
			return 0;
		}

		if (ScreenDiameterRatio >= ScreenSizeLODCutoffs[1])
		{
			return 1;
		}

		return 2;
	}
}

FVisibilitySystem::~FVisibilitySystem() = default;

void FVisibilitySystem::Reset()
{
	NextFrameNumber = 1;
	CachedFrustum = FFrustum();
	PrimitiveScratchVisibilityMask.clear();
	bHistoryValid = false;
}

void FVisibilitySystem::InvalidateHistory()
{
	bHistoryValid = false;
}

void FVisibilitySystem::Invalidate()
{
	InvalidateHistory();
}

void FVisibilitySystem::Build(const FScene& InScene, const FCamera& InCamera, FVisibilityResults& OutResults)
{
	FVisibilityFrameInput FrameInput;
	PrepareFrame(InScene, InCamera, FrameInput, OutResults);

	TArray<uint32> VisibleClusterIndices;
	VisibleClusterIndices.reserve(FrameInput.FrustumVisibleClusters.size());
	for (uint32 ClusterIndex = 0; ClusterIndex < static_cast<uint32>(FrameInput.FrustumVisibleClusters.size()); ++ClusterIndex)
	{
		VisibleClusterIndices.push_back(ClusterIndex);
	}

	FinalizeFrame(
		InScene,
		InCamera,
		FrameInput,
		VisibleClusterIndices,
		FOcclusionTimingStats(),
		false,
		OutResults);

	bHistoryValid = false;
	OutResults.Stats.bHzbValid = false;
}

void FVisibilitySystem::PrepareFrame(const FScene& InScene, const FCamera& InCamera, FVisibilityFrameInput& OutFrameInput, FVisibilityResults& OutResults)
{
	OutFrameInput = FVisibilityFrameInput();
	OutResults = FVisibilityResults();

	OutFrameInput.FrameNumber = NextFrameNumber++;
	OutFrameInput.bOcclusionValid = bHistoryValid;
	OutResults.FrameNumber = OutFrameInput.FrameNumber;
	OutResults.Stats.TotalPrimitiveCount = static_cast<uint32>(InScene.GetPrimitiveCount());
	CachedViewMatrix = InCamera.GetViewMatrix();
	CachedViewProjectionMatrix = CachedViewMatrix * InCamera.GetProjectionMatrix();

	ComputeFrustumVisibleClusters(InScene, InCamera, OutFrameInput);

	OutResults.Stats.TotalClusterCount = OutFrameInput.TotalClusterCount;
	OutResults.Stats.FrustumVisibleClusterCount = static_cast<uint32>(OutFrameInput.FrustumVisibleClusters.size());
	OutResults.Stats.CandidateClusterCount = static_cast<uint32>(OutFrameInput.CandidateClusterIndices.size());
	OutResults.Stats.bHzbValid = OutFrameInput.bOcclusionValid;
}

void FVisibilitySystem::FinalizeFrame(
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityFrameInput& InFrameInput,
	const TArray<uint32>& InVisibleClusterIndices,
	const FOcclusionTimingStats& InOcclusionTimings,
	bool bUsedOcclusion,
	FVisibilityResults& OutResults)
{
	EnsurePrimitiveScratchMaskSize(InScene.GetPrimitiveCount());

	OutResults.FrameNumber = InFrameInput.FrameNumber;
	if (bUsedOcclusion)
	{
		OutResults.VisibleClusterIndices = InVisibleClusterIndices;
	}
	else
	{
		OutResults.VisibleClusterIndices.clear();
		OutResults.VisibleClusterIndices.reserve(InFrameInput.FrustumVisibleClusters.size());
		for (uint32 ClusterIndex = 0; ClusterIndex < static_cast<uint32>(InFrameInput.FrustumVisibleClusters.size()); ++ClusterIndex)
		{
			OutResults.VisibleClusterIndices.push_back(ClusterIndex);
		}
	}

	ExpandClustersToPrimitives(InScene, InFrameInput, OutResults.VisibleClusterIndices, OutResults.VisiblePrimitiveIndices);
	OutResults.VisibleLODIndices.clear();
	OutResults.VisibleLODIndices.reserve(OutResults.VisiblePrimitiveIndices.size());

	const TArray<FScenePrimitiveRuntimeData>& PrimitiveRuntimeData = InScene.GetPrimitiveRuntimeData();
	for (uint32 PrimitiveIndex : OutResults.VisiblePrimitiveIndices)
	{
		if (PrimitiveIndex >= PrimitiveRuntimeData.size())
		{
			OutResults.VisibleLODIndices.push_back(0);
			continue;
		}

		const FScenePrimitiveRuntimeData& PrimitiveData = PrimitiveRuntimeData[PrimitiveIndex];
		FStaticMesh* StaticMesh = PrimitiveData.StaticMesh;
		if (StaticMesh == nullptr)
		{
			OutResults.VisibleLODIndices.push_back(0);
			continue;
		}

		const uint32 LODCount = StaticMesh->GetLODCount();
		if (LODCount == 0)
		{
			OutResults.VisibleLODIndices.push_back(0);
			continue;
		}

		OutResults.VisibleLODIndices.push_back(std::min(SelectLODIndex(PrimitiveData, InCamera), LODCount - 1));
	}

	OutResults.Stats.TotalPrimitiveCount = static_cast<uint32>(InScene.GetPrimitiveCount());
	OutResults.Stats.TotalClusterCount = InFrameInput.TotalClusterCount;
	OutResults.Stats.FrustumVisibleClusterCount = static_cast<uint32>(InFrameInput.FrustumVisibleClusters.size());
	OutResults.Stats.CandidateClusterCount = static_cast<uint32>(InFrameInput.CandidateClusterIndices.size());
	OutResults.Stats.VisibleClusterCount = static_cast<uint32>(OutResults.VisibleClusterIndices.size());
	OutResults.Stats.OccludedClusterCount = OutResults.Stats.CandidateClusterCount >= OutResults.Stats.VisibleClusterCount
		? (OutResults.Stats.CandidateClusterCount - OutResults.Stats.VisibleClusterCount)
		: 0u;
	OutResults.Stats.ExpandedVisiblePrimitiveCount = static_cast<uint32>(OutResults.VisiblePrimitiveIndices.size());
	OutResults.Stats.VisibilityResultAgeFrames = 0;
	OutResults.Stats.bHzbValid = InFrameInput.bOcclusionValid;
	OutResults.Stats.bUsedOcclusion = bUsedOcclusion;
	OutResults.Stats.ReadbackTimeMs = InOcclusionTimings.ReadbackCopyCpuTimeMs;
	OutResults.Stats.OcclusionTimings = InOcclusionTimings;

	bHistoryValid = true;
	LogBuildResult(OutResults);
}

FFrustum FVisibilitySystem::BuildFrustum(const FCamera& InCamera) const
{
	FFrustum Frustum;

	const FTransform& CameraTransform = InCamera.GetTransform();
	const FVector CameraPos = CameraTransform.GetLocation();
	const FVector Forward = CameraTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	const FVector Right = CameraTransform.GetUnitAxis(EAxis::Y).GetSafeNormal();
	const FVector Up = CameraTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();

	const float NearDist = InCamera.GetNearClip();
	const float FarDist = InCamera.GetFarClip();
	const float HalfFovRad = FMath::DegreesToRadians(InCamera.GetFOV() * 0.5f);

	const float NearHalfHeight = std::tan(HalfFovRad) * NearDist;
	const float NearHalfWidth = NearHalfHeight * InCamera.GetAspectRatio();

	const float FarHalfHeight = std::tan(HalfFovRad) * FarDist;
	const float FarHalfWidth = FarHalfHeight * InCamera.GetAspectRatio();

	const FVector NearCenter = CameraPos + Forward * NearDist;
	const FVector FarCenter = CameraPos + Forward * FarDist;
	const FVector FrustumCenter = CameraPos + Forward * ((NearDist + FarDist) * 0.5f);

	const FVector NTL = NearCenter + Up * NearHalfHeight - Right * NearHalfWidth;
	const FVector NTR = NearCenter + Up * NearHalfHeight + Right * NearHalfWidth;
	const FVector NBL = NearCenter - Up * NearHalfHeight - Right * NearHalfWidth;
	const FVector NBR = NearCenter - Up * NearHalfHeight + Right * NearHalfWidth;

	auto MakePlaneFromPointNormal = [](const FVector& Point, const FVector& Normal)
	{
		FPlane Plane;
		Plane.Normal = Normal.GetSafeNormal();
		Plane.Distance = -FVector::DotProduct(Plane.Normal, Point);
		return Plane;
	};

	auto MakePlaneFromPoints = [](const FVector& PointA, const FVector& PointB, const FVector& PointC, const FVector& InFrustumCenter)
	{
		FVector Normal = FVector::CrossProduct(PointB - PointA, PointC - PointA).GetSafeNormal();

		FPlane Plane;
		Plane.Normal = Normal;
		Plane.Distance = -FVector::DotProduct(Plane.Normal, PointA);

		if (Plane.GetSignedDistanceToPoint(InFrustumCenter) < 0.0f)
		{
			Plane.Normal *= -1.0f;
			Plane.Distance *= -1.0f;
		}

		return Plane;
	};

	Frustum.Planes[0] = MakePlaneFromPointNormal(NearCenter, Forward);
	Frustum.Planes[1] = MakePlaneFromPointNormal(FarCenter, -Forward);
	Frustum.Planes[2] = MakePlaneFromPoints(CameraPos, NTL, NBL, FrustumCenter);
	Frustum.Planes[3] = MakePlaneFromPoints(CameraPos, NBR, NTR, FrustumCenter);
	Frustum.Planes[4] = MakePlaneFromPoints(CameraPos, NTR, NTL, FrustumCenter);
	Frustum.Planes[5] = MakePlaneFromPoints(CameraPos, NBL, NBR, FrustumCenter);

	return Frustum;
}

bool FVisibilitySystem::IntersectsAABB(const FFrustum& InFrustum, const FVector& InBoxMin, const FVector& InBoxMax) const
{
	for (const FPlane& Plane : InFrustum.Planes)
	{
		FVector PositiveVertex = InBoxMin;
		if (Plane.Normal.X >= 0.0f) PositiveVertex.X = InBoxMax.X;
		if (Plane.Normal.Y >= 0.0f) PositiveVertex.Y = InBoxMax.Y;
		if (Plane.Normal.Z >= 0.0f) PositiveVertex.Z = InBoxMax.Z;

		if (Plane.GetSignedDistanceToPoint(PositiveVertex) < 0.0f)
		{
			return false;
		}
	}

	return true;
}

void FVisibilitySystem::ComputeFrustumVisibleClusters(const FScene& InScene, const FCamera& InCamera, FVisibilityFrameInput& OutFrameInput)
{
	OutFrameInput.FrustumVisibleClusters.clear();
	OutFrameInput.RefinedStaticClusterPrimitiveIndices.clear();
	OutFrameInput.DynamicClusterPrimitiveIndices.clear();
	OutFrameInput.CandidateClusterIndices.clear();
	CachedFrustum = BuildFrustum(InCamera);

	const TArray<FVisibilityCluster>& StaticClusters = InScene.GetStaticVisibilityClusters();
	OutFrameInput.FrustumVisibleClusters.reserve(StaticClusters.size());
	for (const FVisibilityCluster& Cluster : StaticClusters)
	{
		AppendFrustumVisibleCluster(InScene, InCamera, Cluster, OutFrameInput);
	}

	TArray<FVisibilityCluster> DynamicClusters;
	InScene.BuildDynamicVisibilityClusters(DynamicClusters, OutFrameInput.DynamicClusterPrimitiveIndices);
	OutFrameInput.TotalClusterCount = static_cast<uint32>(StaticClusters.size() + DynamicClusters.size());
	OutFrameInput.FrustumVisibleClusters.reserve(OutFrameInput.FrustumVisibleClusters.size() + DynamicClusters.size());
	for (const FVisibilityCluster& Cluster : DynamicClusters)
	{
		if (!IntersectsAABB(CachedFrustum, Cluster.BoundsMin, Cluster.BoundsMax))
		{
			continue;
		}

		OutFrameInput.FrustumVisibleClusters.push_back(Cluster);
	}

	if (!OutFrameInput.bOcclusionValid)
	{
		return;
	}

	OutFrameInput.CandidateClusterIndices.reserve(OutFrameInput.FrustumVisibleClusters.size());
	for (uint32 ClusterIndex = 0; ClusterIndex < static_cast<uint32>(OutFrameInput.FrustumVisibleClusters.size()); ++ClusterIndex)
	{
		OutFrameInput.CandidateClusterIndices.push_back(ClusterIndex);
	}
}

bool FVisibilitySystem::ShouldRefineClusterForOcclusion(const FCamera& InCamera, const FVisibilityCluster& InCluster) const
{
	if (!bHistoryValid
		|| InCluster.bDynamic
		|| InCluster.SourceBvhNodeIndex < 0
		|| InCluster.PrimitiveCount <= 1)
	{
		return false;
	}

	float MinNdcX = 1.0f;
	float MinNdcY = 1.0f;
	float MaxNdcX = -1.0f;
	float MaxNdcY = -1.0f;

	for (uint32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector Corner = BuildAabbCorner(InCluster, CornerIndex);
		const FVector ViewSpace = CachedViewMatrix.TransformPosition(Corner);
		if (ViewSpace.Z <= (InCamera.GetNearClip() + ClusterRefineNearDepthEpsilon))
		{
			return false;
		}

		const FVector Ndc = CachedViewProjectionMatrix.TransformPosition(Corner);
		MinNdcX = std::min(MinNdcX, std::clamp(Ndc.X, -1.0f, 1.0f));
		MinNdcY = std::min(MinNdcY, std::clamp(Ndc.Y, -1.0f, 1.0f));
		MaxNdcX = std::max(MaxNdcX, std::clamp(Ndc.X, -1.0f, 1.0f));
		MaxNdcY = std::max(MaxNdcY, std::clamp(Ndc.Y, -1.0f, 1.0f));
	}

	const float ScreenWidthFraction = std::max(0.0f, (MaxNdcX - MinNdcX) * 0.5f);
	const float ScreenHeightFraction = std::max(0.0f, (MaxNdcY - MinNdcY) * 0.5f);
	return std::max(ScreenWidthFraction, ScreenHeightFraction) >= ClusterRefineMinScreenFraction;
}

void FVisibilitySystem::AppendFrustumVisibleCluster(
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityCluster& InCluster,
	FVisibilityFrameInput& OutFrameInput)
{
	if (!IntersectsAABB(CachedFrustum, InCluster.BoundsMin, InCluster.BoundsMax))
	{
		return;
	}

	if (ShouldRefineClusterForOcclusion(InCamera, InCluster))
	{
		TArray<FVisibilityCluster> RefinedChildren;
		if (InScene.TryRefineVisibilityCluster(InCluster, RefinedChildren, OutFrameInput.RefinedStaticClusterPrimitiveIndices))
		{
			for (const FVisibilityCluster& ChildCluster : RefinedChildren)
			{
				if (!IntersectsAABB(CachedFrustum, ChildCluster.BoundsMin, ChildCluster.BoundsMax))
				{
					continue;
				}

				OutFrameInput.FrustumVisibleClusters.push_back(ChildCluster);
			}
			return;
		}
	}

	OutFrameInput.FrustumVisibleClusters.push_back(InCluster);
}

void FVisibilitySystem::ExpandClustersToPrimitives(
	const FScene& InScene,
	const FVisibilityFrameInput& InFrameInput,
	const TArray<uint32>& InVisibleClusterIndices,
	TArray<uint32>& OutVisiblePrimitiveIndices)
{
	OutVisiblePrimitiveIndices.clear();
	EnsurePrimitiveScratchMaskSize(InScene.GetPrimitiveCount());
	std::fill(PrimitiveScratchVisibilityMask.begin(), PrimitiveScratchVisibilityMask.end(), 0);

	const TArray<uint32>& StaticClusterPrimitiveIndices = InScene.GetStaticClusterPrimitiveIndices();
	const TArray<FScenePrimitiveRuntimeData>& PrimitiveRuntimeData = InScene.GetPrimitiveRuntimeData();

	for (uint32 ClusterIndex : InVisibleClusterIndices)
	{
		if (ClusterIndex >= InFrameInput.FrustumVisibleClusters.size())
		{
			continue;
		}

		const FVisibilityCluster& Cluster = InFrameInput.FrustumVisibleClusters[ClusterIndex];
		const TArray<uint32>& ClusterPrimitiveIndices = Cluster.bDynamic
			? InFrameInput.DynamicClusterPrimitiveIndices
			: (Cluster.bUsesFramePrimitiveIndices ? InFrameInput.RefinedStaticClusterPrimitiveIndices : StaticClusterPrimitiveIndices);
		const uint32 PrimitiveEnd = Cluster.PrimitiveOffset + Cluster.PrimitiveCount;
		if (PrimitiveEnd > ClusterPrimitiveIndices.size())
		{
			continue;
		}

		for (uint32 PrimitiveCursor = Cluster.PrimitiveOffset; PrimitiveCursor < PrimitiveEnd; ++PrimitiveCursor)
		{
			const uint32 PrimitiveIndex = ClusterPrimitiveIndices[PrimitiveCursor];
			if (PrimitiveIndex >= PrimitiveScratchVisibilityMask.size()
				|| PrimitiveIndex >= PrimitiveRuntimeData.size())
			{
				continue;
			}

			if (!Cluster.bDynamic && InScene.IsPrimitiveDynamic(PrimitiveIndex))
			{
				continue;
			}

			const FScenePrimitiveRuntimeData& Primitive = PrimitiveRuntimeData[PrimitiveIndex];
			if (Primitive.StaticMesh == nullptr || !Primitive.StaticMesh->IsValid())
			{
				continue;
			}

			if (PrimitiveScratchVisibilityMask[PrimitiveIndex] != 0)
			{
				continue;
			}

			PrimitiveScratchVisibilityMask[PrimitiveIndex] = 1;
			OutVisiblePrimitiveIndices.push_back(PrimitiveIndex);
		}
	}
}

void FVisibilitySystem::EnsurePrimitiveScratchMaskSize(size_t PrimitiveCount)
{
	if (PrimitiveScratchVisibilityMask.size() != PrimitiveCount)
	{
		PrimitiveScratchVisibilityMask.assign(PrimitiveCount, 0);
	}
}

void FVisibilitySystem::LogBuildResult(const FVisibilityResults& InResults) const
{
	/*
	std::ostringstream Stream;
	Stream
		<< "VisibilitySystem: "
		<< "ClustersTotal: " << InResults.Stats.TotalClusterCount
		<< ", ClustersFrustum: " << InResults.Stats.FrustumVisibleClusterCount
		<< ", ClustersCand: " << InResults.Stats.CandidateClusterCount
		<< ", ClustersVisible: " << InResults.Stats.VisibleClusterCount
		<< ", PrimsVisible: " << InResults.Stats.ExpandedVisiblePrimitiveCount
		<< ", HzbValid: " << InResults.Stats.bHzbValid
		<< ", UsedOcclusion: " << InResults.Stats.bUsedOcclusion
		<< ", HzbMs: " << InResults.Stats.OcclusionTimings.HzbBuildGpuTimeMs
		<< ", CullMs: " << InResults.Stats.OcclusionTimings.OcclusionCullGpuTimeMs
		<< ", WaitMs: " << InResults.Stats.OcclusionTimings.ReadbackLatencyTimeMs
		<< ", ReadbackMs: " << InResults.Stats.OcclusionTimings.ReadbackCopyCpuTimeMs
		<< '\n';
	OutputDebugStringA(Stream.str().c_str());
	*/
}
