#pragma once

#include "GameFramework/Pawn/LuaCharacter.h"
#include "Game/Musou/Combat/AttackTypes.h"   // EAttackContext
// FMusouAttackStep/Slot/CameraShot — attack_data.lua 로드 데이터 (FMusouCameraShot 을
// 멤버로 들고 있어 전방 선언으로는 부족).
#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Math/Vector.h"

#include <utility>

#include "Source/Game/Musou/Character/MusouCharacter.generated.h"

class UBattleComponent;
class UComboComponent;
class UBoneAttachedStaticMeshComponent;
class UHitFlashComponent;
class UCineCameraComponent;
class APlayerCameraManager;
class UAnimMontage;
class UAnimSequence;

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

	// launcher(self_launch) 발동 시 AnimNotify_MusouAttack 이 호출 — 착지까지
	// 공중 공격이 저글 체인(AirborneJuggle)으로 진입한다. 일반 점프는 단발 점프 공격 유지.
	void OnSelfLaunched() { bJuggleAirborne = true; }

	// 피격 리액션 — GameMode::NotifyPlayerDamaged 가 호출. 평시 피격만 휘청
	// (공격/구르기/무쌍기/공중 중엔 모션 우선으로 스킵), 쿨다운으로 스턴락 방지.
	void PlayHitReaction();

	bool IsWeaponDrawn() const { return bWeaponDrawn; }

	// ── 몽타주 카메라 샷 — AnimNotifyState_CameraShot 이 구동 ──
	// 상시 연출 카메라 2대(핑퐁)로 메인(SpringArm) 카메라와 블렌드 전환.
	// Token = notify 객체 — 뒷 샷이 인수하면 앞 샷의 End 가 복귀를 걸지 않게 식별.
	void StartCameraShot(const FMusouCameraShot& Shot, const void* Token);
	void EndCameraShot(const void* Token);

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
	void OnUltimatePressed();     // R — 무쌍기 (게이지 가득 + 지상, 진행 동작 전부 캔슬)
	void OnDodgePressed();        // Shift — 구르기 (입력 방향, 전 구간 무적, 후딜 캔슬 가능)
	void OnToggleWeaponPressed(); // X — 발도/납도 토글 (납도 중 공격 입력도 발도로 변환)

	// 무기 상태를 무기 컴포넌트(손↔등 본)와 lua 애님 플래그("WeaponDrawn")에 동기화.
	void ApplyWeaponState();

	// 무쌍기 난무 — Tick 이 몽타주 종료를 감지해 다음 슬롯 자동 재생. 체인 소진 시 정리.
	void UpdateUltimateChain();
	void EndUltimate();
	void EndRoll();

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

	// 공중 콤보 행 타임 — 공중 체인 진행 중 CMC 중력을 줄여 체공 연장, 종료 시 원복.
	// 매 Tick 호출 (feedback.air_combo.gravity_scale, lua 튜닝).
	void UpdateAirComboHang();

	// 공격 스텝 재생 — 에디터 몽타주 우선, 없으면 시퀀스에서 런타임 생성 (기본 notify 주입).
	bool          PlayAttackStep(const FMusouAttackStep& Step);
	UAnimMontage* ResolveStepMontage(const FMusouAttackStep& Step);
	void          InjectDefaultAttackNotifies(UAnimSequence* Sequence, const FMusouAttackStep& Step);

	// 카메라 샷 notify 주입 — 공격 notify 와 별도 패스. 저작 notify 가 있는 시퀀스에도
	// 카메라 샷은 주입한다 (저작 CameraShot notify 가 있을 때만 양보).
	void InjectCameraShotNotifies(UAnimSequence* Sequence, const FMusouAttackStep& Step);

	// ── 몽타주 카메라 샷 내부 ──
	void EnsureCinematicCameras();          // 연출 카메라 2대 런타임 생성 (BeginPlay, 씬 비저장)
	void UpdateCameraShot();                // look_at / 월드 고정 유지 + 안전망 (Tick)
	void AimShotCamera();                   // 활성 샷 카메라 시선/위치 갱신
	APlayerCameraManager* GetLocalCameraManager() const;

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

	// 공중 콤보 행 타임 상태 — 적용 중이면 SavedGravity 로 원복해야 한다.
	bool  bAirComboHangActive = false;
	float SavedGravity = 9.8f;

	// launcher 로 떠오른 상태 — 공중 공격이 저글 체인으로 진입. 착지 시 해제 (Tick).
	bool  bJuggleAirborne = false;

	// 무쌍기 난무 상태 — 활성 동안 무적 + 슬롯 순차 자동 재생 (UltimateStep = 다음 인덱스).
	bool  bUltimateActive = false;
	int32 UltimateStep = 0;

	// 구르기 상태 — 활성 동안 무적. 몽타주 종료 시 해제 (Tick).
	bool  bRolling = false;

	// 피격 리액션 쿨다운 잔여 (초) — 군체 다단 히트 스턴락 방지 (feedback.hit_react.cooldown).
	float HitReactCooldownRemaining = 0.0f;

	// 무기 상태 — false = 납도(등에 멘 상태, 시작 기본값). 납도 중 공격 입력은 발도로 변환.
	bool  bWeaponDrawn = false;

	// 발도/납도 모션 중간 본 스왑 대기 (초, 음수 = 없음). Tick 이 카운트다운 후 적용 —
	// 손이 등에 닿는 타이밍(feedback.weapon.swap_frac)에 무기가 손↔등으로 옮겨진다.
	float WeaponSwapDelay = -1.0f;
	bool  bPendingWeaponDrawn = false;

	// ── 몽타주 카메라 샷 상태 ──
	// 연출 카메라 2대 — BeginPlay 런타임 생성 (씬 비저장, 캡슐에 부착). 핑퐁으로
	// 샷1→샷2 연속 컷에서도 블렌드 source/target 이 항상 다른 컴포넌트가 된다.
	UCineCameraComponent* CineCamA = nullptr;
	UCineCameraComponent* CineCamB = nullptr;

	const void*           ActiveShotToken = nullptr;  // 진행 중 샷의 notify 객체 (null = 없음)
	UCineCameraComponent* ActiveShotCam   = nullptr;  // 직전/현재 샷 카메라 — 핑퐁 판단용
	FMusouCameraShot      ActiveShot;
	FVector               ShotWorldLock = FVector(0.0f, 0.0f, 0.0f);  // bFollow=false 샷의 고정 월드 위치

	// 카메라 샷 주입 이력 (시퀀스 → 데이터 버전) — 공격 notify 주입과 별도 추적
	// (가드 조건이 달라 같은 시퀀스라도 주입 가능 여부가 다르다).
	TArray<std::pair<UAnimSequence*, int32>> CameraShotInjectedVersions;
};
