#include "Game/Musou/Character/MusouCharacter.h"

#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/ComboComponent.h"
#include "Game/Musou/Combat/AnimNotify_MusouAttack.h"
#include "Game/Musou/Combat/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimationManager.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimDataModel.h"
#include "Component/Input/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/BoneAttachedStaticMeshComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Math/Rotator.h"

#include <cmath>

// ── 공격 체인 테이블 행 ──
// MontagePath 가 있으면 에디터 저작 몽타주 우선 (notify 는 source 시퀀스에 저작).
// 로드 실패 시 SequencePath 의 시퀀스로 몽타주를 런타임 생성하고 기본 notify
// (MusouAttack 히트 + ComboWindow) 를 PlayLength 비율 위치에 주입한다 —
// 에디터에서 몽타주를 저작해 같은 경로에 저장하면 다음 세션부터 그쪽이 자동 우선.
struct FMusouAttackStepDef
{
	const char* MontagePath;      // null 가능 — fallback 전용 스텝
	const char* SequencePath;     // null 이면 fallback 없음 (몽타주 필수)
	float       BlendIn;

	// ── fallback notify 주입 파라미터 (SequencePath 경로로 생성될 때만 사용) ──
	const char* AttackId;         // AttackTypes.h 테이블 키. null = 히트 notify 안 박음
	float       HitFrac;          // MusouAttack 위치 (PlayLength 비율). <0 = 없음
	float       WindowBeginFrac;  // ComboWindow 시작 비율. <0 = 윈도우 없음 (체인 말단/단발)
	float       WindowEndFrac;
};

namespace
{
	// ── 좌클릭 콤보 체인 — 진입 컨텍스트별 ──
	// 기존 3단 (몽타주/notify 에디터 저작 완료)
	constexpr FMusouAttackStepDef GIdleLightChain[] =
	{
		{ "Content/Montages/Barbarian_Melee Combo Attack Ver. 1_Montage.uasset", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f },
		{ "Content/Montages/Barbarian_Melee Combo Attack Ver. 2_Montage.uasset", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f },
		{ "Content/Montages/Barbarian_Melee Combo Attack Ver. 3_Montage.uasset", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f },
	};

	// 이동 중 — 1단이 돌진 베기 (slide attack, RM +3.56m) 로 바뀌고 2단부터 제자리 체인 합류.
	constexpr FMusouAttackStepDef GMovingLightChain[] =
	{
		{ "Content/Montages/great sword slide attack_mixamo_com_Montage.uasset",
		  "Content/Data/GameJam/Barbarian/great sword slide attack_mixamo_com.uasset",
		  0.1f, "dash_attack", 0.35f, 0.55f, 0.85f },
		{ "Content/Montages/Barbarian_Melee Combo Attack Ver. 2_Montage.uasset", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f },
		{ "Content/Montages/Barbarian_Melee Combo Attack Ver. 3_Montage.uasset", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f },
	};

	// 공중 — 단발 도약 내려찍기 (RM 전진 +3.25m, Z 는 CMC 정책상 gravity 가 결정).
	// 착지 후에도 끝까지 재생 — 전진 RM 이 착지 돌진감을 만든다.
	constexpr FMusouAttackStepDef GAirLightChain[] =
	{
		{ "Content/Montages/great sword jump attack_mixamo_com_Montage.uasset",
		  "Content/Data/GameJam/Barbarian/great sword jump attack_mixamo_com.uasset",
		  0.15f, "jump_attack", 0.45f, -1.0f, -1.0f },
	};

	// ── 우클릭 강공격 — 컨텍스트별 단발 ──
	constexpr FMusouAttackStepDef GIdleHeavyStep =
		{ "Content/Montages/Barbarian_Melee Attack Backhand_Montage.uasset", nullptr, 0.2f, nullptr, -1.0f, -1.0f, -1.0f };

	// 이동 중 — 전진 회전베기 (high spin attack, RM +2.43m)
	constexpr FMusouAttackStepDef GMovingHeavyStep =
		{ "Content/Montages/great sword high spin attack_mixamo_com_Montage.uasset",
		  "Content/Data/GameJam/Barbarian/great sword high spin attack_mixamo_com.uasset",
		  0.15f, "spin_attack", 0.40f, -1.0f, -1.0f };

	// 공중 — 좌클릭과 동일한 도약 내려찍기 (전용 모션 확보 시 교체)
	constexpr FMusouAttackStepDef GAirHeavyStep = GAirLightChain[0];

