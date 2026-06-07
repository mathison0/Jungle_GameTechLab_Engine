#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/GameMode/MusouPlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Component/Input/ActionComponent.h"
#include "Core/Logging/Log.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "UI/UIManager.h"
#include "UI/UserWidget.h"
#include "Viewport/GameViewportClient.h"

#include <algorithm>

namespace
{
	// 킬 버스트 연출 파라미터(임계/슬로모/셰이크)는 attack_data.lua 의 feedback 테이블 —
	// FAttackDataRegistry::GetFeedback() 으로 조회 (핫리로드 튜닝).
	constexpr const char* PauseMenuButtonIds[] = {
		"resume-button",
		"restart-button",
		"stop-button",
	};

	constexpr const char* DeathMenuButtonIds[] = {
		"death-restart-button",
		"death-stop-button",
	};

	constexpr int32 PauseMenuButtonCount = 3;
	constexpr int32 DeathMenuButtonCount = 2;

	// 첫 로컬 플레이어의 카메라 매니저 — 셰이크 발동 지점. 없으면 null (조용히 스킵).
	APlayerCameraManager* GetLocalCameraManager()
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return nullptr;
		}
		APlayerController* PC = GEngine->GetWorld()->GetFirstPlayerController();
		return PC ? PC->GetPlayerCameraManager() : nullptr;
	}

	void SetGameInputPossessed(bool bPossessed)
	{
		if (!GEngine)
		{
			return;
		}

		if (UGameViewportClient* GameViewportClient = GEngine->GetGameViewportClient())
		{
			GameViewportClient->SetInputPossessed(bPossessed);
		}
	}
}

AMusouGameMode::AMusouGameMode()
{
	// UE 컨벤션 — 생성자에서 게임 전용 클래스 지정.
	// World::BeginPlay → GameMode::BeginPlay에서 GameStateClass가,
	// GameMode::StartMatch에서 PlayerControllerClass가 spawn된다.
	PrimaryActorTick.bTickEvenWhenPaused = true;
	GameStateClass = AMusouGameState::StaticClass();
	PlayerControllerClass = AMusouPlayerController::StaticClass();
}

void AMusouGameMode::StartMatch()
{
	// 베이스가 PlayerController spawn + 첫 Pawn AutoPossess를 수행한다.
	AGameModeBase::StartMatch();
	SetGameInputPossessed(true);

	if (!HudWidget)
	{
		HudWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/InGameHUD.rml");
		if (HudWidget)
		{
			auto RestartMatch = []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Play");
				}
			};

			auto StopMatch = []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Intro");
				}
			};

			HudWidget->BindClick("resume-button", [this]()
			{
				SetStopMenuVisible(false);
			});

			HudWidget->BindClick("restart-button", RestartMatch);
			HudWidget->BindClick("death-restart-button", RestartMatch);
			HudWidget->BindClick("stop-button", StopMatch);
			HudWidget->BindClick("death-stop-button", StopMatch);
			BindHudMenuHoverHandlers();
		}
	}

	if (HudWidget)
	{
		HudPresenter.SetWidget(HudWidget);
		HudWidget->SetWantsMouse(false);
		HudWidget->AddToViewport(0);
		SetStopMenuVisible(false);
		UE_LOG("[MusouGameMode] In-game HUD added to viewport");
	}

	UE_LOG("[MusouGameMode] Match started");
}

void AMusouGameMode::EndMatch()
{
	if (AMusouGameState* MusouState = GetMusouGameState())
	{
		MusouState->SetMatchEnded(true);
		UE_LOG("[MusouGameMode] Match ended — Kills=%d Score=%lld MaxCombo=%d Time=%.1fs",
			MusouState->GetKillCount(),
			static_cast<long long>(MusouState->GetScore()),
			MusouState->GetMaxCombo(),
			MusouState->GetMatchTime());
	}

	AGameModeBase::EndMatch();
}

void AMusouGameMode::EndPlay()
{
	HudPresenter.SetWidget(nullptr);

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

	if (!HudPresenter.IsDeathOverlayVisible() && InputSystem::Get().GetKeyDown(VK_ESCAPE))
	{
		SetStopMenuVisible(!bStopMenuVisible);
	}

	if (bStopMenuVisible && !HudPresenter.IsDeathOverlayVisible())
	{
		HandlePauseMenuInput();
	}

	HudPresenter.Tick(DeltaTime, GetMusouGameState(), GetPlayerHealthRatio());

	if (HudPresenter.AreDeathButtonsVisible())
	{
		if (!bDeathMenuSelectionInitialized)
		{
			bDeathMenuSelectionInitialized = true;
			SelectDeathMenuButton(0);
		}

		HandleDeathMenuInput();
	}
}

