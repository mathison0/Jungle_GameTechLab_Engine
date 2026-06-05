#pragma once

#include "GameFramework/Pawn/LuaCharacter.h"

#include "Source/Game/Musou/Character/MusouCharacter.generated.h"

class UBattleComponent;
class UComboComponent;

// ============================================================
// AMusouCharacter — 무쌍 플레이어 캐릭터 (Barbarian)
//
// ALuaCharacter 기반:
//   - Capsule → SkeletalMesh(Barbarian) + SpringArm → Camera 체인
//   - 애님: ULuaAnimInstance + Anim/player_anim.lua (locomotion/공격 몽타주)
//   - 전투: UBattleComponent(체력/데미지) + UComboComponent(콤보 체인)
//
// 배치: 에디터 우클릭 → Place Actor → "Musou Character (Barbarian)"
// ============================================================
UCLASS()
class AMusouCharacter : public ALuaCharacter
{
public:
	GENERATED_BODY()
	AMusouCharacter() = default;
	~AMusouCharacter() override = default;

	// 기본 에셋 경로
	static constexpr const char* DefaultMeshPath = "Content/Data/GameJam/Barbarian/Barbarian_SkeletalMesh.uasset";
	static constexpr const char* DefaultAnimScript = "Anim/player_anim.lua";
	// Pawn 레벨 게임 로직 (히트 판정/피드백) — Content/Script 기준 상대경로
	static constexpr const char* DefaultPawnScript = "Game/barbarian_character.lua";

	// Barbarian 메시 + player_anim.lua로 구성하는 기본 진입점.
	void InitDefaultComponents();

	// 메시 교체 가능 버전 — 애님 스크립트/전투 컴포넌트는 동일하게 부착.
	void InitDefaultComponents(const FString& SkeletalMeshFileName) override;

	void PostDuplicate() override;

	UBattleComponent* GetBattleComponent() const { return BattleComponent; }
	UComboComponent*  GetComboComponent()  const { return ComboComponent; }

protected:
	UBattleComponent* BattleComponent = nullptr;
	UComboComponent*  ComboComponent  = nullptr;
};
