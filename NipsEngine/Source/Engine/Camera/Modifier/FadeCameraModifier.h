#pragma once

#include "Engine/Camera/CameraModifier.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Math/Color.h"

class UFadeCameraModifier : public UCameraModifier
{
public:
	DECLARE_CLASS(UFadeCameraModifier, UCameraModifier)

	void StartFade(const FColor& InColor, float FromAlpha, float ToAlpha, float Duration);
	void SetFade(const FColor& InColor, float Alpha);
	void ClearFade();

	bool ModifyPostProcess(float DeltaTime, FPostProcessSettings& InOutSettings) override;

	bool IsFading() const { return bFading; }
	float GetFadeAlpha() const { return CurrentAlpha; }
	const FColor& GetFadeColor() const { return FadeColor; }


private:
	FColor FadeColor = FColor::Black();

	float FromAlpha = 0.0f;
	float ToAlpha = 0.0f;
	float CurrentAlpha = 0.0f;

	float FadeTime = 0.0f;
	float FadeRemainingTime = 0.0f;

	bool bFading = false;
};