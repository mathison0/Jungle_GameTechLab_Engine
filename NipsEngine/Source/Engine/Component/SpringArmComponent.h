#pragma once
#include "SceneComponent.h"

class USpringArmComponent : public USceneComponent
{
public:
    DECLARE_CLASS(USpringArmComponent, USceneComponent)

    float TargetArmLength = 3000.f;
    FVector SocketOffset = {};
    float CameraLagSpeed = 10.f;
    bool bEnableCameraLag = true;
    bool bDoCollisionTest = true;
	float ProbeRadius = 12.f;
    bool bEnableCameraRotationLag = false;
    float CameraRotationLagSpeed = 10.f;

public:
	void TickComponent(float DeltaTime) override;
    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

	FVector GetTargetLocation() const { return CurrentArmEndpoint; }

private:
    FVector CurrentArmEndpoint = {};
    FQuat CurrentArmRotation = {};
};
