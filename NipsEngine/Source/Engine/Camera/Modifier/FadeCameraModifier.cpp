#include "Engine/Camera/Modifier/FadeCameraModifier.h"
#include "Engine/Math/Utils.h"
#include <algorithm>

DEFINE_CLASS(UFadeCameraModifier, UCameraModifier)

void UFadeCameraModifier::StartFade(const FColor& InColor, float InFromAlpha, float InToAlpha, float Duration)
{
	FadeColor = InColor;
	FromAlpha = MathUtil::Clamp(InFromAlpha, 0.0f, 1.0f);
	ToAlpha = MathUtil::Clamp(InToAlpha, 0.0f, 1.0f);
	FadeTime = std::max(Duration, 0.0f);
	FadeRemainingTime = FadeTime;
	CurrentAlpha = FromAlpha;

	if (FadeTime <= 0.0f)
	{
		CurrentAlpha = ToAlpha;
		bFading = false;
		return;
	}

	bFading = true;
	EnableModifier();
}

void UFadeCameraModifier::SetFade(const FColor& InColor, float Alpha)
{
	FadeColor = InColor;
	CurrentAlpha = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
	FromAlpha = CurrentAlpha;
	ToAlpha = CurrentAlpha;
	FadeTime = 0.0f;
	FadeRemainingTime = 0.0f;
	bFading = false;
	EnableModifier();
}

void UFadeCameraModifier::ClearFade()
{
	FromAlpha = 0.0f;
	ToAlpha = 0.0f;
	CurrentAlpha = 0.0f;
	FadeTime = 0.0f;
	FadeRemainingTime = 0.0f;
	bFading = false;
}

// Tick마다 호출되며, FPostProcessSettings의 값을 덮어씌우고 성공 여부를 반환합니다.
bool UFadeCameraModifier::ModifyPostProcess(float DeltaTime, FPostProcessSettings& InOutSettings)
{
	if (bFading)
	{
		FadeRemainingTime = std::max(FadeRemainingTime - DeltaTime, 0.0f);
		const float ElapsedTime = FadeTime - FadeRemainingTime;
		const float FadeAlpha = FadeTime > 0.0f ? MathUtil::Clamp(ElapsedTime / FadeTime, 0.0f, 1.0f) : 1.0f;
		CurrentAlpha = MathUtil::Lerp(FromAlpha, ToAlpha, FadeAlpha);

		if (FadeRemainingTime <= 0.0f)
		{
			CurrentAlpha = ToAlpha;
			bFading = false;
		}
	}

	InOutSettings.FadeColor = FadeColor;
	InOutSettings.FadeAlpha = MathUtil::Clamp(CurrentAlpha, 0.0f, 1.0f);
	return InOutSettings.FadeAlpha > 0.0f || bFading;
}
