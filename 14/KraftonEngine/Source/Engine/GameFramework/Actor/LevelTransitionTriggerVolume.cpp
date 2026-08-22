#include "GameFramework/Actor/LevelTransitionTriggerVolume.h"

#include "Core/Logging/Log.h"
#include "Engine/Runtime/Engine.h"

void ALevelTransitionTriggerVolume::OnPossessedPawnEntered(APawn* /*Pawn*/)
{
	if (bOneShot && bTransitionRequested)
	{
		return;
	}

	if (TargetSceneName.empty())
	{
		UE_LOG("[LevelTransitionTriggerVolume] TargetSceneName is empty");
		return;
	}

	if (GEngine)
	{
		bTransitionRequested = true;
		UE_LOG("[LevelTransitionTriggerVolume] Request transition to scene: %s", TargetSceneName.c_str());
		GEngine->RequestTransitionToScene(TargetSceneName);
	}
	else
	{
		UE_LOG("[LevelTransitionTriggerVolume] Cannot transition because GEngine is null");
	}
}
