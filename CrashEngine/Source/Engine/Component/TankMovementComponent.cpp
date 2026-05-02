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

    float InputH = FGameInput::GetAxis("Horizontal");
    float InputV = FGameInput::GetAxis("Vertical");
    bool bDriftButton = FGameInput::GetKey(VK_SHIFT);
    
    {
        // --- 기존 설정 및 입력 ---
        const float TrackWidth = 0.8f;
        const float RotateSpeedSign = InputV >= 0.0f ? 1.0f : -1.0f;
    
        // 2. 드리프트 계수 설정 (이전 답변의 추천값 반영)
        const float DriftFactor = bDriftButton ? _DriftFactor : 1.0f;      // 마찰력 약화 비율
        const float DriftSmoothness = bDriftButton ? _DriftSmoothness : 0.8f; // 회복 속도 (낮을수록 관성이 큼)

        // 3. 궤도 속도 계산 (드리프트 중에는 조향 민감도를 높여 '로망'을 살림)
        const float CurrentRotateSpeed = bDriftButton ? RotateSpeed * 1.5f : RotateSpeed;
        const float LeftTrackSpeed = MoveSpeed * InputV + CurrentRotateSpeed * InputH * RotateSpeedSign;
        const float RightTrackSpeed = MoveSpeed * InputV - CurrentRotateSpeed * InputH * RotateSpeedSign;

        const float TargetForwardSpeed = (LeftTrackSpeed + RightTrackSpeed) * 0.5f;
        const float TargetAngularSpeed = (LeftTrackSpeed - RightTrackSpeed) / TrackWidth;

        // --- 드리프트 핵심 로직 시작 ---
        FVector Forward = Owner->GetActorForward();
        FVector Right = Forward.Cross({0, 0, 1});
    
        // 현재 차체가 가고자 하는 의도된 방향과 속도
        FVector IntendedVelocity = Forward * TargetForwardSpeed;

        // 4. 횡방향 미끄러짐 계산 (Slide)
        // ActualVelocity를 IntendedVelocity로 바로 바꾸지 않고, 
        // DriftSmoothness에 따라 천천히 보간(Lerp)하여 미끄러짐을 유발합니다.
        Velocity = FMath::Lerp(Velocity, IntendedVelocity, DriftSmoothness * DeltaTime * 10.0f);

        // 5. 위치 및 회전 업데이트
        FVector Location = Owner->GetActorLocation();
        FRotator Rotation = Owner->GetActorRotation();

        Location += Velocity * DeltaTime;
        Owner->SetActorLocation(Location);

        // 회전 시에도 드리프트 중엔 관성을 주어 뒷부분이 더 돌아가는 느낌을 줄 수 있음
        Rotation.Yaw += TargetAngularSpeed * DeltaTime;
        Owner->SetActorRotation(Rotation);
    }
}

void UTankMovementComponent::Serialize(FArchive& Ar)
{
    UMovementComponent::Serialize(Ar);
    Ar << MoveSpeed;
    Ar << RotateSpeed;
    Ar << _BaseFriction;
    Ar << _DriftFactor;
    Ar << _DriftSmoothness;
}

void UTankMovementComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UMovementComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed });
    OutProps.push_back({ "Rotate Speed", EPropertyType::Float, &RotateSpeed });
    OutProps.push_back({ "Base Friction", EPropertyType::Float, &_BaseFriction });
    OutProps.push_back({ "Drift Factor", EPropertyType::Float, &_DriftFactor });
    OutProps.push_back({ "Drift Smoothness", EPropertyType::Float, &_DriftSmoothness });
}
