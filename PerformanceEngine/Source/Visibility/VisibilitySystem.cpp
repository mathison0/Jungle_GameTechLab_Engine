#include "Visibility/VisibilitySystem.h"

#include <algorithm>
#include <sstream>

#include <Windows.h>

#include "Camera/Camera.h"
#include "Math/MathUtility.h"
#include "Math/Vector.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"

FVisibilitySystem::~FVisibilitySystem()
= default;

void FVisibilitySystem::Reset()
{
	NextFrameNumber = 1;
	CachedFrustum = FFrustum();
	PreviousFrameVisibilityMask.clear();
	bHistoryValid = false;
}

void FVisibilitySystem::InvalidateHistory()
{
	std::fill(PreviousFrameVisibilityMask.begin(), PreviousFrameVisibilityMask.end(), 0);
	bHistoryValid = false;
}

void FVisibilitySystem::PrepareFrame(const FScene& InScene, const FCamera& InCamera, FVisibilityFrameInput& OutFrameInput, FVisibilityResults& OutResults)
{
	const TArray<FRenderItem>& RenderItems = InScene.GetRenderItems();
	EnsurePreviousVisibilityMaskSize(RenderItems.size());

	OutFrameInput = FVisibilityFrameInput();
	OutResults = FVisibilityResults();

	OutFrameInput.FrameNumber = NextFrameNumber++;
	OutResults.FrameNumber = OutFrameInput.FrameNumber;
	OutResults.Stats.TotalPrimitiveCount = static_cast<uint32>(RenderItems.size());

	ComputeFrustumVisiblePrimitives(RenderItems, InCamera, OutFrameInput.FrustumVisiblePrimitiveIndices);
	OutResults.Stats.FrustumVisibleCount = static_cast<uint32>(OutFrameInput.FrustumVisiblePrimitiveIndices.size());

	OutFrameInput.CandidatePrimitiveIndices = OutFrameInput.FrustumVisiblePrimitiveIndices;
	ComputeSeedVisibilityPrimitives(
		RenderItems,
		OutFrameInput.FrustumVisiblePrimitiveIndices,
		OutFrameInput.SeedPrimitiveIndices);

	OutResults.SeedPrimitiveIndices = OutFrameInput.SeedPrimitiveIndices;
	OutResults.Stats.SeedCount = static_cast<uint32>(OutFrameInput.SeedPrimitiveIndices.size());
}

