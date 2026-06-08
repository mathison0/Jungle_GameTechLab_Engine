#pragma once

#include "GameFramework/Pawn/LuaCharacter.h"

#include "Source/Game/Musou/MainBoss/MainBossCharacter.generated.h"

class UBattleComponent;
class UHitFlashComponent;
class UMainBossPatternComponent;

UCLASS()
class AMainBossCharacter : public ALuaCharacter
{
public:
	GENERATED_BODY()
	AMainBossCharacter() = default;
	~AMainBossCharacter() override = default;

	static constexpr const char* DefaultMeshPath = "Content/Data/GameJam/Golem_Boss/SM_Golem_Boss_SkeletalMesh.uasset";

	void InitDefaultComponents();
	void InitDefaultComponents(const FString& SkeletalMeshFileName) override;

	void BeginPlay() override;
	void PostDuplicate() override;
	void PostLoad() override;

	UBattleComponent* GetBattleComponent() const { return BattleComponent; }
	UMainBossPatternComponent* GetPatternComponent() const { return PatternComponent; }
	FString GetBossDisplayName() const { return DisplayName.empty() ? FString("Main Boss") : DisplayName; }

	UPROPERTY(Edit, Save, Category="Main Boss", DisplayName="Display Name")
	FString DisplayName = "Golem";

private:
	void InitializeHitFlash();

	UBattleComponent* BattleComponent = nullptr;
	UMainBossPatternComponent* PatternComponent = nullptr;
	UHitFlashComponent* HitFlashComponent = nullptr;
};
