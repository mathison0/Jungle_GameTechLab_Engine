#pragma once

#include "GameFramework/Actor/TriggerVolumeBase.h"

#include "Source/Engine/GameFramework/Actor/LevelTransitionTriggerVolume.generated.h"
class APawn;

UCLASS()
class ALevelTransitionTriggerVolume : public ATriggerVolumeBase
{
public:
	GENERATED_BODY()
	ALevelTransitionTriggerVolume() = default;
	~ALevelTransitionTriggerVolume() override = default;

	void OnPossessedPawnEntered(APawn* Pawn) override;

	const FString& GetTargetSceneName() const { return TargetSceneName; }
	void SetTargetSceneName(const FString& InSceneName) { TargetSceneName = InSceneName; }

	bool IsOneShot() const { return bOneShot; }
	void SetOneShot(bool bInOneShot) { bOneShot = bInOneShot; }

private:
	UPROPERTY(Edit, Save, Category="LevelTransition", DisplayName="Target Scene Name")
	FString TargetSceneName = "Play2";

	UPROPERTY(Edit, Save, Category="LevelTransition", DisplayName="One Shot")
	bool bOneShot = true;

	bool bTransitionRequested = false;
};
