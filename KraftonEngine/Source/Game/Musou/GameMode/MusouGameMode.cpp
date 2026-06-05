#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/GameMode/MusouPlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "Component/Input/ActionComponent.h"
#include "Core/Logging/Log.h"

#include <algorithm>

AMusouGameMode::AMusouGameMode()
{
	// UE 컨벤션 — 생성자에서 게임 전용 클래스 지정.
	// World::BeginPlay → GameMode::BeginPlay에서 GameStateClass가,
	// GameMode::StartMatch에서 PlayerControllerClass가 spawn된다.
	GameStateClass = AMusouGameState::StaticClass();
	PlayerControllerClass = AMusouPlayerController::StaticClass();
}

void AMusouGameMode::StartMatch()
{
	// 베이스가 PlayerController spawn + 첫 Pawn AutoPossess를 수행한다.
	AGameModeBase::StartMatch();

	UE_LOG("[MusouGameMode] Match started");
}

void AMusouGameMode::EndMatch()
{
	if (AMusouGameState* MusouState = GetMusouGameState())
	{
		MusouState->SetMatchEnded(true);
		UE_LOG("[MusouGameMode] Match ended — Kills=%d MaxCombo=%d Time=%.1fs",
			MusouState->GetKillCount(), MusouState->GetMaxCombo(), MusouState->GetMatchTime());
	}

	AGameModeBase::EndMatch();
}

void AMusouGameMode::BroadcastAttack(const FMusouAttackEvent& Event)
{
	OnAttackPerformed.Broadcast(Event);
}

void AMusouGameMode::NotifyAttackHits(const FMusouAttackEvent& Event, int32 HitCount)
{
	if (HitCount <= 0 || !Event.Attacker)
	{
		return;
	}

	// 히트 수 비례 히트스탑 — 대량 학살 타격감. (수신자별 회신이므로 짧게 유지)
	constexpr float HitStopBase = 0.05f;
	constexpr float HitStopPerHit = 0.005f;
	constexpr float HitStopMax = 0.12f;

	if (UActionComponent* Action = Event.Attacker->GetComponentByClass<UActionComponent>())
	{
		const float Duration = std::min(HitStopBase + HitStopPerHit * static_cast<float>(HitCount), HitStopMax);
		Action->LocalHitStop(Duration);
	}
}

void AMusouGameMode::NotifyEnemyKilled(APawn* Killed)
{
	if (AMusouGameState* MusouState = GetMusouGameState())
	{
		MusouState->AddKill();
	}
}

void AMusouGameMode::NotifyEnemiesKilled(int32 Count)
{
	AMusouGameState* MusouState = GetMusouGameState();
	if (!MusouState)
	{
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		MusouState->AddKill();
	}
}

void AMusouGameMode::NotifyPlayerDeath(APawn* Player)
{
	UE_LOG("[MusouGameMode] Player died");
	EndMatch();
}

AMusouGameState* AMusouGameMode::GetMusouGameState() const
{
	return Cast<AMusouGameState>(GetGameState());
}
