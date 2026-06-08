#pragma once

#include "Core/Types/CoreTypes.h"

class AMusouBossCharacter;
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
	void ShowFor(AMusouBossCharacter* Boss, bool bKilled);
	FString MakeBossName(const AMusouBossCharacter* Boss) const;
	FString GetActiveBossNameOrFallback() const;

	FMusouHudPresenter* Presenter = nullptr;
	AMusouBossCharacter* ActiveBossHealthActor = nullptr;
	FString ActiveBossHealthName;
	bool bHidePending = false;
	float HideRemaining = 0.0f;
};
