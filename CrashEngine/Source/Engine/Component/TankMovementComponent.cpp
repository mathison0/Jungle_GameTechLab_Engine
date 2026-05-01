#include "Component/TankMovementComponent.h"
#include "Input/GameInput.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UTankMovementComponent, UMovementComponent)

void UTankMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    (void)DeltaTime;

    float inputH = GameInput::GetAxis("Horizontal");
    float inputV = GameInput::GetAxis("Vertical");
    
    if (inputH != 0.0f || inputV != 0.0f)
    {
        // 기본 움직임
        const float TrackWidth = 0.8f;  // 탱크의 두 궤도 사이의 거리
        const float RotateSpeedSign = inputV >= 0.0f ? 1.0f : -1.0f;
        const float LeftTrackSpeed = MoveSpeed * inputV + RotateSpeed * inputH * RotateSpeedSign;
        const float RightTrackSpeed = MoveSpeed * inputV - RotateSpeed * inputH * RotateSpeedSign;

        const float LinearSpeed = (LeftTrackSpeed + RightTrackSpeed) * 0.5f;
        const float AngularSpeed = (LeftTrackSpeed - RightTrackSpeed) / TrackWidth;
        
        FVector Forward = Owner->GetActorForward();
        FVector Location = Owner->GetActorLocation();
        Velocity = Forward * LinearSpeed;
        Location += Velocity * DeltaTime;
        Owner->SetActorLocation(Location);
        
        FRotator Rotation = Owner->GetActorRotation();
        Rotation.Yaw += AngularSpeed * DeltaTime;
        Owner->SetActorRotation(Rotation);
    }
}

void UTankMovementComponent::Serialize(FArchive& Ar)
{
    UMovementComponent::Serialize(Ar);
    Ar << MoveSpeed;
    Ar << RotateSpeed;
}

void UTankMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed });
    OutProps.push_back({ "Rotate Speed", EPropertyType::Float, &RotateSpeed });
}
