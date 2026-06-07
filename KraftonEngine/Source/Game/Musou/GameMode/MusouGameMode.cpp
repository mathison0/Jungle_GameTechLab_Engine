#include "Game/Musou/GameMode/MusouGameMode.h"
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

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
	constexpr float KillPopDuration = 0.18f;
	constexpr float KillMilestoneDuration = 1.25f;
	constexpr float KillMilestonePopDuration = 0.24f;
	constexpr float KillMilestoneShakeDuration = 0.48f;
	constexpr float KillMilestoneBaseMarginLeft = -320.0f;
	constexpr float KillMilestoneBaseMarginTop = -60.0f;
	constexpr float BloodVignetteDuration = 1.8f;
	constexpr float BloodVignetteBaseOpacity = 0.90f;
	constexpr float BloodVignetteMinIntensity = 0.55f;
	constexpr float BloodVignetteFullDamage = 30.0f;

	// ── 킬 버스트 연출 — 한 번의 공격 이벤트로 N마리 이상 쓸어담으면 슬로모 + 강셰이크 ──
	// 글로벌 타임 딜레이션(ActionComponent::Slomo)이라 화면 전체가 잠깐 늘어진다.
	// 히트스탑(0.05~0.12s)과 별개 — 둘 다 걸리면 RefreshGlobalTimeDilation 이 합성.
	constexpr int32 KillBurstMinKills   = 5;
	constexpr float KillBurstSlomoDur   = 0.25f;   // 초 (실시간)
	constexpr float KillBurstSlomoRate  = 0.25f;   // 타임스케일 (0..1)
	constexpr float KillBurstShakeScale = 0.4f;

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
	FString MakeScaleTransform(float Scale)
	{
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "scale(%.3f)", Scale);
		return FString(Buffer);
	}

	FString MakePxValue(float Value)
	{
		char Buffer[32] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.1fpx", Value);
		return FString(Buffer);
	}

	FString MakeTextColor(float Alpha, int32 FromRed, int32 FromGreen, int32 FromBlue, int32 ToRed, int32 ToGreen, int32 ToBlue)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const auto LerpChannel = [T](int32 From, int32 To)
		{
			return static_cast<int32>(static_cast<float>(From) + static_cast<float>(To - From) * T + 0.5f);
		};

		const int32 Red = LerpChannel(FromRed, ToRed);
		const int32 Green = LerpChannel(FromGreen, ToGreen);
		const int32 Blue = LerpChannel(FromBlue, ToBlue);

		char Buffer[8] = {};
		std::snprintf(Buffer, sizeof(Buffer), "#%02X%02X%02X", Red, Green, Blue);
		return FString(Buffer);
	}

	FString MakeComboTextColor(float Alpha)
	{
		return MakeTextColor(Alpha, 8, 10, 14, 230, 42, 17);
	}

	FString MakeComboScaleTransform(float Alpha)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const float SmoothT = T * T * (3.0f - 2.0f * T);
		const float Scale = 1.0f + 0.3f * SmoothT;

		return MakeScaleTransform(Scale);
	}

	FString MakeKillScaleTransform(float Alpha)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const float SmoothT = T * T * (3.0f - 2.0f * T);
		const float Scale = 1.0f + 0.12f * SmoothT;

		return MakeScaleTransform(Scale);
	}

	FString MakeKillMilestoneTextColor(float Alpha)
	{
		return MakeTextColor(Alpha, 8, 10, 14, 240, 211, 106);
	}

	FString MakeKillMilestoneScaleTransform(float Alpha)
	{
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const float SmoothT = T * T * (3.0f - 2.0f * T);
		const float Scale = 1.0f + 0.42f * SmoothT;

		return MakeScaleTransform(Scale);
	}

	FString MakeOpacityValue(float Alpha)
	{
		const float ClampedAlpha = std::clamp(Alpha, 0.0f, 1.0f);

		char Buffer[16] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%.3f", ClampedAlpha);
		return FString(Buffer);
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

	if (!HudWidget)
	{
		HudWidget = UUIManager::Get().CreateWidget(GetPlayerController(), "Content/UI/InGameHUD.rml");
		if (HudWidget)
		{
			HudWidget->BindClick("restart-button", []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Play");
				}
			});

			HudWidget->BindClick("stop-button", []()
			{
				if (GEngine)
				{
					GEngine->RequestTransitionToScene("Intro");
				}
			});
		}
	}

	if (HudWidget)
	{
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

	if (InputSystem::Get().GetKeyDown(VK_ESCAPE))
	{
		SetStopMenuVisible(!bStopMenuVisible);
	}

	UpdateHud(DeltaTime);
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
	if (Count >= KillBurstMinKills)
	{
		if (APlayerController* PC = GEngine && GEngine->GetWorld() ? GEngine->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (APawn* PlayerPawn = PC->GetPossessedPawn())
			{
				if (UActionComponent* Action = PlayerPawn->GetComponentByClass<UActionComponent>())
				{
					Action->Slomo(KillBurstSlomoDur, KillBurstSlomoRate);
				}
			}
			if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
			{
				CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(KillBurstShakeScale);
			}
		}
	}
}

