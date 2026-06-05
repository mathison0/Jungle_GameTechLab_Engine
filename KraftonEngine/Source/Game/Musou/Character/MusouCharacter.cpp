#include "Game/Musou/Character/MusouCharacter.h"

#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/ComboComponent.h"
#include "Animation/AnimationManager.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Component/Input/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Math/Rotator.h"

namespace
{
	// ── 공격 몽타주 경로 ──
	// 좌클릭 콤보 체인 — 각 몽타주에 AnimNotify_MusouAttack(comboN) + AnimNotifyState_ComboWindow 배치.
	constexpr const char* GComboMontages[] =
	{
		"Content/Montages/Barbarian_Melee Combo Attack Ver. 1_Montage.uasset",
		"Content/Montages/Barbarian_Melee Combo Attack Ver. 2_Montage.uasset",
		"Content/Montages/Barbarian_Melee Combo Attack Ver. 3_Montage.uasset",
	};
	constexpr int32 GComboMontageCount = sizeof(GComboMontages) / sizeof(GComboMontages[0]);

	// 우클릭 강공격 (추후 전용 몽타주로 교체 예정)
	constexpr const char* GHeavyAttackMontage = "Content/Montages/Barbarian_Melee Attack BackHand_Montage.uasset";

	constexpr float GComboBlendIn = 0.1f;
	constexpr float GHeavyBlendIn = 0.2f;
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
}

void AMusouCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	ComboComponent  = GetComponentByClass<UComboComponent>();
}

void AMusouCharacter::PostLoad()
{
	Super::PostLoad();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	ComboComponent  = GetComponentByClass<UComboComponent>();
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

	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		if (Value == 0.0f) return;
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
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

	// TODO: 스킬 1/2/3 ('1'/'2'/'3') — 스킬 몽타주 확정 후 추가
}

void AMusouCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ComboComponent)
	{
		return;
	}

	// 콤보 리셋 — 체인 끊김(윈도우 내 미입력)/완주/공중.
	// 입력 콜백(InputComponent 틱)보다 늦게 돌더라도 다음 입력 전에 정리되면 충분.
	if (ComboComponent->IsComboActive())
	{
		if (IsFalling() || !IsAnyMontagePlaying())
		{
			ComboComponent->ResetCombo();
		}
	}

	// 콤보 단계 전진 — 윈도우 내 예약 입력 소비
	if (ComboComponent->ConsumeQueuedAdvance())
	{
		PlayComboStep(ComboComponent->GetComboStep());
	}
}

void AMusouCharacter::OnAttackPressed()
{
	if (IsFalling())
	{
		return;
	}

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

	if (ComboComponent->TryAttack())
	{
		PlayComboStep(1);
	}
}

void AMusouCharacter::OnHeavyAttackPressed()
{
	if (IsFalling() || IsAnyMontagePlaying())
	{
		return;
	}

	PlayMontagePath(GHeavyAttackMontage, GHeavyBlendIn);
}

bool AMusouCharacter::PlayMontagePath(const char* Path, float BlendIn)
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || !Path)
	{
		return false;
	}

	UAnimMontage* Montage = FAnimationManager::Get().LoadMontage(Path);
	if (!Montage)
	{
		return false;
	}

	AnimInstance->PlayMontage(Montage, FName::None, 1.0f, BlendIn);
	return true;
}

void AMusouCharacter::PlayComboStep(int32 Step)
{
	if (Step < 1 || Step > GComboMontageCount)
	{
		return;
	}

	PlayMontagePath(GComboMontages[Step - 1], GComboBlendIn);
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
