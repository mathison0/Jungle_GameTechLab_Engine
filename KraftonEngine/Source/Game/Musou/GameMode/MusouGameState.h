#pragma once

#include "GameFramework/GameMode/GameStateBase.h"

#include "Source/Game/Musou/GameMode/MusouGameState.generated.h"

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
	void AddCombo(int32 Count = 1);
	void ResetCombo();

	int32 GetKillCount() const { return KillCount; }
	int32 GetCombo() const { return Combo; }
	int32 GetMaxCombo() const { return MaxCombo; }
	float GetComboRemaining() const { return ComboRemaining; }

	// --- Match ---
	float GetMatchTime() const { return MatchTime; }
	bool IsMatchEnded() const { return bMatchEnded; }
	void SetMatchEnded(bool bEnded) { bMatchEnded = bEnded; }

	// 콤보 유지 시간(초) — 이 시간 내 추가 킬이 없으면 콤보 리셋
	UPROPERTY(Edit, Save, Category="Musou", DisplayName="Combo Window")
	float ComboWindow = 3.0f;

private:
	int32 KillCount = 0;
	int32 Combo = 0;
	int32 MaxCombo = 0;
	float ComboRemaining = 0.0f;  // 콤보 윈도우 잔여 시간
	float MatchTime = 0.0f;       // 매치 경과 시간(초)
	bool bMatchEnded = false;
};
