#include "Visibility/VisibilitySystem.h"

#include "Camera/Camera.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"
#include "Math/MathUtility.h"
#include "Math/Vector.h"

void FVisibilitySystem::Reset()
{
	NextFrameNumber = 1;

	CachedResults = FVisibilityResults();
	CachedFrustum = FFrustum();

	CachedCameraPosition = FVector::ZeroVector;
	CachedCameraForward = FVector::ForwardVector;
	CachedCameraUp = FVector::UpVector;
	CachedCameraRight = FVector::RightVector;

	CachedFOV = 0.0f;
	CachedAspect = 0.0f;
	CachedNearClip = 0.0f;
	CachedFarClip = 0.0f;

	bHasCachedVisibility = false;
}

void FVisibilitySystem::Build(const FScene& InScene, const FCamera& InCamera, FVisibilityResults& OutResults)
{
	const TArray<FRenderItem>& RenderItems = InScene.GetRenderItems();

	OutResults.FrameNumber = NextFrameNumber++;

	if (TryReuseCachedResults(InScene, InCamera, OutResults))
	{
		LogBuildResult(RenderItems.size(), OutResults.VisiblePrimitiveIndices.size(), true);
		return;
	}

	ComputeVisiblePrimitives(RenderItems, InCamera, OutResults);
	UpdateCache(InCamera, OutResults);

	LogBuildResult(RenderItems.size(), OutResults.VisiblePrimitiveIndices.size(), false);
}
namespace
{
	FPlane MakePlaneFromPointNormal(const FVector& Point, const FVector& Normal);
	FPlane MakePlaneFromPoints(const FVector& PointA, const FVector& PointB, const FVector& PointC, const FVector& InFrustumCenter);

	float AbsFloat(float Value)
	{
		return (Value < 0.0f) ? -Value : Value;
	}
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

	/*const FVector FTL = FarCenter + Up * FarHalfHeight - Right * FarHalfWidth;
	const FVector FTR = FarCenter + Up * FarHalfHeight + Right * FarHalfWidth;
	const FVector FBL = FarCenter - Up * FarHalfHeight - Right * FarHalfWidth;
	const FVector FBR = FarCenter - Up * FarHalfHeight + Right * FarHalfWidth;*/

	// inward normals
	Frustum.Planes[0] = MakePlaneFromPointNormal(NearCenter, Forward);     // near
	Frustum.Planes[1] = MakePlaneFromPointNormal(FarCenter, -Forward);     // far

	Frustum.Planes[2] = MakePlaneFromPoints(CameraPos, NTL, NBL, FrustumCenter); // left
	Frustum.Planes[3] = MakePlaneFromPoints(CameraPos, NBR, NTR, FrustumCenter); // right
	Frustum.Planes[4] = MakePlaneFromPoints(CameraPos, NTR, NTL, FrustumCenter); // top
	Frustum.Planes[5] = MakePlaneFromPoints(CameraPos, NBL, NBR, FrustumCenter); // bottom

	return Frustum;
}

bool FVisibilitySystem::IntersectsAABB(const FFrustum& InFrustum, const FVector& InBoxMin, const FVector& InBoxMax) const
{
	for (const FPlane& Plane : InFrustum.Planes)
	{
		FVector PositiveVertex = InBoxMin;
		if (Plane.Normal.X >= 0) PositiveVertex.X = InBoxMax.X;
		if (Plane.Normal.Y >= 0) PositiveVertex.Y = InBoxMax.Y;
		if (Plane.Normal.Z >= 0) PositiveVertex.Z = InBoxMax.Z;

		if (Plane.GetSignedDistanceToPoint(PositiveVertex) < 0)
		{
			return false; // AABB is completely outside this plane
		}
	}
	return true; // AABB intersects or is inside the frustum
}

