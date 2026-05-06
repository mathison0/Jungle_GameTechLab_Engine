#pragma once
#include "SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
public:
    float TargetArmLength = 3000.f;
    FVector SocketOffset = {};
    float CameraLagSpeed = 10.f;
    bool bEnableCameraLag = true;
    bool bDoCollisionTest = true;

	void TickComponent(float DeltaTime) override;

private:
    FVector CurrentArmEndpoint = {};
};