void AMusouGameMode::BroadcastAttack(const FMusouAttackEvent& Event)
{
	OnAttackPerformed.Broadcast(Event);
}

void AMusouGameMode::NotifyAttackComboHits(const FMusouAttackEvent& Event, int32 HitCount)
{
	if (HitCount <= 0 || !Event.Attacker)
	{
		return;
	}

	if (Event.bFromPlayer)
	{
		if (AMusouGameState* MusouState = GetMusouGameState())
		{
			MusouState->AddCombo(HitCount);
		}
	}
}

void AMusouGameMode::NotifyAttackHitFeedback(const FMusouAttackEvent& Event, int32 HitCount)
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

	// 히트 카메라 셰이크 — 강도는 스펙별 (attack_data.lua specs.shake, 핫리로드 튜닝).
	// 플레이어 공격만 — 적 공격이 플레이어 카메라를 흔들면 피격 연출과 겹쳐 혼란.
	if (Event.bFromPlayer && Event.Spec.ShakeScale > 0.0f)
	{
		if (APlayerCameraManager* CamMgr = GetLocalCameraManager())
		{
			CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(Event.Spec.ShakeScale);
		}
	}
}

void AMusouGameMode::NotifyHitConfirmed(const FMusouHitEvent& Event)
{
	OnHitConfirmed.Broadcast(Event);
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

	MusouState->AddKills(Count);

	// 킬 버스트 슬로모 — 한 공격 이벤트(스윙 1회 판정)로 대량 처치 시 화면 전체가
	// 잠깐 늘어지는 무쌍식 학살 연출. 피니셔류는 전방위 강판정이라 자연히 자주 발동.
	// 파라미터는 attack_data.lua feedback.kill_burst — 글로벌 타임 딜레이션이라
	// 히트스탑(0.05~0.12s)과 겹치면 RefreshGlobalTimeDilation 이 합성한다.
	const FMusouFeedbackParams& Feedback = FAttackDataRegistry::Get().GetFeedback();
	if (Count >= Feedback.KillBurstMinKills)
	{
		if (APlayerController* PC = GEngine && GEngine->GetWorld() ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (APawn* PlayerPawn = PC->GetPossessedPawn())
			{
				if (UActionComponent* Action = PlayerPawn->GetComponentByClass<UActionComponent>())
				{
					Action->Slomo(Feedback.KillBurstSlomoDur, Feedback.KillBurstSlomoRate);
				}
			}
			if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
			{
				CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(Feedback.KillBurstShakeScale);
			}
		}
	}
}

void AMusouGameMode::NotifyPlayerDeath(APawn* Player)
{
	UE_LOG("[MusouGameMode] Player died");
	EndMatch();
	SetGameInputPossessed(false);
	bStopMenuVisible = false;
	bDeathMenuSelectionInitialized = false;
	HudPresenter.StartDeathOverlay();
	ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(false);
	}
}

void AMusouGameMode::NotifyPlayerDamaged(APawn* Player, float Damage, float PlayerCurrentHealth, float PlayerMaxHealth, AActor* DamageInstigator)
{
	(void)DamageInstigator;

	if (!Player)
	{
		return;
	}

	HudPresenter.NotifyPlayerDamaged(Damage, PlayerCurrentHealth, PlayerMaxHealth);

	// 피격 리액션 — 평시 피격 시 휘청 모션 (공격/구르기/무쌍기 중엔 캐릭터가 스킵 판단).
	if (AMusouCharacter* MusouCharacter = Cast<AMusouCharacter>(Player))
	{
		MusouCharacter->PlayHitReaction();
	}
}

AMusouGameState* AMusouGameMode::GetMusouGameState() const
{
	return Cast<AMusouGameState>(GetGameState());
}

float AMusouGameMode::GetPlayerHealthRatio() const
{
	if (APlayerController* PlayerController = GetPlayerController())
	{
		if (APawn* PlayerPawn = PlayerController->GetPossessedPawn())
		{
			if (UBattleComponent* Battle = PlayerPawn->GetComponentByClass<UBattleComponent>())
			{
				return std::clamp(Battle->GetHealthRatio(), 0.0f, 1.0f);
			}
		}
	}

	return 1.0f;
}

