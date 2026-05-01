#pragma once
#include "MovementComponent.h"
#include "Component/ActorComponent.h"
#include "Math/Vector.h"

class UTankMovementComponent : public UMovementComponent
{
public:
    DECLARE_CLASS(UTestInputComponent, UMovementComponent)
    UTankMovementComponent() = default;

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    
private:
    float MoveSpeed = 1.0f;
    float RotateSpeed = 1.0f;
    FVector Velocity = {0, 0, 0};  // 기존 속도
};
