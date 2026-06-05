#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/GameMode/MusouPlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "Core/Logging/Log.h"

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

void AMusouGameMode::NotifyEnemyKilled(APawn* Killed)
{
	if (AMusouGameState* MusouState = GetMusouGameState())
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
