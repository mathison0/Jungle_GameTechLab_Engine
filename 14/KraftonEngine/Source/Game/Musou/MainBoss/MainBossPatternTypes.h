#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/FName.h"

enum class EMainBossPatternState : uint8
{
	Decide = 0,
	Chase,
	Execute,
	Recovery,
	Battlecry,
	Dead
};

struct FMainBossPatternStep
{
	FString SequencePath;

	float MinRange = 0.0f;
	float MaxRange = 5.0f;
	float ChaseGiveUpRange = 0.0f;
	float ChaseGiveUpTime = 3.0f;
	float PlayRate = 1.0f;
	bool bAimUntilAttackNotify = false;
	FName AimStopAttackId = FName::None;
	float AimTurnSpeedDegPerSec = 360.0f;
};

struct FMainBossPattern
{
	FName Id = FName::None;
	int32 Phase = 1;
	int32 Weight = 1;
	float Cooldown = 2.0f;
	float RecoveryTime = 0.8f;
	TArray<FMainBossPatternStep> Steps;
};