	// ── 콤보 분기 피니셔 — 콤보 N단 진행 중 강공격 (무쌍 차지어택식 □..△) ──
	// 인덱스 = 분기 시점 단수 - 1. 깊을수록 화려/강력. 윈도우 없음 — 분기 후 체인 종료.
	// 몽타주는 전부 에디터 저작본 존재; Horizontal/360 Low 시퀀스엔 히트 notify 가 없어
	// ResolveStepMontage 가 주입(branch1/2), 360 High 는 attack1 저작본 존중 (주입 스킵).
	constexpr FMusouAttackStepDef GBranchFinishers[] =
	{
		{ "Content/Montages/Barbarian_Melee Attack Horizontal_Montage.uasset",
		  "Content/Data/GameJam/Barbarian/Barbarian_Melee Attack Horizontal.uasset",
		  0.1f, "branch1", 0.40f, -1.0f, -1.0f },  // 1단 분기 — 와이드 횡베기
		{ "Content/Montages/Barbarian_Melee Attack 360 Low_Montage.uasset",
		  "Content/Data/GameJam/Barbarian/Barbarian_Melee Attack 360 Low.uasset",
		  0.1f, "branch2", 0.45f, -1.0f, -1.0f },  // 2단 분기 — 로우 스핀 (전방위)
		{ "Content/Montages/Barbarian_Melee Attack 360 High_Montage.uasset",
		  "Content/Data/GameJam/Barbarian/Barbarian_Melee Attack 360 High.uasset",
		  0.1f, "attack1", 0.45f, -1.0f, -1.0f },  // 3단 분기 — 하이 스핀 대피니셔
	};

	const FMusouAttackStepDef* GetLightChain(EAttackContext Context, int32& OutNumSteps)
	{
		switch (Context)
		{
		case EAttackContext::Moving:
			OutNumSteps = static_cast<int32>(sizeof(GMovingLightChain) / sizeof(GMovingLightChain[0]));
			return GMovingLightChain;
		case EAttackContext::Airborne:
			OutNumSteps = static_cast<int32>(sizeof(GAirLightChain) / sizeof(GAirLightChain[0]));
			return GAirLightChain;
		default:
			OutNumSteps = static_cast<int32>(sizeof(GIdleLightChain) / sizeof(GIdleLightChain[0]));
			return GIdleLightChain;
		}
	}

	const FMusouAttackStepDef& GetHeavyStep(EAttackContext Context)
	{
		switch (Context)
		{
		case EAttackContext::Moving:   return GMovingHeavyStep;
		case EAttackContext::Airborne: return GAirHeavyStep;
		default:                       return GIdleHeavyStep;
		}
	}
}

void AMusouCharacter::InitDefaultComponents()
{
	InitDefaultComponents(FString(DefaultMeshPath));
}

void AMusouCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	// Capsule/Mesh/CharacterMovement + SpringArm/Camera +
	// LuaScriptComponent(Game/barbarian_character.lua — 히트 판정/피드백).
	ALuaCharacter::InitDefaultComponents(SkeletalMeshFileName, FString(DefaultPawnScript));

	// ── 애님: Custom 모드 + ULuaAnimInstance + player_anim.lua ──
	if (Mesh)
	{
		// 플레이어는 Anim Tick LOD 제외 — 평가 스킵 프레임에 root motion 이 버스트로
		// 적용되어(0,0,Δ,0,0,Δ) 캡슐/카메라가 덜컹거리고, 에지 입력/notify 분해능도 떨어진다.
		Mesh->SetEnableAnimationTickLOD(false);

		Mesh->SetAnimInstanceClass(ULuaAnimInstance::StaticClass());
		Mesh->SetAnimationMode(EAnimationMode::AnimationCustom);

		// 인스턴스 생성 후 ScriptFile을 지정하고 재초기화 (init에서 그래프 빌드).
		if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(Mesh->GetAnimInstance()))
		{
			LuaAnim->ScriptFile = DefaultAnimScript;
			Mesh->InitializeAnimation();
		}
	}

	// ── 전투 컴포넌트 ──
	BattleComponent = AddComponent<UBattleComponent>();
	BattleComponent->bIsPlayerTeam = true;  // 플레이어 진영 — 적(군체/보스) 공격만 수신
	ComboComponent  = AddComponent<UComboComponent>();

	// ── 무기 슬롯 — 오른손 본 추적 (Mesh보다 늦게 추가 → 같은 프레임 포즈를 따름) ──
	// 무기 메시는 에디터에서 Static Mesh 프로퍼티로 지정, 그립 오프셋도 에디터에서
	// 실시간 조정 (컴포넌트 editor tick 지원).
	WeaponComponent = AddComponent<UBoneAttachedStaticMeshComponent>();
	WeaponComponent->TargetBoneName = "hand_r";  // Barbarian(UE 마네킹 리그) 오른손 본

	// 컴포넌트 트리(인스펙터)에 보이려면 씬 그래프에 attach 필요.
	// 본 추적은 부모 기준 relative 변환을 처리하므로 attach해도 동작 동일.
	if (Mesh)
	{
		WeaponComponent->AttachToComponent(Mesh);
	}

	HitFlashComponent = AddComponent<UHitFlashComponent>();
	if (Mesh)
	{
		HitFlashComponent->InitializeFromSkinnedMesh(Mesh);
	}
}

void AMusouCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HitFlashComponent && Mesh)
	{
		HitFlashComponent->InitializeFromSkinnedMesh(Mesh);
	}
}

void AMusouCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	ComboComponent  = GetComponentByClass<UComboComponent>();
	WeaponComponent = GetComponentByClass<UBoneAttachedStaticMeshComponent>();
	HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
}

void AMusouCharacter::PostLoad()
{
	Super::PostLoad();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	ComboComponent  = GetComponentByClass<UComboComponent>();
	WeaponComponent = GetComponentByClass<UBoneAttachedStaticMeshComponent>();
	HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
}

void AMusouCharacter::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// ── WASD 이동 (ACharacter에서 이관) ──
	// forward/right 는 ControlRotation.Yaw 기준 — capsule rotation 과 무관.
	// "카메라가 보는 방향" (yaw 만, pitch 무시) 으로 이동.
	InputComponent->AddAxisMapping("MoveForward", 'W',  1.0f);
	InputComponent->AddAxisMapping("MoveForward", 'S', -1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'D',  1.0f);
	InputComponent->AddAxisMapping("MoveRight",   'A', -1.0f);

	// MoveInputThisFrame: 축 바인딩은 value=0 이어도 매 프레임 등록 순서대로 호출되므로
	// MoveForward(첫 바인딩)가 재설정, MoveRight 가 합산 — 프레임 단위로 깨끗하게 재구축.
	// 액션 바인딩(공격)은 축 이후에 돌아 같은 프레임 입력을 SnapFacingToInput 이 읽는다.
	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		MoveInputThisFrame = YawOnly.GetForwardVector() * Value;
		if (Value == 0.0f) return;
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		MoveInputThisFrame += YawOnly.GetRightVector() * Value;
		if (Value == 0.0f) return;
		AddMovementInput(YawOnly.GetRightVector(), Value);
	});

	// Space = Jump (VK_SPACE = 0x20). Walking 중에만 effective (CharacterMovement::Jump 가 guard).
	InputComponent->AddActionMapping("Jump", 0x20);
	InputComponent->BindAction("Jump", EInputEvent::Pressed, [this]()
	{
		Jump();
	});

	// ── 공격 입력 — lua anim에서 이관 (Tick LOD 에지 소실 방지) ──
	// VK_LBUTTON = 0x01, VK_RBUTTON = 0x02
	InputComponent->AddActionMapping("Attack", 0x01);
	InputComponent->BindAction("Attack", EInputEvent::Pressed, [this]()
	{
		OnAttackPressed();
	});

	InputComponent->AddActionMapping("HeavyAttack", 0x02);
	InputComponent->BindAction("HeavyAttack", EInputEvent::Pressed, [this]()
	{
		OnHeavyAttackPressed();
	});

	InputComponent->AddActionMapping("TestHitFlash", 'F');
	InputComponent->BindAction("TestHitFlash", EInputEvent::Pressed, [this]()
	{
		if (HitFlashComponent)
		{
			HitFlashComponent->PlayFlash();
		}
	});

	// TODO: 스킬 1/2/3 ('1'/'2'/'3') — 스킬 몽타주 확정 후 추가
}

void AMusouCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ComboComponent)
	{
		return;
	}

	// 콤보 리셋 — 체인 끊김(윈도우 내 미입력)/완주, 또는 지상 체인 중 낙하(절벽 등).
	// 공중 체인은 낙하가 전제라 낙하 리셋에서 제외 — 몽타주 종료로만 리셋.
	// 입력 콜백(InputComponent 틱)보다 늦게 돌더라도 다음 입력 전에 정리되면 충분.
	if (ComboComponent->IsComboActive())
	{
		const bool bAirChain = (ActiveChainContext == EAttackContext::Airborne);
		if (!IsAnyMontagePlaying() || (!bAirChain && IsFalling()))
		{
			ComboComponent->ResetCombo();
		}
	}

	// 분기 피니셔 — 윈도우 내 강공격 예약 소비. 라이트 전진보다 먼저 폴링
	// (예약 자체가 상호 배타라 동시 성립은 없지만, 우선순위를 코드로 명시).
	if (const int32 BranchStep = ComboComponent->ConsumeQueuedHeavyBranch())
	{
		PlayBranchFinisher(BranchStep);
	}
	// 콤보 단계 전진 — 윈도우 내 예약 입력 소비
	else if (ComboComponent->ConsumeQueuedAdvance())
	{
		PlayComboStep(ComboComponent->GetComboStep());
	}
}

