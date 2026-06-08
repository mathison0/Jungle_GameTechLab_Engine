#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

// ============================================================
// FMusouMatchPersistence — 씬 전환 사이로 매치 상태를 옮기는 캐리어 (싱글톤).
//
// GameMode/GameState/플레이어는 씬 전환 시 파괴/재생성되므로, Play→Play2 처럼
// "이어서 플레이"할 때 점수/킬/최대콤보/무쌍게이지(마나)/체력을 보존한다.
//
// 흐름:
//   - AMusouGameMode::EndPlay 에서 bCarryOnExit 면 현재 상태를 Save (아니면 Clear).
//   - 새 씬의 AMusouGameMode 가 HasState 면 GameState/플레이어에 복원 후 Clear.
//   - 재시작/종료/크레딧 버튼은 SetCarryOnExit(false) → 그 전환은 보존 안 함(새 게임).
//     트리거 볼륨(Play→Play2)은 버튼을 안 거치므로 기본 carry=true 로 보존된다.
// ============================================================
class FMusouMatchPersistence : public TSingleton<FMusouMatchPersistence>
{
	friend class TSingleton<FMusouMatchPersistence>;

public:
	void Save(int64 InScore, int32 InKills, int32 InMaxCombo, float InMusouGauge,
	          float InHealth, float InMaxHealth)
	{
		Score = InScore;
		Kills = InKills;
		MaxCombo = InMaxCombo;
		MusouGauge = InMusouGauge;
		Health = InHealth;
		MaxHealth = InMaxHealth;
		bHasState = true;
	}

	bool HasState() const { return bHasState; }
	void Clear() { bHasState = false; }

	int64 GetScore() const { return Score; }
	int32 GetKills() const { return Kills; }
	int32 GetMaxCombo() const { return MaxCombo; }
	float GetMusouGauge() const { return MusouGauge; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }

	// 이번 EndPlay 가 상태를 보존할지 — 기본 true. 재시작/종료 버튼이 false 로 설정.
	bool ShouldCarryOnExit() const { return bCarryOnExit; }
	void SetCarryOnExit(bool bCarry) { bCarryOnExit = bCarry; }

private:
	FMusouMatchPersistence() = default;
	~FMusouMatchPersistence() = default;

	int64 Score = 0;
	int32 Kills = 0;
	int32 MaxCombo = 0;
	float MusouGauge = 0.0f;
	float Health = 100.0f;
	float MaxHealth = 100.0f;
	bool  bHasState = false;
	bool  bCarryOnExit = true;
};
