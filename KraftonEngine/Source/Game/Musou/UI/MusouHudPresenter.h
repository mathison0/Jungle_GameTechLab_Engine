#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/Score/MusouScoreboard.h"

class AMusouGameState;
struct FMusouMatchResult;
class UUserWidget;

class FMusouHudPresenter
{
public:
	void SetWidget(UUserWidget* InWidget);

	void Tick(float DeltaTime, const AMusouGameState* MusouState, float PlayerHealthRatio);
	void SetPauseMenuVisible(bool bVisible);
	void NotifyPlayerDamaged(float Damage, float PlayerCurrentHealth, float PlayerMaxHealth);
	void StartDeathOverlay();

	// 승리 확정 결과를 받아 결과 오버레이를 시작한다.
	// 점수/킬/콤보 표시는 GameState를 다시 읽지 않고 Result 스냅샷만 사용한다.
	void StartVictoryOverlay(const FMusouMatchResult& Result, const TArray<FMusouScoreboardEntry>& ScoreboardEntries);
	void NotifyVictoryScoreSubmitted(const TArray<FMusouScoreboardEntry>& ScoreboardEntries);
	void NotifyVictoryScoreSaveFailed();
	void ShowPreviousScoreboardPage();
	void ShowNextScoreboardPage();

	bool IsDeathOverlayVisible() const { return bDeathOverlayVisible; }
	bool IsVictoryOverlayVisible() const { return bVictoryOverlayVisible; }

	// 사망/승리 결과 오버레이가 떠 있는 동안은 pause 메뉴와 게임 입력을 열지 않는다.
	bool IsResultOverlayVisible() const { return bDeathOverlayVisible || bVictoryOverlayVisible; }
	bool AreDeathButtonsVisible() const { return bDeathButtonsVisible; }
	bool AreVictoryButtonsVisible() const { return bVictoryButtonsVisible; }

private:
	void UpdateStatusHud(const AMusouGameState* MusouState, float PlayerHealthRatio);
	void UpdateKillHud(float DeltaTime, const AMusouGameState* MusouState);
	void UpdateComboHud(const AMusouGameState* MusouState);
	void UpdateBloodVignette(float DeltaTime);
	void UpdateDeathOverlay(float DeltaTime);
	void UpdateVictoryOverlay(float DeltaTime);
	void SetScoreboardEntries(const TArray<FMusouScoreboardEntry>& Entries, bool bResetPage);
	void RenderScoreboardPage();
	void UpdateScoreboardPager();

	UUserWidget* Widget = nullptr;

	bool bKillHudInitialized = false;
	bool bDeathOverlayVisible = false;
	bool bDeathButtonsVisible = false;

	// 승리 버튼은 결과 문구 페이드인이 끝난 뒤에만 마우스/키보드 입력을 받는다.
	bool bVictoryOverlayVisible = false;
	bool bVictoryButtonsVisible = false;
	bool bVictoryScoreboardVisible = false;
	bool bVictoryScoreSubmitted = false;
	TArray<FMusouScoreboardEntry> ScoreboardEntries;
	int32 ScoreboardPageIndex = 0;

	int32 LastHudKillCount = 0;
	int32 LastDisplayedKillMilestone = 0;
	int32 ActiveKillMilestone = 0;

	float KillPopRemaining = 0.0f;
	float KillMilestoneRemaining = 0.0f;
	float KillMilestoneElapsed = 0.0f;
	float BloodVignetteRemaining = 0.0f;
	float BloodVignetteIntensity = 0.0f;
	float DeathOverlayElapsed = 0.0f;
	float VictoryOverlayElapsed = 0.0f;
	float VictoryHealthRatio = 1.0f;
};