void AMusouCharacter::OnAttackPressed()
{
	if (!ComboComponent)
	{
		return;
	}

	// 다른 몽타주(강공격/스킬) 재생 중엔 새 콤보 시작 금지.
	// 콤보 진행 중 입력은 TryAttack이 내부에서 다음 단계로 버퍼링.
	if (!ComboComponent->IsComboActive() && IsAnyMontagePlaying())
	{
		return;
	}

	// 새 콤보 시작 — 진입 컨텍스트를 이 시점에 한 번만 평가해 체인 고정.
	// (진행 중 컨텍스트가 바뀌어도 체인은 유지 — 2단부터 다른 체인으로 새지 않게.)
	if (!ComboComponent->IsComboActive())
	{
		ActiveChainContext = ResolveAttackContext();
		int32 NumSteps = 0;
		GetLightChain(ActiveChainContext, NumSteps);
		ComboComponent->SetMaxComboSteps(NumSteps);
	}

	if (ComboComponent->TryAttack())
	{
		PlayComboStep(1);
	}
}

void AMusouCharacter::OnHeavyAttackPressed()
{
	// 콤보 진행 중 — 분기 피니셔 예약 (□..△). 윈도우에서 Tick 이 소비해 단수별 피니셔 재생.
	if (ComboComponent && ComboComponent->IsComboActive())
	{
		ComboComponent->TryHeavyBranch();
		return;
	}

	if (IsAnyMontagePlaying())
	{
		return;
	}

	PlayAttackStep(GetHeavyStep(ResolveAttackContext()));
}

EAttackContext AMusouCharacter::ResolveAttackContext() const
{
	if (IsFalling())
	{
		return EAttackContext::Airborne;
	}

	if (CharacterMovement)
	{
		const FVector& V = CharacterMovement->GetVelocity();
		const float SpeedXYSq = V.X * V.X + V.Y * V.Y;
		if (SpeedXYSq >= MovingAttackSpeedThreshold * MovingAttackSpeedThreshold)
		{
			return EAttackContext::Moving;
		}
	}
	return EAttackContext::Idle;
}

void AMusouCharacter::SnapFacingToInput()
{
	if (!CapsuleComponent)
	{
		return;
	}

	// 입력 없으면 현재 facing 유지. (RM 전진이 새 facing 방향으로 나가고, 이후
	// PhysOrientToMovement 도 같은 방향 속도를 따라가므로 스냅과 충돌하지 않는다.)
	const FVector& Input = MoveInputThisFrame;
	const float LenSq2D = Input.X * Input.X + Input.Y * Input.Y;
	if (LenSq2D < 1e-4f)
	{
		return;
	}

	// PhysOrientToMovement 와 동일한 yaw 좌표계 — atan2(Y, X), +X 가 0°.
	const float TargetYaw = std::atan2(Input.Y, Input.X) * (180.0f / 3.14159265f);

	FRotator R = CapsuleComponent->GetRelativeRotation();
	R.Yaw = TargetYaw;
	CapsuleComponent->SetRelativeRotation(R);
}

bool AMusouCharacter::PlayAttackStep(const FMusouAttackStepDef& Step)
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	UAnimMontage* Montage = ResolveStepMontage(Step);
	if (!Montage)
	{
		return false;
	}

	// 회전 스냅 — 모든 공격 스텝(시작/전진/분기/강공격) 공통. 스텝마다 재조준 가능.
	SnapFacingToInput();

	AnimInstance->PlayMontage(Montage, FName::None, 1.0f, Step.BlendIn);
	return true;
}

