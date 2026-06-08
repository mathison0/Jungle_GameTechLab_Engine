#pragma once

#include "GameFramework/GameMode/GameStateBase.h"

#include "Source/Game/Musou/GameMode/MusouGameState.generated.h"

// 승리/패배가 확정된 순간의 결과 스냅샷.
// 이후 outro, 스코어보드 저장, 결과 UI는 이 값을 기준으로 삼아 점수 흔들림을 막는다.
struct FMusouMatchResult
{
	bool bVictory = false;    // true면 승리, false면 패배/중단 계열 결과로 확장 가능
	int64 Score = 0;          // 승리/패배 확정 순간의 최종 점수
	int32 KillCount = 0;      // 최종 처치 수
	int32 MaxCombo = 0;       // 매치 중 달성한 최대 콤보
	float MatchTime = 0.0f;   // 매치 진행 시간(초)
	float PlayerHealthRatio = 1.0f; // 결과 확정 순간의 플레이어 HP 비율(0..1)
};

// ============================================================
// AMusouGameState — 무쌍 게임의 런타임 상태(데이터) 보유
//
// AMusouGameMode가 spawn하며, 게임플레이 코드/UI/Lua가
// 킬 카운트·콤보·경과 시간 등을 여기서 읽는다.
// ============================================================
UCLASS()
class AMusouGameState : public AGameStateBase
{
public:
	GENERATED_BODY()
	AMusouGameState() = default;
	~AMusouGameState() override = default;

	void Tick(float DeltaTime) override;

	// --- Kill / Combo ---
	// 처치 수와 콤보는 분리한다. KillCount는 처치 기준, Combo는 플레이어 타격 성공 기준으로 누적된다.
	void AddKill();
	void AddKills(int32 Count);
	void AddCombo(int32 Count = 1);
	void ResetCombo();

	int32 GetKillCount() const { return KillCount; }
	int32 GetCombo() const { return Combo; }
	int32 GetMaxCombo() const { return MaxCombo; }
	int64 GetScore() const { return Score; }
	float GetComboRemaining() const { return ComboRemaining; }

	// --- 무쌍 게이지 (0..1) ---
	// 킬로 차오르고(AddKills 가 feedback.ultimate.kills_to_fill 기준 적립),
	// 가득 차면 무쌍기(AMusouCharacter) 발동 가능. 발동 시 TryConsumeMusouGauge 로 소모.
	float GetMusouGauge() const { return MusouGauge; }
	bool IsMusouGaugeFull() const { return MusouGauge >= 1.0f; }
	bool TryConsumeMusouGauge();   // 가득 찼을 때만 소모하고 true

	// --- Match ---
	float GetMatchTime() const { return MatchTime; }
	bool IsMatchEnded() const { return bMatchEnded; }
	void SetMatchEnded(bool bEnded) { bMatchEnded = bEnded; }

	// 결과 확정 시점의 값을 한 번에 복사한다. EndMatch 이후 점수/시간 갱신이 멈추더라도
	// UI와 저장소가 같은 값을 보도록 GameMode에서 이 스냅샷을 전달한다.
	FMusouMatchResult MakeMatchResult(bool bVictory) const;

	// --- 씬 전환 보존(FMusouMatchPersistence) 복원용 직접 setter ---
	void SetScore(int64 V)      { Score = V; }
	void SetKillCount(int32 V)  { KillCount = (V < 0) ? 0 : V; }
	void SetMaxCombo(int32 V)   { MaxCombo = (V < 0) ? 0 : V; }
	void SetMusouGauge(float V) { MusouGauge = (V < 0.0f) ? 0.0f : (V > 1.0f ? 1.0f : V); }

	// 콤보 유지 시간(초) — 이 시간 내 추가 킬이 없으면 콤보 리셋
	UPROPERTY(Edit, Save, Category="Musou", DisplayName="Combo Window")
	float ComboWindow = 3.0f;

private:
	int32 KillCount = 0;
	int32 Combo = 0;
	int32 MaxCombo = 0;
	int64 Score = 0;
	float ComboRemaining = 0.0f;  // 콤보 윈도우 잔여 시간
	float MatchTime = 0.0f;       // 매치 경과 시간(초)
	float MusouGauge = 0.0f;      // 무쌍 게이지 (0..1) — 킬 적립, 무쌍기 발동 시 소모
	bool bMatchEnded = false;
};
