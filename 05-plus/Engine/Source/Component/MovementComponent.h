#pragma once

#include "ActorComponent.h"

class ENGINE_API UMovementComponent : public UActorComponent
{
public:
	DECLARE_RTTI(UMovementComponent, UActorComponent)

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void Serialize(FArchive& Ar) override;
	void DuplicateSubObjects() override;

	void SetEnabled(bool bInEnabled);
	bool IsEnabled() const;

	void SetAmplitude(float InAmplitude);
	float GetAmplitude() const;

	void SetSpeed(float InSpeed);
	float GetSpeed() const;

private:
	void ResetRuntimeState();

	float Amplitude = 1.0f;
	float Speed = 2.0f;
	bool bEnabled = true;

	float InitialRelativeZ = 0.0f;
	float ElapsedTime = 0.0f;
};
