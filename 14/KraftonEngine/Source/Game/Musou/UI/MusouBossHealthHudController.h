#pragma once

#include "Core/Types/CoreTypes.h"

class AMusouBossCharacter;
class AMainBossCharacter;
class APawn;
class FMusouHudPresenter;
struct FMusouHitEvent;

class FMusouBossHealthHudController
{
public:
	void SetPresenter(FMusouHudPresenter* InPresenter);
	void Tick(float DeltaTime);
	void NotifyHitConfirmed(const FMusouHitEvent& Event);
	void NotifyEnemyKilled(APawn* Killed);
	void Clear();

private:
	void ShowFor(APawn* Boss, bool bKilled);
	FString MakeBossName(const APawn* Boss) const;
	FString GetActiveBossNameOrFallback() const;

	FMusouHudPresenter* Presenter = nullptr;
	APawn* ActiveBossHealthActor = nullptr;
	FString ActiveBossHealthName;
	bool bHidePending = false;
	float HideRemaining = 0.0f;
};
