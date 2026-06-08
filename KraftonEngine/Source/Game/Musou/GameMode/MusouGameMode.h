#pragma once

#include "GameFramework/GameMode/GameModeBase.h"
#include "Core/Delegate.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/Combat/HitTypes.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/UI/MusouBossHealthHudController.h"
#include "Game/Musou/UI/MusouHudPresenter.h"
#include "Game/Musou/UI/MusouMenuNavigator.h"

#include "Source/Game/Musou/GameMode/MusouGameMode.generated.h"

class AMusouGameState;
class AActor;
class APawn;
struct FMusouMatchResult;
class UUserWidget;

// 공격 발동 브로드캐스트 — 군체 Manager / 보스 BattleComponent가 구독한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMusouAttackPerformed, const FMusouAttackEvent&);

// 히트 확정 이벤트 — 피격 연출/사운드/카메라 반응처럼 결과를 구독하는 쪽에서 사용한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMusouHitConfirmed, const FMusouHitEvent&);

// 승리 결과 이벤트 — 스코어보드 저장, outro 시작, 결과 UI 표시가 이 스냅샷을 공유한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMusouVictoryResolved, const FMusouMatchResult&);

// ============================================================
// AMusouGameMode — 무쌍 게임 룰의 주체 + 전투 이벤트 허브
//
// 생성자에서 MusouGameState/MusouPlayerController를 지정한다.
//
// 전투 파이프라인:
//   AnimNotify_MusouAttack → BroadcastAttack(이벤트 발행)
//     → 수신자(군체 Manager / 보스 BattleComponent)가 각자 판정·적용
//     → 수신자가 콤보 증가 / 히트 피드백 / 킬 집계를 각각 회신
//
// 활성화: Settings/ProjectSettings.ini → Game.GameModeClassName = "AMusouGameMode"
//         (또는 씬별 WorldSettings.GameMode)
// ============================================================
UCLASS()
class AMusouGameMode : public AGameModeBase
{
public:
	GENERATED_BODY()
	AMusouGameMode();
	~AMusouGameMode() override = default;

	// --- Match flow ---
	void StartMatch() override;
	void EndMatch() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;

	// --- 공격 이벤트 허브 ---
	// 군체 Manager 등 수신 시스템은 BeginPlay에서 AddRaw/AddUObject로 구독,
	// EndPlay에서 핸들로 Remove.
	FOnMusouAttackPerformed OnAttackPerformed;

	FOnMusouHitConfirmed OnHitConfirmed;

	// 승리 확정 직후 한 번만 broadcast 된다. 구독자는 전달받은 Result만 신뢰하고
	// GameState의 실시간 값을 다시 읽지 않는 쪽이 안전하다.
	FOnMusouVictoryResolved OnVictoryResolved;

	// AnimNotify_MusouAttack이 호출 — 구독자 전체에 공격 이벤트 발행.
	void BroadcastAttack(const FMusouAttackEvent& Event);

	// 수신자가 히트 결과를 단계별로 회신한다. 콤보 증가와 히트스탑은 타이밍 의존성이 달라 분리한다.
	void NotifyAttackComboHits(const FMusouAttackEvent& Event, int32 HitCount);
	void NotifyAttackHitFeedback(const FMusouAttackEvent& Event, int32 HitCount);

	void NotifyHitConfirmed(const FMusouHitEvent& Event);

	// "맞았을 때만" 슬로모 예약 — AnimNotify_Slomo(bOnlyOnHit)가 호출. Attacker 가
	// Window(실시간 초) 내 실제 히트를 내면 그 순간 Slomo 발동, 아니면 폐기.
	// 같은 프레임에 히트가 먼저 처리되는 순서도 직전 히트 기록으로 커버한다.
	void RequestHitSlomo(APawn* Attacker, float Duration, float TimeDilation, float Window);

	// --- 게임 룰 이벤트 진입점 ---
	// 적 처치 시 호출 (적 사망 처리 코드에서). GameState에 킬/콤보 누적.
	virtual void NotifyEnemyKilled(APawn* Killed);

	// 군체 Manager용 — 한 번에 여러 마리 처치 집계.
	virtual void NotifyEnemiesKilled(int32 Count);

	// 플레이어 사망 시 호출 — 매치 종료.
	virtual void NotifyPlayerDeath(APawn* Player);

