#include "Game/Musou/GameMode/MusouGameMode.h"
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

	// 승리 결과 메뉴는 사망 메뉴와 같은 입력/hover 흐름을 쓰되, id만 분리해 둔다.
	constexpr const char* VictoryMenuButtonIds[] = {
		"victory-restart-button",
		"victory-stop-button",
	};

	constexpr int32 PauseMenuButtonCount = 3;
	constexpr int32 DeathMenuButtonCount = 2;
	constexpr int32 VictoryMenuButtonCount = 2;

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
			HudWidget->BindClick("victory-restart-button", RestartMatch);
			HudWidget->BindClick("stop-button", StopMatch);
			HudWidget->BindClick("death-stop-button", StopMatch);
			HudWidget->BindClick("victory-stop-button", StopMatch);
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

	// TODO: 실제 승리 조건이 들어오면 제거할 테스트 진입점.
	// 현재는 플레이 중 T 키로 승리 확정 흐름과 결과 오버레이를 검증한다.
	if (!HudPresenter.IsResultOverlayVisible() && !bStopMenuVisible && InputSystem::Get().GetKeyDown('T'))
	{
		NotifyVictory();
	}

	if (!HudPresenter.IsResultOverlayVisible() && InputSystem::Get().GetKeyDown(VK_ESCAPE))
	{
		SetStopMenuVisible(!bStopMenuVisible);
	}

	if (bStopMenuVisible && !HudPresenter.IsResultOverlayVisible())
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

	if (HudPresenter.AreVictoryButtonsVisible())
	{
		// 결과 문구가 모두 페이드인된 뒤 버튼이 나타나는 첫 프레임에 기본 선택을 맞춘다.
		if (!bVictoryMenuSelectionInitialized)
		{
			bVictoryMenuSelectionInitialized = true;
			SelectVictoryMenuButton(0);
		}

		HandleVictoryMenuInput();
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
	AMusouGameState* MusouState = GetMusouGameState();
	if (MusouState)
	{
		if (MusouState->IsMatchEnded())
		{
			return;
		}
	}

	UE_LOG("[MusouGameMode] Player died");
	EndMatch();
	SetGameInputPossessed(false);
	bStopMenuVisible = false;
	bDeathMenuSelectionInitialized = false;
	bVictoryMenuSelectionInitialized = false;
	HudPresenter.StartDeathOverlay();
	ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);
	ClearHudButtonSelection(VictoryMenuButtonIds, VictoryMenuButtonCount);

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(false);
	}
}

void AMusouGameMode::NotifyVictory()
{
	AMusouGameState* MusouState = GetMusouGameState();
	if (!MusouState || MusouState->IsMatchEnded())
	{
		return;
	}

	// 점수는 승리 확정 순간에 먼저 고정한다.
	// EndMatch 이후 outro/UI tick이 이어져도 저장될 결과는 이 Result 하나만 사용한다.
	FMusouMatchResult Result = MusouState->MakeMatchResult(true);

	UE_LOG("[MusouGameMode] Victory resolved — Kills=%d Score=%lld MaxCombo=%d Time=%.1fs",
		Result.KillCount,
		static_cast<long long>(Result.Score),
		Result.MaxCombo,
		Result.MatchTime);

	EndMatch();
	SetGameInputPossessed(false);
	bStopMenuVisible = false;
	bDeathMenuSelectionInitialized = false;
	bVictoryMenuSelectionInitialized = false;

	// 스코어보드 저장, outro 시작 같은 후속 시스템은 여기에서 Result를 구독하면 된다.
	OnVictoryResolved.Broadcast(Result);

	// 현재 단계에서는 승리 오버레이를 즉시 띄운다. 나중에 outro가 생기면
	// outro 시작/종료 타이밍에 맞춰 이 호출 위치만 조정하면 된다.
	HudPresenter.StartVictoryOverlay(Result);
	ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);
	ClearHudButtonSelection(DeathMenuButtonIds, DeathMenuButtonCount);

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
	if (HudPresenter.IsResultOverlayVisible())
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
			if (bStopMenuVisible && !HudPresenter.IsResultOverlayVisible())
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

	for (int32 ButtonIndex = 0; ButtonIndex < VictoryMenuButtonCount; ++ButtonIndex)
	{
		HudWidget->BindMouseOver(VictoryMenuButtonIds[ButtonIndex], [this, ButtonIndex]()
		{
			if (HudPresenter.AreVictoryButtonsVisible())
			{
				SelectVictoryMenuButton(ButtonIndex);
			}
		});
	}
}

void AMusouGameMode::SelectPauseMenuButton(int32 ButtonIndex)
{
	SelectedPauseButtonIndex = (ButtonIndex % PauseMenuButtonCount + PauseMenuButtonCount) % PauseMenuButtonCount;
	UpdatePauseMenuSelectionVisuals();
	ClearHudButtonSelection(DeathMenuButtonIds, DeathMenuButtonCount);
	ClearHudButtonSelection(VictoryMenuButtonIds, VictoryMenuButtonCount);
}

void AMusouGameMode::SelectDeathMenuButton(int32 ButtonIndex)
{
	SelectedDeathButtonIndex = (ButtonIndex % DeathMenuButtonCount + DeathMenuButtonCount) % DeathMenuButtonCount;
	UpdateDeathMenuSelectionVisuals();
	ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);
	ClearHudButtonSelection(VictoryMenuButtonIds, VictoryMenuButtonCount);
}

void AMusouGameMode::SelectVictoryMenuButton(int32 ButtonIndex)
{
	SelectedVictoryButtonIndex = (ButtonIndex % VictoryMenuButtonCount + VictoryMenuButtonCount) % VictoryMenuButtonCount;
	UpdateVictoryMenuSelectionVisuals();
	ClearHudButtonSelection(PauseMenuButtonIds, PauseMenuButtonCount);
	ClearHudButtonSelection(DeathMenuButtonIds, DeathMenuButtonCount);
}

void AMusouGameMode::MovePauseMenuSelection(int32 Delta)
{
	SelectPauseMenuButton(SelectedPauseButtonIndex + Delta);
}

void AMusouGameMode::MoveDeathMenuSelection(int32 Delta)
{
	SelectDeathMenuButton(SelectedDeathButtonIndex + Delta);
}

void AMusouGameMode::MoveVictoryMenuSelection(int32 Delta)
{
	SelectVictoryMenuButton(SelectedVictoryButtonIndex + Delta);
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

void AMusouGameMode::ExecuteVictoryMenuSelection()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	HudWidget->Click(VictoryMenuButtonIds[SelectedVictoryButtonIndex]);
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

void AMusouGameMode::HandleVictoryMenuInput()
{
	InputSystem& Input = InputSystem::Get();
	// 사망 결과 메뉴와 동일한 조작 체계: 위/아래 이동, Enter/Space 실행.
	if (Input.GetKeyDown(VK_UP))
	{
		MoveVictoryMenuSelection(-1);
	}
	if (Input.GetKeyDown(VK_DOWN))
	{
		MoveVictoryMenuSelection(1);
	}
	if (Input.GetKeyDown(VK_RETURN) || Input.GetKeyDown(VK_SPACE))
	{
		ExecuteVictoryMenuSelection();
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

void AMusouGameMode::UpdateVictoryMenuSelectionVisuals()
{
	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	for (int32 ButtonIndex = 0; ButtonIndex < VictoryMenuButtonCount; ++ButtonIndex)
	{
		HudWidget->SetClass(VictoryMenuButtonIds[ButtonIndex], "selected", ButtonIndex == SelectedVictoryButtonIndex);
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
