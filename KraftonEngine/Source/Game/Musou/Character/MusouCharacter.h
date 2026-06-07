#pragma once

#include "GameFramework/Pawn/LuaCharacter.h"
#include "Game/Musou/Combat/AttackTypes.h"   // EAttackContext
#include "Math/Vector.h"

#include <utility>

#include "Source/Game/Musou/Character/MusouCharacter.generated.h"

class UBattleComponent;
class UComboComponent;
class UBoneAttachedStaticMeshComponent;
class UHitFlashComponent;
class UAnimMontage;
class UAnimSequence;

// 공격 스텝/슬롯 정의 — AttackDataRegistry.h (Content/Script/Data/attack_data.lua 에서 로드).
struct FMusouAttackStep;
struct FMusouAttackSlot;

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
	void OnAttackPressed();       // 좌클릭 — 콤보 체인 시작/예약 (컨텍스트별 체인)
	void OnHeavyAttackPressed();  // 우클릭 — 강공격 (컨텍스트별 단발 / 콤보 중엔 분기 예약)

	// 진입 컨텍스트 판정 — Falling → Airborne, XY 속도 ≥ 임계 → Moving, 그 외 Idle.
	EAttackContext ResolveAttackContext() const;

	// 공격 스텝 시작 시 이번 프레임 WASD 입력 방향으로 캡슐 yaw 즉시 회전 (입력 없으면 유지).
	void SnapFacingToInput();

	// 몽타주에 의한 이동/점프 잠금 — 말미 MontageMoveUnlockTail 구간과 blend-out 중엔 해제
	// (UE 의 BlendOutTriggerTime 개념 이식: 후딜 꼬리에서 컨트롤 자연 복귀).
	bool IsMovementLockedByMontage() const;

	// 잠금 해제 구간에서 이동 입력이 오면 몽타주 조기 blend-out (UE 의 recovery cancel 패턴).
	// 콤보 전진/분기 예약이 살아 있으면 보류 — 체인이 끊기지 않게.
	void TryMovementCancelMontage();

	// 공격 스텝 재생 — 에디터 몽타주 우선, 없으면 시퀀스에서 런타임 생성 (기본 notify 주입).
	bool          PlayAttackStep(const FMusouAttackStep& Step);
	UAnimMontage* ResolveStepMontage(const FMusouAttackStep& Step);
	void          InjectDefaultAttackNotifies(UAnimSequence* Sequence, const FMusouAttackStep& Step);

	// 슬롯에서 변주 1개 선택 — 랜덤 + 직전 변주 반복 회피. 빈 슬롯이면 nullptr.
	const FMusouAttackStep* PickVariant(const FMusouAttackSlot& Slot);
	bool PlayAttackSlot(const FMusouAttackSlot& Slot);   // PickVariant → PlayAttackStep

	void PlayComboStep(int32 Step);
	void PlayBranchFinisher(int32 BranchStep);  // 콤보 N단 분기 피니셔 (무쌍 차지어택식)
	bool IsAnyMontagePlaying() const;
	bool IsFalling() const;

	UBattleComponent* BattleComponent = nullptr;
	UComboComponent*  ComboComponent  = nullptr;
	UBoneAttachedStaticMeshComponent* WeaponComponent = nullptr;  // 오른손(hand_r) 무기 슬롯

	UPROPERTY(Edit, Save, Category = "Combat|FX")
	UHitFlashComponent* HitFlashComponent = nullptr;

	// 이동 중 공격 판정 임계 (m/s, XY) — MaxWalkSpeed 6.0 의 1/3 기준.
	UPROPERTY(Edit, Save, Category = "Combat", DisplayName = "Moving Attack Speed Threshold", Min=0.0f, Max=10.0f, Speed=0.1f)
	float MovingAttackSpeedThreshold = 2.0f;

	// 몽타주 말미에서 이동 잠금이 풀리는 여유 시간 (초) — 0 이면 끝까지 잠금.
	UPROPERTY(Edit, Save, Category = "Combat", DisplayName = "Montage Move Unlock Tail", Min=0.0f, Max=1.0f, Speed=0.01f)
	float MontageMoveUnlockTail = 0.2f;

	// 콤보 시작 시점에 고정되는 활성 체인 컨텍스트 — 진행 중 컨텍스트 변화에 영향받지 않음.
	EAttackContext ActiveChainContext = EAttackContext::Idle;

	// 이번 프레임 WASD 입력의 월드 방향 (카메라 yaw 기준). 축 바인딩이 매 프레임 재구축 —
	// 공격 시작 회전 스냅(SnapFacingToInput)의 입력 소스. 입력 없으면 영벡터.
	FVector MoveInputThisFrame = FVector(0.0f, 0.0f, 0.0f);

	// 런타임 fallback 몽타주 캐시 (시퀀스 경로 → 생성 몽타주). 에디터 저작 몽타주가
	// 없을 때만 채워짐 — 액터 수명과 함께 정리.
	TArray<std::pair<FString, UAnimMontage*>> RuntimeAttackMontages;

	// notify 주입 이력 (시퀀스 → 주입 시점의 attack_data 버전). 핫리로드로 버전이
	// 바뀌면 Auto* notify 를 걷어내고 새 값으로 재주입 — 라이브 타이밍 튜닝용.
	TArray<std::pair<UAnimSequence*, int32>> InjectedSequenceVersions;

	// 슬롯별 직전 변주 인덱스 (같은 모션 연속 재생 회피). key = 슬롯 주소 —
	// 데이터 핫리로드로 슬롯이 재구성되면 자연히 미스나서 새로 기록된다.
	TArray<std::pair<const void*, int32>> LastVariantPick;
};
