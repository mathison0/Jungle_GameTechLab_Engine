#include "CameraShakeModifier.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Engine/Math/Bezier.h"
#include <cmath>

DEFINE_CLASS(UCameraShakeModifier, UCameraModifier)

void UCameraShakeModifier::StartShake(const FCameraShakeParams& InParams)
{
    Params  = InParams;
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
    if (Elapsed >= Params.Duration)
    {
        bShaking = false;
        return false;
    }

    const float t = Elapsed / Params.Duration;

	const float FadeOutTime = 0.2f;
	float SmoothAlpha = 1.0f;

	if (Params.Duration - Elapsed < FadeOutTime)
	{
        SmoothAlpha = (Params.Duration - Elapsed) / FadeOutTime;
	}

    // Rotation (Pitch/Yaw/Roll 독립 주파수)
    const float RotAlpha = Bezier::BezierValue(t, Params.RotBezierCP) * SmoothAlpha;
    InOutView.Rotation.X += std::sin(Elapsed * Params.RotFrequency[0]) * Params.RotAmplitude[0] * RotAlpha;
    InOutView.Rotation.Y += std::cos(Elapsed * Params.RotFrequency[1]) * Params.RotAmplitude[1] * RotAlpha;
    InOutView.Rotation.Z += std::sin(Elapsed * Params.RotFrequency[2]) * Params.RotAmplitude[2] * RotAlpha;

    // Location (X/Y/Z 독립 주파수)
    const float LocAlpha = Bezier::BezierValue(t, Params.LocBezierCP) * SmoothAlpha;
    InOutView.Location.X += std::sin(Elapsed * Params.LocFrequency[0]) * Params.LocAmplitude[0] * LocAlpha;
    InOutView.Location.Y += std::cos(Elapsed * Params.LocFrequency[1]) * Params.LocAmplitude[1] * LocAlpha;
    InOutView.Location.Z += std::sin(Elapsed * Params.LocFrequency[2]) * Params.LocAmplitude[2] * LocAlpha;

    // FOV
    if (Params.FOVAmplitude != 0.0f)
    {
        const float FovAlpha = Bezier::BezierValue(t, Params.FOVBezierCP) * SmoothAlpha;
        InOutView.FOV += std::sin(Elapsed * Params.FOVFrequency) * Params.FOVAmplitude * FovAlpha;
    }

    return true;
}
