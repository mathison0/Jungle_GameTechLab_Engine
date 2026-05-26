#include "SphereComponent.h"
#include "Object/Object.h"

#include <cmath>



void USphereComponent::PostDuplicate(UObject* Original)
{
	UShapeComponent::PostDuplicate(Original);

	USphereComponent* SphereComp = Cast<USphereComponent>(Original);
	SphereRadius = SphereComp->SphereRadius;
}


void USphereComponent::UpdateWorldAABB() const
{
	const FVector Center = GetWorldLocation();

	const float ScaledRadius = GetScaledSphereRadius();
	WorldAABB.Min = Center - FVector(ScaledRadius, ScaledRadius, ScaledRadius);
	WorldAABB.Max = Center + FVector(ScaledRadius, ScaledRadius, ScaledRadius);
}

bool USphereComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	const FVector Center = GetWorldLocation();
	const float Radius = GetScaledSphereRadius();
	if (Radius <= 0.0f)
	{
		return false;
	}

	const FVector OriginToCenter = Ray.Origin - Center;
	const float A = FVector::DotProduct(Ray.Direction, Ray.Direction);
	const float B = 2.0f * FVector::DotProduct(OriginToCenter, Ray.Direction);
	const float C = FVector::DotProduct(OriginToCenter, OriginToCenter) - Radius * Radius;
	const float Discriminant = B * B - 4.0f * A * C;
	if (Discriminant < 0.0f || A <= 1.0e-6f)
	{
		return false;
	}

	const float SqrtDiscriminant = std::sqrt(Discriminant);
	const float InvDenominator = 1.0f / (2.0f * A);
	const float T0 = (-B - SqrtDiscriminant) * InvDenominator;
	const float T1 = (-B + SqrtDiscriminant) * InvDenominator;

	float HitT = T0;
	if (HitT < 0.0f)
	{
		HitT = T1;
	}
	if (HitT < 0.0f)
	{
		return false;
	}

	const FVector HitLocation = Ray.Origin + Ray.Direction * HitT;

	OutHitResult.bHit = true;
	OutHitResult.HitComponent = this;
	OutHitResult.Distance = HitT;
	OutHitResult.Location = HitLocation;
	OutHitResult.Normal = (HitLocation - Center).GetSafeNormal();
	OutHitResult.FaceIndex = -1;
	return true;
}

EPrimitiveType USphereComponent::GetPrimitiveType() const
{
	return EPrimitiveType::EPT_Sphere;
}
