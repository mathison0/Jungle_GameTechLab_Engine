#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/GameMode/MusouPlayerController.h"
#include "Game/Musou/Score/MusouScoreboard.h"
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
			HudWidget->BindClick("scoreboard-save-button", [this]()
			{
				SubmitVictoryScore();
			});
			HudWidget->BindClick("scoreboard-prev-button", [this]()
			{
				HudPresenter.ShowPreviousScoreboardPage();
			});
			HudWidget->BindClick("scoreboard-next-button", [this]()
			{
				HudPresenter.ShowNextScoreboardPage();
			});
			ConfigureHudMenuNavigators();
		}
	}

	bHasPendingVictoryResult = false;
	bVictoryScoreSubmitted = false;

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
	PauseMenuNavigator.SetWidget(nullptr);
	DeathMenuNavigator.SetWidget(nullptr);
	VictoryMenuNavigator.SetWidget(nullptr);

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
		DeathMenuNavigator.EnsureSelection();
		HandleDeathMenuInput();
	}

	if (HudPresenter.AreVictoryButtonsVisible())
	{
		// 결과 문구가 모두 페이드인된 뒤 버튼이 나타나는 첫 프레임에 기본 선택을 맞춘다.
		VictoryMenuNavigator.EnsureSelection();
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
	bHasPendingVictoryResult = false;
	bVictoryScoreSubmitted = false;
	HudPresenter.StartDeathOverlay();
	PauseMenuNavigator.ClearSelection();
	DeathMenuNavigator.ClearSelection();
	VictoryMenuNavigator.ClearSelection();

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
	Result.PlayerHealthRatio = GetPlayerHealthRatio(); // EndMatch가 UnPossess하기 전에 현재 HP를 고정한다.
	PendingVictoryResult = Result;
	bHasPendingVictoryResult = true;
	bVictoryScoreSubmitted = false;

	UE_LOG("[MusouGameMode] Victory resolved — Kills=%d Score=%lld MaxCombo=%d Time=%.1fs",
		Result.KillCount,
		static_cast<long long>(Result.Score),
		Result.MaxCombo,
		Result.MatchTime);

	EndMatch();
	SetGameInputPossessed(false);
	bStopMenuVisible = false;

	// 스코어보드 저장, outro 시작 같은 후속 시스템은 여기에서 Result를 구독하면 된다.
	OnVictoryResolved.Broadcast(Result);

	// 현재 단계에서는 승리 오버레이를 즉시 띄운다. 나중에 outro가 생기면
	// outro 시작/종료 타이밍에 맞춰 이 호출 위치만 조정하면 된다.
	HudPresenter.StartVictoryOverlay(Result, FMusouScoreboard::LoadEntries());
	PauseMenuNavigator.ClearSelection();
	DeathMenuNavigator.ClearSelection();
	VictoryMenuNavigator.ClearSelection();

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

void AMusouGameMode::SubmitVictoryScore()
{
	if (!bHasPendingVictoryResult || bVictoryScoreSubmitted)
	{
		return;
	}

	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	TArray<FMusouScoreboardEntry> UpdatedEntries;
	if (!FMusouScoreboard::Submit(PendingVictoryResult, HudWidget->GetValue("scoreboard-name-input"), &UpdatedEntries))
	{
		HudPresenter.NotifyVictoryScoreSaveFailed();
		return;
	}

	// 저장 버튼을 누른 바로 이 시점에만 실제 스코어보드 파일과 화면 목록이 갱신된다.
	bVictoryScoreSubmitted = true;
	VictoryMenuNavigator.ClearSelection();
	HudPresenter.NotifyVictoryScoreSubmitted(UpdatedEntries);
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
		PauseMenuNavigator.Select(0);
	}
	else
	{
		PauseMenuNavigator.ClearSelection();
	}

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(bStopMenuVisible);
	}
}

void AMusouGameMode::ConfigureHudMenuNavigators()
{
	if (!HudWidget)
	{
		return;
	}

	PauseMenuNavigator.SetWidget(HudWidget);
	PauseMenuNavigator.SetButtons(PauseMenuButtonIds, PauseMenuButtonCount);
	PauseMenuNavigator.BindHoverHandlers([this]()
	{
		return bStopMenuVisible && !HudPresenter.IsResultOverlayVisible();
	});

	DeathMenuNavigator.SetWidget(HudWidget);
	DeathMenuNavigator.SetButtons(DeathMenuButtonIds, DeathMenuButtonCount);
	DeathMenuNavigator.BindHoverHandlers([this]()
	{
		return HudPresenter.AreDeathButtonsVisible();
	});

	VictoryMenuNavigator.SetWidget(HudWidget);
	VictoryMenuNavigator.SetButtons(VictoryMenuButtonIds, VictoryMenuButtonCount);
	VictoryMenuNavigator.BindHoverHandlers([this]()
	{
		return HudPresenter.AreVictoryButtonsVisible();
	});
}

void AMusouGameMode::HandlePauseMenuInput()
{
	PauseMenuNavigator.HandleVerticalInput(InputSystem::Get());
}

void AMusouGameMode::HandleDeathMenuInput()
{
	DeathMenuNavigator.HandleVerticalInput(InputSystem::Get());
}

void AMusouGameMode::HandleVictoryMenuInput()
{
	// 이름 입력 중에는 Enter/Space/방향키를 RML input이 우선 처리하도록 둔다.
	VictoryMenuNavigator.HandleVerticalInput(InputSystem::Get(), true);
}
