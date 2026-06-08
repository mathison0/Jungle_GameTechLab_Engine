#include "Game/Musou/MainBoss/AnimNotify_GolemFootstep.h"

#include "Audio/AudioManager.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <array>

namespace
{
	constexpr std::array<const char*, 4> GolemFootstepPaths = {
		"Content/Data/GameJam/Golem_Boss/SFX/Golem_HeavyFootstep_01.wav",
		"Content/Data/GameJam/Golem_Boss/SFX/Golem_HeavyFootstep_02.wav",
		"Content/Data/GameJam/Golem_Boss/SFX/Golem_HeavyFootstep_03.wav",
		"Content/Data/GameJam/Golem_Boss/SFX/Golem_HeavyFootstep_04.wav",
	};

	TSet<FString> GLoadedGolemFootstepPaths;
	uint32 GolemFootstepRandomState = 0x9e3779b9u;
	int32 LastGolemFootstepIndex = -1;

	uint32 NextGolemFootstepRandom()
	{
		uint32 X = GolemFootstepRandomState;
		X ^= X << 13;
		X ^= X >> 17;
		X ^= X << 5;
		GolemFootstepRandomState = X ? X : 0x9e3779b9u;
		return GolemFootstepRandomState;
	}

	int32 PickGolemFootstepIndex()
	{
		const int32 Count = static_cast<int32>(GolemFootstepPaths.size());
		int32 Index = static_cast<int32>(NextGolemFootstepRandom() % Count);
		if (Count > 1 && Index == LastGolemFootstepIndex)
		{
			Index = (Index + 1) % Count;
		}

		LastGolemFootstepIndex = Index;
		return Index;
	}
}

void UAnimNotify_GolemFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)MeshComp;
	(void)Anim;

	const int32 Index = PickGolemFootstepIndex();
	const FString SoundPath = GolemFootstepPaths[Index];
	const FString Key = FString("GolemFootstep:") + SoundPath;

	if (GLoadedGolemFootstepPaths.find(SoundPath) == GLoadedGolemFootstepPaths.end())
	{
		if (FAudioManager::Get().LoadAudio(Key, SoundPath, false))
		{
			GLoadedGolemFootstepPaths.insert(SoundPath);
		}
		else
		{
			UE_LOG("[AnimNotify_GolemFootstep] LoadAudio failed: %s", SoundPath.c_str());
			return;
		}
	}

	FAudioManager::Get().PlaySfxLimited(
		Key,
		(std::max)(Volume, 0.0f),
		(std::max)(MaxConcurrent, 1),
		(std::max)(MinIntervalSeconds, 0.0f));
}
