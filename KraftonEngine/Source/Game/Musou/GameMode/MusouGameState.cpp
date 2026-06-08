#include "Game/Musou/GameMode/MusouGameState.h"

#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Engine/Profiling/Time/Timer.h"
#include "Engine/Runtime/Engine.h"

#include <algorithm>

namespace
{
	double GetEngineTotalTime()
	{
		const FTimer* Timer = GEngine ? GEngine->GetTimer() : nullptr;
		return Timer ? Timer->GetTotalTime() : 0.0;
	}
}

void AMusouGameState::BeginPlay()
{
	Super::BeginPlay();
	ResetMatchTimerBase();
}

void AMusouGameState::Tick(float DeltaTime)
{
	AGameStateBase::Tick(DeltaTime);

	if (bMatchEnded)
	{
		return;
	}

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
	// 무쌍 게이지는 킬이 아니라 적중 누적으로 적립 — AddMusouGaugeFromHits 참고.
}

void AMusouGameState::AddMusouGaugeFromHits(int32 HitCount)
{
	if (bMatchEnded || HitCount <= 0)
	{
		return;
	}

	// 적중 수만큼 게이지 적립 — hits_to_fill 히트면 가득 (attack_data.lua feedback.ultimate).
	const int32 HitsToFill = (std::max)(FAttackDataRegistry::Get().GetFeedback().UltimateHitsToFill, 1);
	MusouGauge = (std::min)(MusouGauge + static_cast<float>(HitCount) / static_cast<float>(HitsToFill), 1.0f);
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

float AMusouGameState::GetMatchTime() const
{
	return bMatchEnded ? MatchTime : CalculateCurrentMatchTime();
}

void AMusouGameState::SetMatchEnded(bool bEnded)
{
	if (bMatchEnded == bEnded)
	{
		return;
	}

	if (bEnded)
	{
		MatchTime = CalculateCurrentMatchTime();
		bMatchEnded = true;
		return;
	}

	bMatchEnded = false;
	ResetMatchTimerBase();
}

void AMusouGameState::SetMatchTime(float V)
{
	MatchTime = (V < 0.0f) ? 0.0f : V;
	ResetMatchTimerBase();
}

float AMusouGameState::CalculateCurrentMatchTime() const
{
	if (!bMatchTimerBaseInitialized)
	{
		return MatchTime;
	}

	const double ElapsedSinceBase = (std::max)(0.0, GetEngineTotalTime() - MatchTimerBaseTotalTime);
	return MatchTime + static_cast<float>(ElapsedSinceBase);
}

void AMusouGameState::ResetMatchTimerBase()
{
	MatchTimerBaseTotalTime = GetEngineTotalTime();
	bMatchTimerBaseInitialized = true;
}

FMusouMatchResult AMusouGameState::MakeMatchResult(bool bVictory) const
{
	FMusouMatchResult Result;
	Result.bVictory = bVictory;
	Result.Score = Score;
	Result.KillCount = KillCount;
	Result.MaxCombo = MaxCombo;
	Result.MatchTime = GetMatchTime();
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
