#pragma once

#include "Engine/Camera/CameraModifier.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Math/Color.h"

class ULetterBoxCameraModifier : public UCameraModifier
{
public:
	DECLARE_CLASS(ULetterBoxCameraModifier, UCameraModifier)

	void StartLetterBox(float InTargetRatio, float Duration);
	void SetLetterBox(float InRatio);
	void ClearLetterBox();

	bool ModifyPostProcess(float DeltaTime, FPostProcessSettings& InOutSettings) override;

	bool IsTransitioning() const { return bTransitioning; }

	float GetCurrentRatio() const { return CurrentRatio; }

private:
	float StartRatio = 0.0f;
	float TargetRatio = 0.0f;
	float CurrentRatio = 0.0f;

	float TransitionTime = 0.0f;
	float TransitionRemainingTime = 0.0f;

	bool bTransitioning = false;
};