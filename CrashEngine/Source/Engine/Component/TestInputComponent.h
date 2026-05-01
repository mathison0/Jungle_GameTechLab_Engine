#pragma once
#include "Component/ActorComponent.h"

class UTestInputComponent : public UActorComponent
{
public:
    DECLARE_CLASS(UTestInputComponent, UActorComponent)
    UTestInputComponent() = default;

    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    void Serialize(FArchive& Ar) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    
private:
    float MoveSpeed = 1.0f;
};
