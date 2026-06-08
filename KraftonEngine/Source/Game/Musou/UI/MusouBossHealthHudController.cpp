#include "Game/Musou/UI/MusouBossHealthHudController.h"

#include "Game/Musou/Boss/MusouBossCharacter.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/HitTypes.h"
#include "Game/Musou/UI/MusouHudPresenter.h"
#include "GameFramework/Pawn/Pawn.h"

#include <algorithm>

namespace
{
	constexpr float BossHealthPostDeathVisibleDuration = 1.2f;
}

void FMusouBossHealthHudController::SetPresenter(FMusouHudPresenter* InPresenter)
{
	Presenter = InPresenter;
}

void FMusouBossHealthHudController::Tick(float DeltaTime)
{
	if (bHidePending)
	{
		HideRemaining = std::max(0.0f, HideRemaining - DeltaTime);
		if (Presenter)
		{
			Presenter->ShowBossHealth(GetActiveBossNameOrFallback(), 0.0f);
		}
		if (HideRemaining <= 0.0f)
		{
			Clear();
		}
		return;
	}

	if (!ActiveBossHealthActor)
	{
		return;
	}

	UBattleComponent* Battle = ActiveBossHealthActor->GetBattleComponent();
	if (!Battle)
	{
		Clear();
		return;
	}

	if (Battle->IsDead())
	{
		ActiveBossHealthActor = nullptr;
		bHidePending = true;
		HideRemaining = BossHealthPostDeathVisibleDuration;
		if (Presenter)
		{
			Presenter->ShowBossHealth(GetActiveBossNameOrFallback(), 0.0f);
		}
		return;
	}

	if (Presenter)
	{
		Presenter->ShowBossHealth(
			GetActiveBossNameOrFallback(),
			std::clamp(Battle->GetHealthRatio(), 0.0f, 1.0f));
	}
}

void FMusouBossHealthHudController::NotifyHitConfirmed(const FMusouHitEvent& Event)
{
	if (!Event.Attack || !Event.Attack->bFromPlayer)
	{
		return;
	}

	if (AMusouBossCharacter* Boss = Cast<AMusouBossCharacter>(Event.HitActor))
	{
		ShowFor(Boss, Event.bKilled);
	}
}

void FMusouBossHealthHudController::NotifyEnemyKilled(APawn* Killed)
{
	if (ActiveBossHealthActor && Killed == static_cast<APawn*>(ActiveBossHealthActor))
	{
		ShowFor(ActiveBossHealthActor, true);
	}
}

void FMusouBossHealthHudController::Clear()
{
	ActiveBossHealthActor = nullptr;
	ActiveBossHealthName.clear();
	bHidePending = false;
	HideRemaining = 0.0f;

	if (Presenter)
	{
		Presenter->HideBossHealth();
	}
}

void FMusouBossHealthHudController::ShowFor(AMusouBossCharacter* Boss, bool bKilled)
{
	if (!Boss)
	{
		return;
	}

	UBattleComponent* Battle = Boss->GetBattleComponent();
	if (!Battle)
	{
		return;
	}

	ActiveBossHealthName = MakeBossName(Boss);
	const bool bShouldShowDead = bKilled || Battle->IsDead();
	const float HealthRatio = bShouldShowDead ? 0.0f : std::clamp(Battle->GetHealthRatio(), 0.0f, 1.0f);
	if (Presenter)
	{
		Presenter->ShowBossHealth(ActiveBossHealthName, HealthRatio);
	}

	if (bShouldShowDead)
	{
		ActiveBossHealthActor = nullptr;
		bHidePending = true;
		HideRemaining = BossHealthPostDeathVisibleDuration;
		return;
	}

	ActiveBossHealthActor = Boss;
	bHidePending = false;
	HideRemaining = 0.0f;
}

FString FMusouBossHealthHudController::MakeBossName(const AMusouBossCharacter* Boss) const
{
	if (!Boss)
	{
		return FString("BOSS");
	}

	const FString BossDisplayName = Boss->GetBossDisplayName();
	if (!BossDisplayName.empty())
	{
		return BossDisplayName;
	}

	if (!Boss->BossId.IsValid())
	{
		return FString("BOSS");
	}

	FString BossName = Boss->BossId.ToString();
	std::replace(BossName.begin(), BossName.end(), '_', ' ');
	return BossName.empty() ? FString("BOSS") : BossName;
}

FString FMusouBossHealthHudController::GetActiveBossNameOrFallback() const
{
	return ActiveBossHealthName.empty() ? FString("BOSS") : ActiveBossHealthName;
}
