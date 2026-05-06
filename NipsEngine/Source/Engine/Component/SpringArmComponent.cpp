#include "SpringArmComponent.h"
#include "Core/CollisionTypes.h"
#include "ActorComponent.h"
#include "GameFrameWork/World.h"

DEFINE_CLASS(USpringArmComponent, USceneComponent)

void USpringArmComponent::TickComponent(float DeltaTime)
{
    if (bEnableCameraRotationLag)
    {
        FVector RelativeRotationEuler = GetRelativeRotation();
        FQuat TargetRot = FQuat::MakeFromEuler(RelativeRotationEuler);

        if (CurrentArmRotation.IsIdentity())
        {
            CurrentArmRotation = TargetRot;
        }

        CurrentArmRotation = FQuat::Slerp(CurrentArmRotation, TargetRot, MathUtil::Clamp(DeltaTime * CameraRotationLagSpeed, 0.f, 1.f));

        SetRelativeRotation(CurrentArmRotation.Euler());
    }

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

void USpringArmComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    USceneComponent::GetEditableProperties(OutProps);

    // 카메라 기본 설정
    OutProps.push_back({ "Target Arm Length", EPropertyType::Float, &TargetArmLength });
    OutProps.push_back({ "Socket Offset", EPropertyType::Vec3, &SocketOffset });

    // 충돌 설정
    OutProps.push_back({ "Do Collision Test", EPropertyType::Bool, &bDoCollisionTest });
    OutProps.push_back({ "Probe Radius", EPropertyType::Float, &ProbeRadius });

    // 지연(Lag) 설정
    OutProps.push_back({ "Enable Location Lag", EPropertyType::Bool, &bEnableCameraLag });
    OutProps.push_back({ "Location Lag Speed", EPropertyType::Float, &CameraLagSpeed });

    OutProps.push_back({ "Enable Rotation Lag", EPropertyType::Bool, &bEnableCameraRotationLag });
    OutProps.push_back({ "Rotation Lag Speed", EPropertyType::Float, &CameraRotationLagSpeed });
}

void USpringArmComponent::PostEditProperty(const char* PropertyName)
{
    USceneComponent::PostEditProperty(PropertyName);

    TargetArmLength = std::max(0.0f, TargetArmLength);
    CameraLagSpeed = std::max(0.0f, CameraLagSpeed);
    CameraRotationLagSpeed = std::max(0.0f, CameraRotationLagSpeed);
    ProbeRadius = std::max(0.0f, ProbeRadius);

    if (strcmp(PropertyName, "Target Arm Length") == 0 ||
        strcmp(PropertyName, "Enable Location Lag") == 0)
    {
        FVector ParentWorldPos = GetParent() ? GetParent()->GetWorldLocation() : FVector::ZeroVector;
        FVector DesiredPos = ParentWorldPos - (GetForwardVector() * TargetArmLength) + SocketOffset;

        CurrentArmEndpoint = DesiredPos;
    }
}
