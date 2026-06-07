#include "Game/Musou/GameMode/MusouGameState.h"

#include "Game/Musou/Combat/AttackDataRegistry.h"

#include <algorithm>

void AMusouGameState::Tick(float DeltaTime)
{
	AGameStateBase::Tick(DeltaTime);

	if (bMatchEnded)
	{
		return;
	}

	MatchTime += DeltaTime;

	// 콤보 윈도우 — 시간 내 추가 킬이 없으면 콤보 리셋
	if (Combo > 0)
	{
		ComboRemaining -= DeltaTime;
		if (ComboRemaining <= 0.0f)
		{
			ResetCombo();
		}
	}
}

void AMusouGameState::AddKill()
{
	AddKills(1);
}

void AMusouGameState::AddKills(int32 Count)
{
	if (bMatchEnded)
	{
		return;
	}

	if (Count <= 0)
	{
		return;
	}

	KillCount += Count;
	Score += static_cast<int64>(Count) * static_cast<int64>(Combo);

	// 무쌍 게이지 적립 — kills_to_fill 킬이면 가득 (attack_data.lua feedback.ultimate).
	const int32 KillsToFill = (std::max)(FAttackDataRegistry::Get().GetFeedback().UltimateKillsToFill, 1);
	MusouGauge = (std::min)(MusouGauge + static_cast<float>(Count) / static_cast<float>(KillsToFill), 1.0f);
}

bool AMusouGameState::TryConsumeMusouGauge()
{
	if (!IsMusouGaugeFull())
	{
		return false;
	}
	MusouGauge = 0.0f;
	return true;
}

FMusouMatchResult AMusouGameState::MakeMatchResult(bool bVictory) const
{
	FMusouMatchResult Result;
	Result.bVictory = bVictory;
	Result.Score = Score;
	Result.KillCount = KillCount;
	Result.MaxCombo = MaxCombo;
	Result.MatchTime = MatchTime;
	return Result;
}

void AMusouGameState::AddCombo(int32 Count)
{
	if (bMatchEnded || Count <= 0)
	{
		return;
	}

	Combo += Count;
	if (Combo > MaxCombo)
	{
		MaxCombo = Combo;
	}
	ComboRemaining = ComboWindow;
}

void AMusouGameState::ResetCombo()
{
	Combo = 0;
	ComboRemaining = 0.0f;
}
