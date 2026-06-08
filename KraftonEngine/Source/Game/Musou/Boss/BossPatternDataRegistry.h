#pragma once

#include "Game/Musou/Boss/BossPatternTypes.h"

class FBossPatternDataRegistry
{
public:
	static FBossPatternDataRegistry& Get();

	void EnsureFresh();
	const FBossDefinition* FindBoss(const FName& BossId) const;

private:
	FBossPatternDataRegistry() = default;

	bool LoadFromLua();
	void LoadDefaults();

	TArray<FBossDefinition> Bosses;
	bool bLoadedOnce = false;
	long long LastWriteStamp = 0;
};
