#include "Game/Musou/UI/MusouHudPresenter.h"

#include "Game/Musou/GameMode/MusouGameState.h"
#include "UI/UserWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
	constexpr float KillPopDuration = 0.18f;
	constexpr float KillMilestoneDuration = 1.25f;
	constexpr float KillMilestonePopDuration = 0.24f;
	constexpr float KillMilestoneShakeDuration = 0.48f;
	constexpr float KillMilestoneBaseMarginLeft = -320.0f;
	constexpr float KillMilestoneBaseMarginTop = -60.0f;

	constexpr float BloodVignetteDuration = 1.8f;
	constexpr float BloodVignetteBaseOpacity = 0.90f;
	constexpr float BloodVignetteMinIntensity = 0.55f;
	constexpr float BloodVignetteFullDamage = 30.0f;
	constexpr float BloodVignetteMinTriggerHealthRatio = 0.15f;
	constexpr float BloodVignetteLowHealthTriggerRatio = 0.35f;

	constexpr float DeathOverlayFadeDuration = 0.85f;
	constexpr float DeathOverlayMaxAlpha = 0.72f;
	constexpr float DeathTitleFadeDelay = 0.25f;
	constexpr float DeathTitleFadeDuration = 1.15f;

	FString MakeScaleTransform(float Scale)
	{
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "scale(%.3f)", Scale);
		return FString(Buffer);
	}

	FString MakePxValue(float Value)
	{
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.1fpx", Value);
		return FString(Buffer);
	}

	FString MakeTextColor(float Alpha, int32 FromRed, int32 FromGreen, int32 FromBlue, int32 ToRed, int32 ToGreen, int32 ToBlue)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const auto LerpChannel = [T](int32 From, int32 To)
		{
			return static_cast<int32>(static_cast<float>(From) + static_cast<float>(To - From) * T + 0.5f);
		};

		const int32 Red = LerpChannel(FromRed, ToRed);
		const int32 Green = LerpChannel(FromGreen, ToGreen);
		const int32 Blue = LerpChannel(FromBlue, ToBlue);

		char Buffer[8] = {};
		std::snprintf(Buffer, sizeof(Buffer), "#%02X%02X%02X", Red, Green, Blue);
		return FString(Buffer);
	}

	FString MakeComboTextColor(float Alpha)
	{
		return MakeTextColor(Alpha, 8, 10, 14, 230, 42, 17);
	}

	float SmoothStep01(float Value)
	{
		const float T = std::clamp(Value, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	FString MakeComboScaleTransform(float Alpha)
	{
		const float Scale = 1.0f + 0.3f * SmoothStep01(Alpha);
		return MakeScaleTransform(Scale);
	}

	FString MakeKillScaleTransform(float Alpha)
	{
		const float Scale = 1.0f + 0.12f * SmoothStep01(Alpha);
		return MakeScaleTransform(Scale);
	}

	FString MakeKillMilestoneTextColor(float Alpha)
	{
		return MakeTextColor(Alpha, 8, 10, 14, 240, 211, 106);
	}

	FString MakeKillMilestoneScaleTransform(float Alpha)
	{
		const float Scale = 1.0f + 0.42f * SmoothStep01(Alpha);
		return MakeScaleTransform(Scale);
	}

	FString MakeOpacityValue(float Alpha)
	{
		const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);

		char Buffer[16] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.3f", ClampedAlpha);
		return FString(Buffer);
	}

	FString MakeOverlayColor(float Alpha)
	{
		const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);
		const int32 AlphaByte = static_cast<int32>(ClampedAlpha * 255.0f + 0.5f);

		char Buffer[16] = {};
		std::snprintf(Buffer, sizeof(Buffer), "#080A0E%02X", AlphaByte);
		return FString(Buffer);
	}
}

void FMusouHudPresenter::SetWidget(UUserWidget* InWidget)
{
	Widget = InWidget;
}

void FMusouHudPresenter::Tick(float DeltaTime, const AMusouGameState* MusouState, float PlayerHealthRatio)
{
	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	if (MusouState)
	{
		UpdateStatusHud(MusouState, PlayerHealthRatio);
		UpdateKillHud(DeltaTime, MusouState);
		UpdateComboHud(MusouState);
	}

	UpdateBloodVignette(DeltaTime);
	UpdateDeathOverlay(DeltaTime);
}