bool FVisibilitySystem::HasCameraChanged(const FCamera& InCamera) const
{
	if (!bHasCachedVisibility)
	{
		return true;
	}

	const float Epsilon = 1e-4f;

	const FTransform& CameraTransform = InCamera.GetTransform();
	const FVector CameraPos = CameraTransform.GetLocation();
	const FVector Forward = CameraTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	const FVector Up = CameraTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	const FVector Right = CameraTransform.GetUnitAxis(EAxis::Y).GetSafeNormal();

	return !CameraPos.Equals(CachedCameraPosition, Epsilon)
		|| !Forward.Equals(CachedCameraForward, Epsilon)
		|| !Up.Equals(CachedCameraUp, Epsilon)
		|| !Right.Equals(CachedCameraRight, Epsilon)
		|| AbsFloat(InCamera.GetFOV() - CachedFOV) > Epsilon
		|| AbsFloat(InCamera.GetAspectRatio() - CachedAspect) > Epsilon
		|| AbsFloat(InCamera.GetNearClip() - CachedNearClip) > Epsilon
		|| AbsFloat(InCamera.GetFarClip() - CachedFarClip) > Epsilon;
}

void FVisibilitySystem::UpdateCachedCameraState(const FCamera& InCamera)
{
	const FTransform& CameraTransform = InCamera.GetTransform();
	CachedCameraPosition = CameraTransform.GetLocation();
	CachedCameraForward = CameraTransform.GetUnitAxis(EAxis::X).GetSafeNormal();
	CachedCameraUp = CameraTransform.GetUnitAxis(EAxis::Z).GetSafeNormal();
	CachedCameraRight = CameraTransform.GetUnitAxis(EAxis::Y).GetSafeNormal();
	CachedFOV = InCamera.GetFOV();
	CachedAspect = InCamera.GetAspectRatio();
	CachedNearClip = InCamera.GetNearClip();
	CachedFarClip = InCamera.GetFarClip();
	bHasCachedVisibility = true;
}

bool FVisibilitySystem::TryReuseCachedResults(const FScene& InScene, const FCamera& InCamera, FVisibilityResults& OutResults) const
{
	(void)InScene;

	if (HasCameraChanged(InCamera))
	{
		return false;
	}

	OutResults.VisiblePrimitiveIndices = CachedResults.VisiblePrimitiveIndices;
	return true;
}

void FVisibilitySystem::ComputeVisiblePrimitives(const TArray<FRenderItem>& RenderItems, const FCamera& InCamera, FVisibilityResults& OutResults)
{
	OutResults.VisiblePrimitiveIndices.clear();
	OutResults.VisiblePrimitiveIndices.reserve(RenderItems.size());

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

		OutResults.VisiblePrimitiveIndices.push_back(PrimitiveIndex);
	}
}

void FVisibilitySystem::UpdateCache(const FCamera& InCamera, const FVisibilityResults& InResults)
{
	CachedResults = InResults;
	UpdateCachedCameraState(InCamera);
}

void FVisibilitySystem::LogBuildResult(size_t TotalCount, size_t VisibleCount, bool bReusedCache) const
{
	if (bReusedCache)
	{
		OutputDebugStringA("VisibilitySystem: Reuse cached visible list\n");
	}
	else
	{
		OutputDebugStringA("VisibilitySystem: Camera changed, recomputed visible list\n");
	}

	std::string Msg = "Frustum Culling - Total: "
		+ std::to_string(TotalCount)
		+ ", Visible: "
		+ std::to_string(VisibleCount)
		+ "\n";

	OutputDebugStringA(Msg.c_str());
}

// 평면 생성 헬퍼
namespace
{
	FPlane MakePlaneFromPointNormal(const FVector& Point, const FVector& Normal)
	{
		FPlane Plane;
		Plane.Normal = Normal.GetSafeNormal();
		Plane.Distance = -FVector::DotProduct(Plane.Normal, Point);
		return Plane;
	}

	// 법선 방향이 프러스텀 중심을 향하도록.
	FPlane MakePlaneFromPoints(const FVector& PointA, const FVector& PointB, const FVector& PointC, const FVector& InFrustumCenter)
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
	}
}