#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/GameMode/MusouPlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "Component/Input/ActionComponent.h"
#include "Core/Logging/Log.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"

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

	if (!HudWidget)
	{
		HudWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/InGameHUD.rml");
	}

	if (HudWidget)
	{
		HudWidget->SetWantsMouse(false);
		HudWidget->AddToViewport(0);
		UE_LOG("[MusouGameMode] In-game HUD added to viewport");
	}

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

void AMusouGameMode::EndPlay()
{
	if (HudWidget)
	{
		HudWidget->RemoveFromParent();
		HudWidget = nullptr;
	}

	AGameModeBase::EndPlay();
}

void AMusouGameMode::Tick(float DeltaTime)
{
	AGameModeBase::Tick(DeltaTime);
	UpdateHud();
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

void AMusouGameMode::UpdateHud()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	const AMusouGameState* MusouState = GetMusouGameState();
	if (!MusouState)
	{
		return;
	}

	const int32 Combo = MusouState->GetCombo();
	const int32 KillCount = MusouState->GetKillCount();
	const float ComboWindow = MusouState->ComboWindow;
	const float ComboRemaining = MusouState->GetComboRemaining();
	const float Alpha = (Combo > 0 && ComboWindow > 0.0f)
		? std::clamp(ComboRemaining / ComboWindow, 0.0f, 1.0f)
		: 0.0f;

	HudWidget->SetText("kill-counter", FString("KILL ") + std::to_string(KillCount));

	if (Combo > 0)
	{
		HudWidget->SetText("combo-counter", FString("Combo ") + std::to_string(Combo));
	}
	else
	{
		HudWidget->SetText("combo-counter", "");
	}

	const FString AlphaText = std::to_string(Alpha);
	HudWidget->SetProperty("combo-counter", "opacity", AlphaText);
	HudWidget->SetProperty("combo-counter", "color", FString("rgba(230, 42, 17, ") + AlphaText + ")");
}
