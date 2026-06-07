#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/GameMode/MusouGameState.h"

// 스코어보드에 실제 저장되는 한 줄의 기록.
// MatchResult 스냅샷을 그대로 복사해 저장 시점 이후의 GameState 변화와 분리한다.
struct FMusouScoreboardEntry
{
	FString PlayerName;
	int64 Score = 0;
	int32 KillCount = 0;
	int32 MaxCombo = 0;
	float MatchTime = 0.0f;
	float PlayerHealthRatio = 1.0f;
	FString SavedAt;
	int64 SavedAtEpochMs = 0;
};

class FMusouScoreboard
{
public:
	static constexpr int32 MaxEntryCount = 12;

	static TArray<FMusouScoreboardEntry> LoadEntries();
	static bool Submit(const FMusouMatchResult& Result, const FString& PlayerName, TArray<FMusouScoreboardEntry>* OutEntries = nullptr);
};
