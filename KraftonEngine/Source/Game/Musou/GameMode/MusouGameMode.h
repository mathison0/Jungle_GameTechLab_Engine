#pragma once

#include "GameFramework/GameMode/GameModeBase.h"
#include "Core/Delegate.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/Combat/HitTypes.h"
#include "Game/Musou/UI/MusouHudPresenter.h"

#include "Source/Game/Musou/GameMode/MusouGameMode.generated.h"

class AMusouGameState;
class AActor;
class APawn;
class UUserWidget;

// 공격 발동 브로드캐스트 — 군체 Manager / 보스 BattleComponent가 구독한다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMusouAttackPerformed, const FMusouAttackEvent&);

// Hit Event
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMusouHitConfirmed, const FMusouHitEvent&);

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

	// AnimNotify_MusouAttack이 호출 — 구독자 전체에 공격 이벤트 발행.
	void BroadcastAttack(const FMusouAttackEvent& Event);

	// 수신자가 히트 결과를 단계별로 회신한다. 콤보 증가와 히트스탑은 타이밍 의존성이 달라 분리한다.
	void NotifyAttackComboHits(const FMusouAttackEvent& Event, int32 HitCount);
	void NotifyAttackHitFeedback(const FMusouAttackEvent& Event, int32 HitCount);

	void NotifyHitConfirmed(const FMusouHitEvent& Event);

	// --- 게임 룰 이벤트 진입점 ---
	// 적 처치 시 호출 (적 사망 처리 코드에서). GameState에 킬/콤보 누적.
	virtual void NotifyEnemyKilled(APawn* Killed);

	// 군체 Manager용 — 한 번에 여러 마리 처치 집계.
	virtual void NotifyEnemiesKilled(int32 Count);

	// 플레이어 사망 시 호출 — 매치 종료.
	virtual void NotifyPlayerDeath(APawn* Player);

	// 플레이어에게 실제 데미지가 적용된 순간 호출 — HUD 피격 연출 진입점.
	virtual void NotifyPlayerDamaged(APawn* Player, float Damage, float PlayerCurrentHealth, float PlayerMaxHealth, AActor* DamageInstigator);

	// --- Accessors ---
	AMusouGameState* GetMusouGameState() const;

private:
	void SetStopMenuVisible(bool bVisible);
	float GetPlayerHealthRatio() const;
	void BindHudMenuHoverHandlers();
	void SelectPauseMenuButton(int32 ButtonIndex);
	void SelectDeathMenuButton(int32 ButtonIndex);
	void MovePauseMenuSelection(int32 Delta);
	void MoveDeathMenuSelection(int32 Delta);
	void ExecutePauseMenuSelection();
	void ExecuteDeathMenuSelection();
	void HandlePauseMenuInput();
	void HandleDeathMenuInput();
	void UpdatePauseMenuSelectionVisuals();
	void UpdateDeathMenuSelectionVisuals();
	void ClearHudButtonSelection(const char* const* ButtonIds, int32 ButtonCount);

	UUserWidget* HudWidget = nullptr;
	FMusouHudPresenter HudPresenter;
	bool bStopMenuVisible = false;
	bool bDeathMenuSelectionInitialized = false;
	int32 SelectedPauseButtonIndex = 0;
	int32 SelectedDeathButtonIndex = 0;
};