	// 승리 조건 달성 시 호출 — 결과 스냅샷 생성 후 매치 종료.
	virtual void NotifyVictory();

	// 최종 보스 조우 시 호출 — 전투 전 story dialog를 띄우고 월드를 일시정지한다.
	virtual void NotifyFinalBossEncounterStarted();
	bool TryStartFinalBossEncounterDialog();
	bool IsFinalBossEncounterDialogActive() const;

	// 보스 카메라 연출 등에서 HUD 전체 표시 여부를 제어한다.
	void SetHudVisible(bool bVisible);

	// 플레이어에게 실제 데미지가 적용된 순간 호출 — HUD 피격 연출 진입점.
	virtual void NotifyPlayerDamaged(APawn* Player, float Damage, float PlayerCurrentHealth, float PlayerMaxHealth, AActor* DamageInstigator);

	// --- Accessors ---
	AMusouGameState* GetMusouGameState() const;

private:
	// 맞았을 때만 슬로모 — 예약/직전히트 큐 처리.
	void FireHitSlomo(APawn* Attacker, float Duration, float TimeDilation) const;
	void TickHitSlomoQueue(float DeltaTime);

	void SetStopMenuVisible(bool bVisible);
	float GetPlayerHealthRatio() const;
	void ConfigureHudMenuNavigators();
	void ShowAudioSettings();
	void HideAudioSettings();
	void AdjustBGMVolume(float Delta);
	void RefreshAudioSettingsUI();
	void HandleAudioSettingsInput();
	void RefreshCameraDirectionUI();   // 설정 UI 의 "카메라 연출" 토글 라벨 갱신
	void ToggleCameraDirection();      // 설정 토글 + 라이브 플레이어에 즉시 반영
	void HandlePauseMenuInput();
	void HandleDeathMenuInput();
	void HandleVictoryMenuInput();
	void SubmitVictoryScore();

	// 씬 전환 보존 — Play→Play2 진입 시 점수/킬/콤보/무쌍게이지/체력 복원(첫 Tick),
	// 종료 시 carry 여부에 따라 저장/폐기.
	void RestorePersistedMatchState();

	UUserWidget* HudWidget = nullptr;
	FMusouHudPresenter HudPresenter;
	FMusouBossHealthHudController BossHealthHud;
	FMusouMenuNavigator PauseMenuNavigator;
	FMusouMenuNavigator DeathMenuNavigator;
	FMusouMenuNavigator VictoryMenuNavigator;

	// 결과 오버레이가 떠 있으면 pause 메뉴 입력을 막고, 활성 메뉴 선택은 각 navigator가 보관한다.
	bool bStopMenuVisible = false;
	bool bAudioSettingsVisible = false;
	// intro 검은 overlay는 즉시 띄우되, SpringArm/카메라 첫 Tick 이후 월드를 pause 한다.
	bool bPendingIntroWorldPause = false;
	bool bHasPendingVictoryResult = false;
	bool bVictoryScoreSubmitted = false;
	FMusouMatchResult PendingVictoryResult;

	// 씬 전환 보존 복원 — 모든 BeginPlay(특히 BattleComponent 의 Health=MaxHealth) 이후에
	// 적용해야 덮어쓰기되므로 첫 Tick 에서 1회 수행한다.
	bool bRestorePending = false;

	// 종료(EndPlay) 시점엔 폰/GameState 가 이미 정리됐을 수 있어 매 Tick 현재 값을 캐시.
	int64 CachedScore = 0;
	int32 CachedKills = 0;
	int32 CachedMaxCombo = 0;
	float CachedGauge = 0.0f;
	float CachedHealth = 100.0f;
	float CachedMaxHealth = 100.0f;

	// 맞았을 때만 슬로모 — 히트 회신을 기다리는 예약, 그리고 같은 프레임에 히트가
	// 먼저 처리되는 순서를 덮기 위한 직전 히트 기록(짧은 수명). 둘 다 Tick 에서 만료.
	struct FPendingHitSlomo
	{
		APawn* Attacker = nullptr;
		float Duration = 0.0f;
		float Dilation = 1.0f;
		float TimeRemaining = 0.0f;
	};
	struct FRecentAttackHit
	{
		APawn* Attacker = nullptr;
		float TimeRemaining = 0.0f;
	};
	TArray<FPendingHitSlomo> PendingHitSlomos;
	TArray<FRecentAttackHit> RecentAttackHits;
};