UAnimMontage* AMusouCharacter::ResolveStepMontage(const FMusouAttackStepDef& Step)
{
	FAnimationManager& Manager = FAnimationManager::Get();

	// 1) 런타임 생성 캐시 — fallback 으로 한 번 만든 몽타주 재사용 (실패 로그 반복 방지).
	if (Step.SequencePath)
	{
		for (const auto& Entry : RuntimeAttackMontages)
		{
			if (Entry.first == Step.SequencePath)
			{
				return Entry.second;
			}
		}
	}

	// 2) 에디터 저작 몽타주 우선. 단 소스 시퀀스에 notify 가 하나도 저작 안 된 경우
	//    (Horizontal/360 Low 등 모션만 있는 에셋) 는 fallback 과 동일하게 기본 notify 주입 —
	//    InjectDefaultAttackNotifies 가 저작본 존재 시 스스로 스킵하므로 무조건 호출해도 안전.
	if (Step.MontagePath)
	{
		if (UAnimMontage* Montage = Manager.LoadMontage(Step.MontagePath))
		{
			InjectDefaultAttackNotifies(Montage->GetSourceSequence(), Step);
			return Montage;
		}
	}

	// 3) Fallback — 시퀀스에서 런타임 생성 + 기본 notify 주입.
	if (!Step.SequencePath)
	{
		return nullptr;
	}

	UAnimSequence* Sequence = Manager.LoadAnimation(Step.SequencePath);
	if (!Sequence)
	{
		return nullptr;
	}

	InjectDefaultAttackNotifies(Sequence, Step);

	UAnimMontage* Montage = Manager.CreateMontage(Sequence, Sequence->GetName() + "_RuntimeMontage");
	if (!Montage)
	{
		return nullptr;
	}
	Montage->EnsureDefaultSection();

	RuntimeAttackMontages.push_back({ FString(Step.SequencePath), Montage });
	return Montage;
}

void AMusouCharacter::InjectDefaultAttackNotifies(UAnimSequence* Sequence, const FMusouAttackStepDef& Step)
{
	// 시퀀스에 이미 notify 가 저작돼 있으면 존중 — 주입 스킵.
	if (!Sequence || !Sequence->GetNotifies().empty())
	{
		return;
	}

	UAnimDataModel* Model = Sequence->GetDataModel();
	const float Length = Sequence->GetPlayLength();
	if (!Model || Length <= 0.0f)
	{
		return;
	}

	TArray<FAnimNotifyEvent>& ModelNotifies = Sequence->GetMutableModelNotifies();

	// 히트 판정 — 스윙 임팩트 추정 지점. 정확한 타이밍은 에디터 저작으로 대체 권장.
	if (Step.AttackId && Step.HitFrac >= 0.0f)
	{
		UAnimNotify_MusouAttack* Hit = UObjectManager::Get().CreateObject<UAnimNotify_MusouAttack>(Model);
		Hit->AttackId = Step.AttackId;

		FAnimNotifyEvent Event;
		Event.NotifyName  = FName("AutoMusouAttack");
		Event.TriggerTime = Length * Step.HitFrac;
		Event.Notify      = Hit;
		ModelNotifies.push_back(Event);
	}

	// 콤보 윈도우 — 후딜 구간.
	if (Step.WindowBeginFrac >= 0.0f && Step.WindowEndFrac > Step.WindowBeginFrac)
	{
		FAnimNotifyEvent Event;
		Event.NotifyName  = FName("AutoComboWindow");
		Event.TriggerTime = Length * Step.WindowBeginFrac;
		Event.Duration    = Length * (Step.WindowEndFrac - Step.WindowBeginFrac);
		Event.NotifyState = UObjectManager::Get().CreateObject<UAnimNotifyState_ComboWindow>(Model);
		ModelNotifies.push_back(Event);
	}

	Sequence->RefreshRuntimeNotifies();
}

void AMusouCharacter::PlayComboStep(int32 Step)
{
	int32 NumSteps = 0;
	const FMusouAttackStepDef* Chain = GetLightChain(ActiveChainContext, NumSteps);
	if (!Chain || Step < 1 || Step > NumSteps)
	{
		return;
	}

	PlayAttackStep(Chain[Step - 1]);
}

void AMusouCharacter::PlayBranchFinisher(int32 BranchStep)
{
	constexpr int32 NumBranches = static_cast<int32>(sizeof(GBranchFinishers) / sizeof(GBranchFinishers[0]));
	if (BranchStep < 1)
	{
		return;
	}

	// 테이블보다 깊은 단수(향후 4단+ 체인)는 마지막 분기로 clamp — 대피니셔 공유.
	const int32 Index = (BranchStep <= NumBranches) ? BranchStep - 1 : NumBranches - 1;
	PlayAttackStep(GBranchFinishers[Index]);
}

bool AMusouCharacter::IsAnyMontagePlaying() const
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	return AnimInstance && AnimInstance->IsMontagePlaying();
}

bool AMusouCharacter::IsFalling() const
{
	return CharacterMovement && CharacterMovement->IsFalling();
}
