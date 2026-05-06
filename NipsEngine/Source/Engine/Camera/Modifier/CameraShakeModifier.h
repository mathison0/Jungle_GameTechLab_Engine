#pragma once
#include "Engine/Camera/CameraModifier.h"

class UCameraShakeModifier : public UCameraModifier
{
public:
	DECLARE_CLASS(UCameraShakeModifier, UCameraModifier)

	void StartShake(float InAmplitude, float InFrequency, float InDuration);
	void StopShake();

	bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

	// [0]=CP1x [1]=CP1y [2]=CP2x [3]=CP2y [4]=P0y(시작) [5]=P3y(끝)
	float BezierCP[6] = { 0.25f, 0.1f, 0.75f, 0.9f, 1.0f, 0.0f };

	float GetAmplitude() const { return Amplitude; }
	float GetFrequency() const { return Frequency; }
	float GetDuration() const { return Duration; }
	bool GetIsShaking() const { return bShaking; }

private:
	float Amplitude = 0.0f;
	float Frequency = 0.0f;
	float Duration = 0.0f;
	float Elapsed = 0.0f;

	bool bShaking = false;
};
