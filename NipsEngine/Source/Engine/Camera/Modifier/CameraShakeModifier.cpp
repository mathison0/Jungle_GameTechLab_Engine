#include "CameraShakeModifier.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Engine/Math/Utils.h"
#include <cmath>

DEFINE_CLASS(UCameraShakeModifier, UCameraModifier)

void UCameraShakeModifier::StartShake(float InAmplitude, float InFrequency, float InDuration)
{
    Amplitude = InAmplitude;
    Frequency = InFrequency;
    Duration = InDuration;
    Elapsed = 0.0f;
    bShaking = true;
    EnableModifier();
}

void UCameraShakeModifier::StopShake()
{
    bShaking = false;
}

bool UCameraShakeModifier::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
    if (!bShaking) return false;

	Elapsed += DeltaTime;
    if (Elapsed >= Duration)
    {
        bShaking = false;
        return false;
	}

	const float t = Elapsed / Duration;



    return false;
}
