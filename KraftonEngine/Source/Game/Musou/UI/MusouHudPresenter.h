#pragma once

#include "Core/Types/CoreTypes.h"

class AMusouGameState;
class UUserWidget;

class FMusouHudPresenter
{
public:
	void SetWidget(UUserWidget* InWidget);

	void Tick(float DeltaTime, const AMusouGameState* MusouState, float PlayerHealthRatio);
	void SetPauseMenuVisible(bool bVisible);
	void NotifyPlayerDamaged(float Damage, float PlayerCurrentHealth, float PlayerMaxHealth);
	void StartDeathOverlay();

	bool IsDeathOverlayVisible() const { return bDeathOverlayVisible; }
	bool AreDeathButtonsVisible() const { return bDeathButtonsVisible; }

private:
	void UpdateStatusHud(const AMusouGameState* MusouState, float PlayerHealthRatio);
	void UpdateKillHud(float DeltaTime, const AMusouGameState* MusouState);
	void UpdateComboHud(const AMusouGameState* MusouState);
	void UpdateBloodVignette(float DeltaTime);
	void UpdateDeathOverlay(float DeltaTime);

	UUserWidget* Widget = nullptr;

	bool bKillHudInitialized = false;
	bool bDeathOverlayVisible = false;
	bool bDeathButtonsVisible = false;

	int32 LastHudKillCount = 0;
	int32 LastDisplayedKillMilestone = 0;
	int32 ActiveKillMilestone = 0;

	float KillPopRemaining = 0.0f;
	float KillMilestoneRemaining = 0.0f;
	float KillMilestoneElapsed = 0.0f;
	float BloodVignetteRemaining = 0.0f;
	float BloodVignetteIntensity = 0.0f;
	float DeathOverlayElapsed = 0.0f;
};
