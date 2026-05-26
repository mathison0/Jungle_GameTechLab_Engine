#include "BoxComponent.h" 
#include "Object/Object.h"

#include <algorithm>
#include <cfloat>
#include <cmath>



bool UBoxComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	const FOBB Box = GetWorldOBB();
	if (!Box.IsValid())
	{
		return false;
	}

	FVector Axes[3];
	Box.GetAxes(Axes[0], Axes[1], Axes[2]);
	const float Extents[3] = { Box.Extents.X, Box.Extents.Y, Box.Extents.Z };
	const FVector RayToCenter = Box.Center - Ray.Origin;

	float TMin = -FLT_MAX;
	float TMax = FLT_MAX;
	FVector EnterNormal = FVector::ZeroVector;
	FVector ExitNormal = FVector::ZeroVector;

	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		const FVector Axis = Axes[AxisIndex].GetSafeNormal();
		const float ProjectedCenter = FVector::DotProduct(Axis, RayToCenter);
		const float ProjectedDirection = FVector::DotProduct(Axis, Ray.Direction);
		const float Extent = Extents[AxisIndex];

		if (std::fabs(ProjectedDirection) <= 1.0e-6f)
		{
			if (-ProjectedCenter - Extent > 0.0f || -ProjectedCenter + Extent < 0.0f)
			{
				return false;
			}
			continue;
		}

		float NearT = (ProjectedCenter - Extent) / ProjectedDirection;
		float FarT = (ProjectedCenter + Extent) / ProjectedDirection;
		FVector NearNormal = ProjectedDirection > 0.0f ? -Axis : Axis;
		FVector FarNormal = -NearNormal;

		if (NearT > FarT)
		{
			std::swap(NearT, FarT);
			std::swap(NearNormal, FarNormal);
		}

		if (NearT > TMin)
		{
			TMin = NearT;
			EnterNormal = NearNormal;
		}
		if (FarT < TMax)
		{
			TMax = FarT;
			ExitNormal = FarNormal;
		}
		if (TMin > TMax)
		{
			return false;
		}
	}

	if (TMax < 0.0f)
	{
		return false;
	}

	const bool bStartsInside = TMin < 0.0f;
	const float HitT = bStartsInside ? TMax : TMin;
	const FVector HitNormal = bStartsInside ? ExitNormal : EnterNormal;

	OutHitResult.bHit = true;
	OutHitResult.HitComponent = this;
	OutHitResult.Distance = HitT;
	OutHitResult.Location = Ray.Origin + Ray.Direction * HitT;
	OutHitResult.Normal = HitNormal.GetSafeNormal();
	OutHitResult.FaceIndex = -1;
	return true;
}

EPrimitiveType UBoxComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Box;
}