void AMusouGameMode::SetStopMenuVisible(bool bVisible)
{
	if (HudPresenter.IsDeathOverlayVisible())
	{
		return;
	}

	bStopMenuVisible = bVisible;
	HudPresenter.SetPauseMenuVisible(bStopMenuVisible);

	if (bStopMenuVisible)
	{
		SelectPauseMenuButton(0);
	}
	else
	{
		ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);
	}

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(bStopMenuVisible);
	}
}

void AMusouGameMode::BindHudMenuHoverHandlers()
{
	if (!HudWidget)
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < PauseMenuButtonCount; ++ButtonIndex)
	{
		HudWidget->BindMouseOver(PauseMenuButtonIds[ButtonIndex], [this, ButtonIndex]()
		{
			if (bStopMenuVisible && !HudPresenter.IsDeathOverlayVisible())
			{
				SelectPauseMenuButton(ButtonIndex);
			}
		});
	}

	for (int32 ButtonIndex = 0; ButtonIndex < DeathMenuButtonCount; ++ButtonIndex)
	{
		HudWidget->BindMouseOver(DeathMenuButtonIds[ButtonIndex], [this, ButtonIndex]()
		{
			if (HudPresenter.AreDeathButtonsVisible())
			{
				SelectDeathMenuButton(ButtonIndex);
			}
		});
	}
}

void AMusouGameMode::SelectPauseMenuButton(int32 ButtonIndex)
{
	SelectedPauseButtonIndex = (ButtonIndex % PauseMenuButtonCount + PauseMenuButtonCount) % PauseMenuButtonCount;
	UpdatePauseMenuSelectionVisuals();
	ClearHudButtonSelection(DeathMenuButtonIds, DeathMenuButtonCount);
}

void AMusouGameMode::SelectDeathMenuButton(int32 ButtonIndex)
{
	SelectedDeathButtonIndex = (ButtonIndex % DeathMenuButtonCount + DeathMenuButtonCount) % DeathMenuButtonCount;
	UpdateDeathMenuSelectionVisuals();
	ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);
}

void AMusouGameMode::MovePauseMenuSelection(int32 Delta)
{
	SelectPauseMenuButton(SelectedPauseButtonIndex + Delta);
}

void AMusouGameMode::MoveDeathMenuSelection(int32 Delta)
{
	SelectDeathMenuButton(SelectedDeathButtonIndex + Delta);
}

void AMusouGameMode::ExecutePauseMenuSelection()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	HudWidget->Click(PauseMenuButtonIds[SelectedPauseButtonIndex]);
}

void AMusouGameMode::ExecuteDeathMenuSelection()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	HudWidget->Click(DeathMenuButtonIds[SelectedDeathButtonIndex]);
}

void AMusouGameMode::HandlePauseMenuInput()
{
	InputSystem& Input = InputSystem::Get();
	if (Input.GetKeyDown(VK_UP))
	{
		MovePauseMenuSelection(-1);
	}
	if (Input.GetKeyDown(VK_DOWN))
	{
		MovePauseMenuSelection(1);
	}
	if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
	{
		ExecutePauseMenuSelection();
	}
}

void AMusouGameMode::HandleDeathMenuInput()
{
	InputSystem& Input = InputSystem::Get();
	if (Input.GetKeyDown(VK_UP))
	{
		MoveDeathMenuSelection(-1);
	}
	if (Input.GetKeyDown(VK_DOWN))
	{
		MoveDeathMenuSelection(1);
	}
	if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
	{
		ExecuteDeathMenuSelection();
	}
}

void AMusouGameMode::UpdatePauseMenuSelectionVisuals()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < PauseMenuButtonCount; ++ButtonIndex)
	{
		HudWidget->SetClass(PauseMenuButtonIds[ButtonIndex], "selected", ButtonIndex == SelectedPauseButtonIndex);
	}
}

void AMusouGameMode::UpdateDeathMenuSelectionVisuals()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < DeathMenuButtonCount; ++ButtonIndex)
	{
		HudWidget->SetClass(DeathMenuButtonIds[ButtonIndex], "selected", ButtonIndex == SelectedDeathButtonIndex);
	}
}

void AMusouGameMode::ClearHudButtonSelection(const char* const* ButtonIds, int32 ButtonCount)
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < ButtonCount; ++ButtonIndex)
	{
		HudWidget->SetClass(ButtonIds[ButtonIndex], "selected", false);
	}
}
