#pragma once

#include "GameFramework/Pawn/LuaCharacter.h"

#include "Source/Game/Musou/Character/MusouCharacter.generated.h"

class UBattleComponent;
class UComboComponent;
class UBoneAttachedStaticMeshComponent;
class UHitFlashComponent;

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

	void BeginPlay() override;

	void PostDuplicate() override;
	void PostLoad() override;

	UBattleComponent* GetBattleComponent() const { return BattleComponent; }
	UComboComponent*  GetComboComponent()  const { return ComboComponent; }
	UBoneAttachedStaticMeshComponent* GetWeaponComponent() const { return WeaponComponent; }

protected:
	// 입력 binding — WASD 이동/Space 점프 + 좌클릭 콤보/우클릭 강공격.
	// ※ 공격 입력을 lua anim에서 이관한 이유: lua update()는 Animation Tick LOD
	//   게이트 뒤에서 돌아 스킵 프레임의 에지 입력(GetKeyDown)이 소실된다.
	//   InputComponent는 액터 틱에서 매 프레임 처리 — 입력 누락 없음.
	void SetupInputComponent() override;

	// 콤보 단계 전진/리셋 폴링 (매 프레임).
	void Tick(float DeltaTime) override;

	// ── 공격 입력 핸들러 ──
	void OnAttackPressed();       // 좌클릭 — 콤보 체인 시작/예약
	void OnHeavyAttackPressed();  // 우클릭 — 강공격 (현재 attack2 몽타주)

	// 몽타주 재생 헬퍼 — 경로 로드 + DefaultSlot 재생. 실패 시 false.
	bool PlayMontagePath(const char* Path, float BlendIn);
	void PlayComboStep(int32 Step);
	bool IsAnyMontagePlaying() const;
	bool IsFalling() const;

	UBattleComponent* BattleComponent = nullptr;
	UComboComponent*  ComboComponent  = nullptr;
	UBoneAttachedStaticMeshComponent* WeaponComponent = nullptr;  // 오른손(hand_r) 무기 슬롯

	UPROPERTY(Edit, Save, Category = "Combat|FX")
	UHitFlashComponent* HitFlashComponent = nullptr;
};
