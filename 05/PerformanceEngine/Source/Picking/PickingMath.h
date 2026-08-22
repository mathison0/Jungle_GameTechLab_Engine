#pragma once

#include <cmath>
#include <limits>

#include "Camera/Camera.h"
#include "Math/MathUtility.h"
#include "Scene/SceneTypes.h"

namespace PickingMath
{
	constexpr float NoHitDistance = 1.0e30f;
	constexpr float ParallelTolerance = 1.0e-6f;

	inline FRay BuildPickRay(const FCamera& InCamera, int32 InMouseX, int32 InMouseY, int32 InViewportWidth, int32 InViewportHeight)
	{
		FRay Result = {};
		Result.Origin = InCamera.GetLocation();
		Result.Direction = InCamera.GetRotation().GetForwardVector();

		if (InViewportWidth <= 0 || InViewportHeight <= 0)
		{
			return Result;
		}

		const float PixelX = (static_cast<float>(InMouseX) + 0.5f) / static_cast<float>(InViewportWidth);
		const float PixelY = (static_cast<float>(InMouseY) + 0.5f) / static_cast<float>(InViewportHeight);
		const float NdcX = PixelX * 2.0f - 1.0f;
		const float NdcY = 1.0f - PixelY * 2.0f;

		const FMatrix ViewProjection = InCamera.GetViewMatrix() * InCamera.GetProjectionMatrix();
		const FMatrix InverseViewProjection = ViewProjection.GetInverse();
		const FVector WorldNear = InverseViewProjection.TransformPosition(FVector(NdcX, NdcY, 0.0f));
		const FVector WorldFar = InverseViewProjection.TransformPosition(FVector(NdcX, NdcY, 1.0f));
		const FVector Direction = (WorldFar - WorldNear).GetSafeNormal();
		if (!Direction.IsNearlyZero())
		{
			Result.Direction = Direction;
		}

		return Result;
	}

	inline float IntersectRayAabb(const FVector& InOrigin, const FVector& InInvDirection, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		float TMin = 0.0f;
		float TMax = std::numeric_limits<float>::max();

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const float Origin = InOrigin[AxisIndex];
			const float InverseDirection = InInvDirection[AxisIndex];
			const float BoundsMin = InBoundsMin[AxisIndex];
			const float BoundsMax = InBoundsMax[AxisIndex];

			if (std::fabs(InverseDirection) > 1.e8f)
			{
				if (Origin < BoundsMin || Origin > BoundsMax)
				{
					return NoHitDistance;
				}
				continue;
			}

			float T0 = (BoundsMin - Origin) * InverseDirection;
			float T1 = (BoundsMax - Origin) * InverseDirection;
			if (T0 > T1)
			{
				std::swap(T0, T1);
			}

			TMin = std::max(TMin, T0);
			TMax = std::min(TMax, T1);
			if (TMin > TMax)
			{
				return NoHitDistance;
			}
		}

		return TMin;
	}

	inline bool IntersectRayTriangleFrontFace(
		const FRay& InRay,
		const FVector& InA,
		const FVector& InB,
		const FVector& InC,
		float& OutDistance,
		FVector& OutWorldPosition)
	{
		const FVector EdgeAB = InB - InA;
		const FVector EdgeAC = InC - InA;
		const FVector PVector = FVector::CrossProduct(InRay.Direction, EdgeAC);
		const float Determinant = FVector::DotProduct(EdgeAB, PVector);
		if (Determinant < 1.e-8f)
		{
			return false;
		}

		const float InverseDeterminant = 1.0f / Determinant;
		const FVector TVector = InRay.Origin - InA;
		const float U = FVector::DotProduct(TVector, PVector) * InverseDeterminant;
		if (U < 0.0f || U > 1.0f)
		{
			return false;
		}

		const FVector QVector = FVector::CrossProduct(TVector, EdgeAB);
		const float V = FVector::DotProduct(InRay.Direction, QVector) * InverseDeterminant;
		if (V < 0.0f || U + V > 1.0f)
		{
			return false;
		}

		const float T = FVector::DotProduct(EdgeAC, QVector) * InverseDeterminant;
		if (T <= 0.0f)
		{
			return false;
		}

		OutDistance = T;
		OutWorldPosition = InRay.Origin + InRay.Direction * T;
		return true;
	}

	inline bool IntersectRayTriangleTwoSided(
		const FRay& InRay,
		const FVector& InA,
		const FVector& InB,
		const FVector& InC,
		float& OutDistance)
	{
		const FVector EdgeAB = InB - InA;
		const FVector EdgeAC = InC - InA;
		const FVector PVector = FVector::CrossProduct(InRay.Direction, EdgeAC);
		const float Determinant = FVector::DotProduct(EdgeAB, PVector);
		if (std::fabs(Determinant) <= ParallelTolerance)
		{
			return false;
		}

		const float InverseDeterminant = 1.0f / Determinant;
		const FVector TVector = InRay.Origin - InA;
		const float U = FVector::DotProduct(TVector, PVector) * InverseDeterminant;
		if (U < 0.0f || U > 1.0f)
		{
			return false;
		}

		const FVector QVector = FVector::CrossProduct(TVector, EdgeAB);
		const float V = FVector::DotProduct(InRay.Direction, QVector) * InverseDeterminant;
		if (V < 0.0f || U + V > 1.0f)
		{
			return false;
		}

		const float T = FVector::DotProduct(EdgeAC, QVector) * InverseDeterminant;
		if (T <= ParallelTolerance)
		{
			return false;
		}

		OutDistance = T;
		return true;
	}

	inline bool IntersectPlane(const FRay& InRay, const FVector& InPlaneOrigin, const FVector& InPlaneNormal, FVector& OutIntersection)
	{
		const float Denominator = FVector::DotProduct(InPlaneNormal, InRay.Direction);
		if (std::fabs(Denominator) <= ParallelTolerance)
		{
			return false;
		}

		const float T = FVector::DotProduct(InPlaneOrigin - InRay.Origin, InPlaneNormal) / Denominator;
		if (T <= ParallelTolerance)
		{
			return false;
		}

		OutIntersection = InRay.Origin + InRay.Direction * T;
		return true;
	}
}
