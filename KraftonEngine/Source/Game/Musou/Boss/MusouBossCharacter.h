#pragma once

#include "GameFramework/Pawn/LuaCharacter.h"
#include "Object/FName.h"

#include "Source/Game/Musou/Boss/MusouBossCharacter.generated.h"

class UBattleComponent;
class UBossPatternComponent;

UCLASS()
class AMusouBossCharacter : public ALuaCharacter
{
public:
	GENERATED_BODY()
	AMusouBossCharacter() = default;
	~AMusouBossCharacter() override = default;

	static constexpr const char* DefaultMeshPath = "Content/Data/GameJam/Knight/SK_Knight_SkeletalMesh.uasset";
	static constexpr const char* DefaultAnimScript = "Anim/boss_knight_anim.lua";
	static constexpr const char* DefaultBossScript = "template.lua";

	void InitDefaultComponents();
	void InitDefaultComponents(const FString& SkeletalMeshFileName) override;

	void BeginPlay() override;
	void PostDuplicate() override;
	void PostLoad() override;

	UBattleComponent* GetBattleComponent() const { return BattleComponent; }
	UBossPatternComponent* GetPatternComponent() const { return PatternComponent; }

	UPROPERTY(Edit, Save, Category="Boss", DisplayName="Boss Id")
	FName BossId = FName("knight_boss");

private:
	void ApplyBossDefinition();
	void EnsureBossAnimation(const FString& AnimScript);

	UBattleComponent* BattleComponent = nullptr;
	UBossPatternComponent* PatternComponent = nullptr;
};
