#include "SpringArmComponent.h"
#include "Core/CollisionTypes.h"
#include "ActorComponent.h"
#include "GameFrameWork/World.h"

void USpringArmComponent::TickComponent(float DeltaTime)
{
    FVector ParentWorldPos = GetParent() ? GetParent()->GetWorldLocation() : FVector::ZeroVector;
	FVector ForwardVector = GetForwardVector();
    FVector DesiredPos = ParentWorldPos - (ForwardVector * TargetArmLength) + SocketOffset;

	UWorld* World = GetOwner() ? GetOwner()->GetFocusedWorld() : nullptr;
    if (!World)
        return;

	if (bDoCollisionTest)
	{
        FHitResult Hit;
        FRay Ray(ParentWorldPos, -ForwardVector);

        if (World->LineTraceSingle(Ray, TargetArmLength, Hit, GetOwner()))
        {
            float ProbeRadius = 12.0f;
            DesiredPos = Hit.Location + (ForwardVector * ProbeRadius);
        }
	}

	if (bEnableCameraLag)
	{
        if (CurrentArmEndpoint.IsZero())
			CurrentArmEndpoint = DesiredPos;

		CurrentArmEndpoint = FVector::Lerp(CurrentArmEndpoint, DesiredPos, MathUtil::Clamp(DeltaTime * CameraLagSpeed, 0.f, 1.f));
	}
	else
	{
        CurrentArmEndpoint = DesiredPos;
	}

	SetWorldLocation(CurrentArmEndpoint);
}
