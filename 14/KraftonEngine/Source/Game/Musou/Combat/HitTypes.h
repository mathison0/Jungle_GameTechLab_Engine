#pragma once

#include "Game/Crowd/CrowdUnitTypes.h"
#include "Game/Musou/Combat/AttackTypes.h"

class AActor;

struct FMusouHitEvent
{
	const FMusouAttackEvent* Attack = nullptr;

	AActor* HitActor = nullptr;
	FUnitHandle UnitHandle;

	FVector HitLocation = FVector::ZeroVector;
	FVector HitDirection = FVector::ZeroVector;

	float Damage = 0.0f;
	bool bKilled = false;

	FString HitSoundPath;
};
