#pragma once
#include "Engine/Camera/CameraModifier.h"

class UCameraShakeModifier : public UCameraModifier
{
public:
    DECLARE_CLASS(UCameraShakeModifier, UCameraModifier)

	void StartShake(float InAmplitude, float InFrequency, float InDuration);
    void StopShake();

	bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView) override;

    float BezierCP[4] = { 0.25f, 0.1f, 0.75f, 0.9f }; // {CP1.x, CP1.y, CP2.x, CP2.y}

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