void FVisibilitySystem::FinalizeGpuResults(
	const FScene& InScene,
	const FVisibilityFrameInput& InFrameInput,
	const TArray<uint32>& InVisiblePrimitiveIndices,
	uint32 InGpuCandidateCount,
	uint32 InGpuVisibleCount,
	float InGpuReadbackTimeMs,
	FVisibilityResults& OutResults)
{
	const TArray<FRenderItem>& RenderItems = InScene.GetRenderItems();
	EnsurePreviousVisibilityMaskSize(RenderItems.size());

	OutResults.FrameNumber = InFrameInput.FrameNumber;
	OutResults.VisiblePrimitiveIndices = InVisiblePrimitiveIndices;
	OutResults.SeedPrimitiveIndices = InFrameInput.SeedPrimitiveIndices;

	OutResults.Stats.TotalPrimitiveCount = static_cast<uint32>(RenderItems.size());
	OutResults.Stats.FrustumVisibleCount = static_cast<uint32>(InFrameInput.FrustumVisiblePrimitiveIndices.size());
	OutResults.Stats.SeedCount = static_cast<uint32>(InFrameInput.SeedPrimitiveIndices.size());
	OutResults.Stats.VisibleCount = static_cast<uint32>(InVisiblePrimitiveIndices.size());
	OutResults.Stats.OccludedCount = static_cast<uint32>(InGpuCandidateCount - InGpuVisibleCount);
	OutResults.Stats.GpuCandidateCount = InGpuCandidateCount;
	OutResults.Stats.GpuVisibleCount = InGpuVisibleCount;
	OutResults.Stats.ReadbackTimeMs = InGpuReadbackTimeMs;

	UpdatePreviousVisibilityMask(RenderItems.size(), OutResults);
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

void FVisibilitySystem::ComputeFrustumVisiblePrimitives(const TArray<FRenderItem>& RenderItems, const FCamera& InCamera, TArray<uint32>& OutVisibleIndices)
{
	OutVisibleIndices.clear();
	OutVisibleIndices.reserve(RenderItems.size());
	CachedFrustum = BuildFrustum(InCamera);

	for (uint32 PrimitiveIndex = 0; PrimitiveIndex < static_cast<uint32>(RenderItems.size()); ++PrimitiveIndex)
	{
		const FRenderItem& Item = RenderItems[PrimitiveIndex];
		if (Item.StaticMesh == nullptr || !Item.StaticMesh->IsValid())
		{
			continue;
		}

		if (!IntersectsAABB(CachedFrustum, Item.WorldBoundsMin, Item.WorldBoundsMax))
		{
			continue;
		}

		OutVisibleIndices.push_back(PrimitiveIndex);
	}
}

void FVisibilitySystem::ComputeSeedVisibilityPrimitives(
	const TArray<FRenderItem>& RenderItems,
	const TArray<uint32>& InFrustumVisiblePrimitiveIndices,
	TArray<uint32>& OutSeedPrimitiveIndices)
{
	OutSeedPrimitiveIndices.clear();
	if (!bHistoryValid || InFrustumVisiblePrimitiveIndices.empty())
	{
		return;
	}
	for (uint32 PrimitiveIndex : InFrustumVisiblePrimitiveIndices)
	{
		if (PrimitiveIndex >= RenderItems.size())
		{
			continue;
		}

		const bool bWasVisibleLastFrame = PrimitiveIndex < PreviousFrameVisibilityMask.size()
			&& PreviousFrameVisibilityMask[PrimitiveIndex] != 0;
		if (!bWasVisibleLastFrame)
		{
			continue;
		}

		OutSeedPrimitiveIndices.push_back(PrimitiveIndex);
	}
}

void FVisibilitySystem::UpdatePreviousVisibilityMask(size_t PrimitiveCount, const FVisibilityResults& InResults)
{
	EnsurePreviousVisibilityMaskSize(PrimitiveCount);
	std::fill(PreviousFrameVisibilityMask.begin(), PreviousFrameVisibilityMask.end(), 0);

	for (uint32 PrimitiveIndex : InResults.VisiblePrimitiveIndices)
	{
		if (PrimitiveIndex < PreviousFrameVisibilityMask.size())
		{
			PreviousFrameVisibilityMask[PrimitiveIndex] = 1;
		}
	}

	bHistoryValid = true;
}

void FVisibilitySystem::EnsurePreviousVisibilityMaskSize(size_t PrimitiveCount)
{
	if (PreviousFrameVisibilityMask.size() != PrimitiveCount)
	{
		PreviousFrameVisibilityMask.assign(PrimitiveCount, 0);
	}
}

void FVisibilitySystem::LogBuildResult(const FVisibilityResults& InResults) const
{
	/*
	std::ostringstream Stream;
	Stream
		<< "VisibilitySystem: "
		<< "GPU"
		<< " - Total: " << InResults.Stats.TotalPrimitiveCount
		<< ", Frustum: " << InResults.Stats.FrustumVisibleCount
		<< ", Seed: " << InResults.Stats.SeedCount
		<< ", Visible: " << InResults.Stats.VisibleCount
		<< ", Occluded: " << InResults.Stats.OccludedCount
		<< ", GPUCand: " << InResults.Stats.GpuCandidateCount
		<< ", GPUVisible: " << InResults.Stats.GpuVisibleCount
		<< ", ReadbackMs: " << InResults.Stats.ReadbackTimeMs
		<< ", FinalVisible: " << InResults.VisiblePrimitiveIndices.size()
		<< '\n';

	OutputDebugStringA(Stream.str().c_str());*/
}
