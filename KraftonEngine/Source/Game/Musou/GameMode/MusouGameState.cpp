#include "Game/Musou/GameMode/MusouGameState.h"

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
	if (bMatchEnded)
	{
		return;
	}

	++KillCount;
	++Combo;
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