void AMusouGameMode::NotifyPlayerDeath(APawn* Player)
{
	UE_LOG("[MusouGameMode] Player died");
	EndMatch();
}

void AMusouGameMode::NotifyPlayerDamaged(APawn* Player, float Damage, AActor* DamageInstigator)
{
	(void)DamageInstigator;

	if (!Player || Damage <= 0.0f)
	{
		return;
	}

	const float DamageIntensity = std::clamp(Damage / BloodVignetteFullDamage, BloodVignetteMinIntensity, 1.0f);
	BloodVignetteIntensity = std::max(BloodVignetteIntensity, DamageIntensity);
	BloodVignetteRemaining = BloodVignetteDuration;
}

AMusouGameState* AMusouGameMode::GetMusouGameState() const
{
	return Cast<AMusouGameState>(GetGameState());
}

void AMusouGameMode::UpdateHud(float DeltaTime)
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
	const int64 Score = MusouState->GetScore();
	const float ComboWindow = MusouState->ComboWindow;
	const float ComboRemaining = MusouState->GetComboRemaining();
	const float DisplayAlpha = (Combo > 0 && ComboWindow > 0.0f)
		? std::clamp(ComboRemaining / ComboWindow, 0.0f, 1.0f)
		: 0.0f;

	float PlayerHealthRatio = 1.0f;
	if (APlayerController* PlayerController = GetPlayerController())
	{
		if (APawn* PlayerPawn = PlayerController->GetPossessedPawn())
		{
			if (UBattleComponent* Battle = PlayerPawn->GetComponentByClass<UBattleComponent>())
			{
				PlayerHealthRatio = std::clamp(Battle->GetHealthRatio(), 0.0f, 1.0f);
			}
		}
	}
	HudWidget->SetAttribute("hp-bar", "value", PlayerHealthRatio);
	HudWidget->SetText("score-counter", FString("score: ") + std::to_string(static_cast<long long>(Score)));

	if (!bKillHudInitialized)
	{
		LastHudKillCount = KillCount;
		bKillHudInitialized = true;
	}
	else if (KillCount > LastHudKillCount)
	{
		const int32 LastMilestone = LastHudKillCount / 10;
		const int32 CurrentMilestone = KillCount / 10;
		LastHudKillCount = KillCount;
		KillPopRemaining = KillPopDuration;

		const int32 ReachedMilestone = CurrentMilestone * 10;
		if (CurrentMilestone > LastMilestone && ReachedMilestone > LastDisplayedKillMilestone)
		{
			ActiveKillMilestone = ReachedMilestone;
			LastDisplayedKillMilestone = ActiveKillMilestone;
			KillMilestoneElapsed = 0.0f;
			KillMilestoneRemaining = KillMilestoneDuration;
		}
	}
	else if (KillCount != LastHudKillCount)
	{
		LastHudKillCount = KillCount;
	}

	const float KillPopAlpha = KillPopDuration > 0.0f
		? std::clamp(KillPopRemaining / KillPopDuration, 0.0f, 1.0f)
		: 0.0f;

	HudWidget->SetText("kill-count-value", std::to_string(KillCount));
	HudWidget->SetProperty("kill-count-value", "transform", MakeKillScaleTransform(KillPopAlpha));
	KillPopRemaining = std::max(0.0f, KillPopRemaining - DeltaTime);

	if (KillMilestoneRemaining > 0.0f && ActiveKillMilestone > 0)
	{
		const float FadeAlpha = std::clamp(KillMilestoneRemaining / KillMilestoneDuration, 0.0f, 1.0f);
		const float PopAlpha = std::clamp(1.0f - (KillMilestoneElapsed / KillMilestonePopDuration), 0.0f, 1.0f);
		const float ShakeAlpha = std::clamp(1.0f - (KillMilestoneElapsed / KillMilestoneShakeDuration), 0.0f, 1.0f) * FadeAlpha;
		const float ShakeX = static_cast<float>(std::sin(KillMilestoneElapsed * 78.0f)) * 6.0f * ShakeAlpha;
		const float ShakeY = static_cast<float>(std::sin(KillMilestoneElapsed * 113.0f + 0.7f)) * 2.5f * ShakeAlpha;

		HudWidget->SetProperty("kill-milestone", "display", "block");
		HudWidget->SetText("kill-milestone", std::to_string(ActiveKillMilestone) + " K.O.");
		HudWidget->SetProperty("kill-milestone", "color", MakeKillMilestoneTextColor(FadeAlpha));
		HudWidget->SetProperty("kill-milestone", "transform", MakeKillMilestoneScaleTransform(PopAlpha));
		HudWidget->SetProperty("kill-milestone", "margin-left", MakePxValue(KillMilestoneBaseMarginLeft + ShakeX));
		HudWidget->SetProperty("kill-milestone", "margin-top", MakePxValue(KillMilestoneBaseMarginTop + ShakeY));

		KillMilestoneElapsed += DeltaTime;
		KillMilestoneRemaining = std::max(0.0f, KillMilestoneRemaining - DeltaTime);
	}
	else
	{
		HudWidget->SetProperty("kill-milestone", "display", "none");
	}

	if (Combo > 0 && DisplayAlpha > 0.03f)
	{
		HudWidget->SetProperty("combo-counter-frame", "display", "block");
		HudWidget->SetText("combo-counter", FString("Combo ") + std::to_string(Combo));
	}
	else
	{
		HudWidget->SetProperty("combo-counter-frame", "display", "none");
		HudWidget->SetText("combo-counter", "");
	}

	HudWidget->SetProperty("combo-counter", "opacity", "1.0");
	HudWidget->SetProperty("combo-counter", "color", MakeComboTextColor(DisplayAlpha));
	HudWidget->SetProperty("combo-counter", "transform", MakeComboScaleTransform(DisplayAlpha));

	const float BloodTimeAlpha = BloodVignetteDuration > 0.0f
		? std::clamp(BloodVignetteRemaining / BloodVignetteDuration, 0.0f, 1.0f)
		: 0.0f;
	const float BloodFadeAlpha = BloodTimeAlpha * BloodTimeAlpha;
	const float BloodAlpha = BloodVignetteBaseOpacity * BloodVignetteIntensity * BloodFadeAlpha;

	if (BloodAlpha > 0.01f)
	{
		HudWidget->SetProperty("blood-vignette", "opacity", MakeOpacityValue(BloodAlpha));
		HudWidget->SetProperty("blood-vignette", "visibility", "visible");
	}
	else
	{
		HudWidget->SetProperty("blood-vignette", "opacity", "0");
		HudWidget->SetProperty("blood-vignette", "visibility", "hidden");
	}

	BloodVignetteRemaining = std::max(0.0f, BloodVignetteRemaining - DeltaTime);
	if (BloodVignetteRemaining <= 0.0f)
	{
		BloodVignetteIntensity = 0.0f;
	}
}

void AMusouGameMode::SetStopMenuVisible(bool bVisible)
{
	bStopMenuVisible = bVisible;

	if (!HudWidget || !HudWidget->IsDocumentLoaded())
	{
		return;
	}

	HudWidget->SetProperty("pause-overlay", "display", bStopMenuVisible ? "flex" : "none");
	HudWidget->SetWantsMouse(bStopMenuVisible);

	if (UWorld* World = GetWorld())
	{
		World->SetPaused(bStopMenuVisible);
	}
}