void FMusouHudPresenter::SetPauseMenuVisible(bool bVisible)
{
	if (bDeathOverlayVisible || !Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	if (bVisible)
	{
		Widget->SetProperty("pause-overlay", "background-color", "#1616165c");
		Widget->SetProperty("death-title", "display", "none");
		Widget->SetProperty("death-title", "opacity", "0");
		Widget->SetProperty("pause-menu", "display", "block");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetProperty("resume-button", "display", "block");
		Widget->SetProperty("restart-button", "display", "block");
		Widget->SetProperty("stop-button", "display", "block");
	}

	Widget->SetProperty("pause-overlay", "display", bVisible ? "block" : "none");
	Widget->SetWantsMouse(bVisible);
}

void FMusouHudPresenter::NotifyPlayerDamaged(float Damage, float PlayerCurrentHealth, float PlayerMaxHealth)
{
	if (Damage <= 0.0f || PlayerMaxHealth <= 0.0f)
	{
		return;
	}

	const bool bLowHealth = PlayerCurrentHealth <= PlayerMaxHealth * BloodVignetteLowHealthTriggerRatio;
	if (!bLowHealth && Damage < PlayerMaxHealth * BloodVignetteMinTriggerHealthRatio)
	{
		return;
	}

	const float DamageIntensity = std::clamp(Damage / BloodVignetteFullDamage, BloodVignetteMinIntensity, 1.0f);
	BloodVignetteIntensity = std::max(BloodVignetteIntensity, DamageIntensity);
	BloodVignetteRemaining = BloodVignetteDuration;
}

void FMusouHudPresenter::StartDeathOverlay()
{
	if (bDeathOverlayVisible)
	{
		return;
	}

	bDeathOverlayVisible = true;
	bDeathButtonsVisible = false;
	DeathOverlayElapsed = 0.0f;

	if (!Widget || !Widget->IsDocumentLoaded())
	{
		return;
	}

	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-overlay", "background-color", MakeOverlayColor(0.0f));
	Widget->SetProperty("death-title", "display", "block");
	Widget->SetProperty("death-title", "opacity", "0");
	Widget->SetProperty("pause-menu", "display", "none");
	Widget->SetProperty("death-menu", "display", "none");
	Widget->SetWantsMouse(false);
}

void FMusouHudPresenter::UpdateStatusHud(const AMusouGameState* MusouState, float PlayerHealthRatio)
{
	Widget->SetAttribute("hp-bar", "value", std::clamp(PlayerHealthRatio, 0.0f, 1.0f));
	Widget->SetText("score-counter", FString("score: ") + std::to_string(static_cast<long long>(MusouState->GetScore())));
	Widget->SetAttribute("musou-bar", "value", MusouState->GetMusouGauge());
	Widget->SetProperty("musou-ready-label", "visibility", MusouState->IsMusouGaugeFull() ? "visible" : "hidden");
}

void FMusouHudPresenter::UpdateKillHud(float DeltaTime, const AMusouGameState* MusouState)
{
	const int32 KillCount = MusouState->GetKillCount();

	if (!bKillHudInitialized)
	{
		LastHudKillCount = KillCount;
		bKillHudInitialized = true;
	}
	else if (KillCount > LastHudKillCount)
	{
		const int32 LastMilestone = LastHudKillCount / 10;
		const int32 CurrentMilestone = KillCount / 10;
		LastHudKillCount = KillCount;
		KillPopRemaining = KillPopDuration;

		const int32 ReachedMilestone = CurrentMilestone * 10;
		if (CurrentMilestone > LastMilestone && ReachedMilestone > LastDisplayedKillMilestone)
		{
			ActiveKillMilestone = ReachedMilestone;
			LastDisplayedKillMilestone = ActiveKillMilestone;
			KillMilestoneElapsed = 0.0f;
			KillMilestoneRemaining = KillMilestoneDuration;
		}
	}
	else if (KillCount != LastHudKillCount)
	{
		LastHudKillCount = KillCount;
	}

	const float KillPopAlpha = KillPopDuration > 0.0f
		? std::clamp(KillPopRemaining / KillPopDuration, 0.0f, 1.0f)
		: 0.0f;

	Widget->SetText("kill-count-value", std::to_string(KillCount));
	Widget->SetProperty("kill-count-value", "transform", MakeKillScaleTransform(KillPopAlpha));
	KillPopRemaining = std::max(0.0f, KillPopRemaining - DeltaTime);

	if (KillMilestoneRemaining > 0.0f && ActiveKillMilestone > 0)
	{
		const float FadeAlpha = std::clamp(KillMilestoneRemaining / KillMilestoneDuration, 0.0f, 1.0f);
		const float PopAlpha = std::clamp(1.0f - (KillMilestoneElapsed / KillMilestonePopDuration), 0.0f, 1.0f);
		const float ShakeAlpha = std::clamp(1.0f - (KillMilestoneElapsed / KillMilestoneShakeDuration), 0.0f, 1.0f) * FadeAlpha;
		const float ShakeX = static_cast<float>(std::sin(KillMilestoneElapsed * 78.0f)) * 6.0f * ShakeAlpha;
		const float ShakeY = static_cast<float>(std::sin(KillMilestoneElapsed * 113.0f + 0.7f)) * 2.5f * ShakeAlpha;

		Widget->SetProperty("kill-milestone", "display", "block");
		Widget->SetText("kill-milestone", std::to_string(ActiveKillMilestone) + " K.O.");
		Widget->SetProperty("kill-milestone", "color", MakeKillMilestoneTextColor(FadeAlpha));
		Widget->SetProperty("kill-milestone", "transform", MakeKillMilestoneScaleTransform(PopAlpha));
		Widget->SetProperty("kill-milestone", "margin-left", MakePxValue(KillMilestoneBaseMarginLeft + ShakeX));
		Widget->SetProperty("kill-milestone", "margin-top", MakePxValue(KillMilestoneBaseMarginTop + ShakeY));

		KillMilestoneElapsed += DeltaTime;
		KillMilestoneRemaining = std::max(0.0f, KillMilestoneRemaining - DeltaTime);
	}
	else
	{
		Widget->SetProperty("kill-milestone", "display", "none");
	}
}

void FMusouHudPresenter::UpdateComboHud(const AMusouGameState* MusouState)
{
	const int32 Combo = MusouState->GetCombo();
	const float ComboWindow = MusouState->ComboWindow;
	const float ComboRemaining = MusouState->GetComboRemaining();
	const float DisplayAlpha = (Combo > 0 && ComboWindow > 0.0f)
		? std::clamp(ComboRemaining / ComboWindow, 0.0f, 1.0f)
		: 0.0f;

	if (Combo > 0 && DisplayAlpha > 0.03f)
	{
		Widget->SetProperty("combo-counter-frame", "display", "block");
		Widget->SetText("combo-counter", FString("Combo ") + std::to_string(Combo));
	}
	else
	{
		Widget->SetProperty("combo-counter-frame", "display", "none");
		Widget->SetText("combo-counter", "");
	}

	Widget->SetProperty("combo-counter", "opacity", "1.0");
	Widget->SetProperty("combo-counter", "color", MakeComboTextColor(DisplayAlpha));
	Widget->SetProperty("combo-counter", "transform", MakeComboScaleTransform(DisplayAlpha));
}

void FMusouHudPresenter::UpdateBloodVignette(float DeltaTime)
{
	const float BloodTimeAlpha = BloodVignetteDuration > 0.0f
		? std::clamp(BloodVignetteRemaining / BloodVignetteDuration, 0.0f, 1.0f)
		: 0.0f;
	const float BloodFadeAlpha = BloodTimeAlpha * BloodTimeAlpha;
	const float BloodAlpha = BloodVignetteBaseOpacity * BloodVignetteIntensity * BloodFadeAlpha;

	if (BloodAlpha > 0.01f)
	{
		Widget->SetProperty("blood-vignette", "opacity", MakeOpacityValue(BloodAlpha));
		Widget->SetProperty("blood-vignette", "visibility", "visible");
	}
	else
	{
		Widget->SetProperty("blood-vignette", "opacity", "0");
		Widget->SetProperty("blood-vignette", "visibility", "hidden");
	}

	BloodVignetteRemaining = std::max(0.0f, BloodVignetteRemaining - DeltaTime);
	if (BloodVignetteRemaining <= 0.0f)
	{
		BloodVignetteIntensity = 0.0f;
	}
}

void FMusouHudPresenter::UpdateDeathOverlay(float DeltaTime)
{
	if (!bDeathOverlayVisible)
	{
		return;
	}

	DeathOverlayElapsed += DeltaTime;

	const float OverlayAlpha = DeathOverlayMaxAlpha * SmoothStep01(DeathOverlayElapsed / DeathOverlayFadeDuration);
	const float TitleAlpha = SmoothStep01((DeathOverlayElapsed - DeathTitleFadeDelay) / DeathTitleFadeDuration);

	Widget->SetProperty("pause-overlay", "display", "block");
	Widget->SetProperty("pause-overlay", "background-color", MakeOverlayColor(OverlayAlpha));
	Widget->SetProperty("death-title", "display", "block");
	Widget->SetProperty("death-title", "opacity", MakeOpacityValue(TitleAlpha));

	if (TitleAlpha < 1.0f)
	{
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "none");
		Widget->SetWantsMouse(false);
		return;
	}

	if (!bDeathButtonsVisible)
	{
		bDeathButtonsVisible = true;
		Widget->SetProperty("pause-menu", "display", "none");
		Widget->SetProperty("death-menu", "display", "block");
		Widget->SetWantsMouse(true);
	}
}
