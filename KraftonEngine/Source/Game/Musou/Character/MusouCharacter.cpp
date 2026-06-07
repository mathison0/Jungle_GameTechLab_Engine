#include "Game/Musou/Character/MusouCharacter.h"

#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/ComboComponent.h"
#include "Game/Musou/Combat/AnimNotify_MusouAttack.h"
#include "Game/Musou/Combat/AnimNotifyState_ComboWindow.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/World.h"
#include "Component/Input/ActionComponent.h"
#include "Animation/AnimationManager.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Animation/Montage/AnimMontage.h"
#include "Animation/Montage/AnimMontageInstance.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/Sequence/AnimDataModel.h"
#include "Component/Input/InputComponent.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/BoneAttachedStaticMeshComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/Shape/CapsuleComponent.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

// 공격 체인/스펙/스텝 데이터는 전부 Content/Script/Data/attack_data.lua —
// FAttackDataRegistry 가 로드/핫리로드. 여기는 재생 로직만 남는다.

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
	//
	// 몽타주 재생 중 이동 잠금: AddMovementInput 만 차단 — 이동은 root motion 이 전담.
	// 말미 MontageMoveUnlockTail 구간에선 잠금 해제 + 이동 입력으로 조기 blend-out 캔슬
	// (UE 의 BlendOutTriggerTime / recovery cancel 패턴). MoveInputThisFrame 재구축은
	// 잠금과 무관하게 유지 (콤보 전진/분기 시 재조준 입력원).
	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		MoveInputThisFrame = YawOnly.GetForwardVector() * Value;
		if (Value == 0.0f || IsMovementLockedByMontage()) return;
		TryMovementCancelMontage();
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		MoveInputThisFrame += YawOnly.GetRightVector() * Value;
		if (Value == 0.0f || IsMovementLockedByMontage()) return;
		TryMovementCancelMontage();
		AddMovementInput(YawOnly.GetRightVector(), Value);
	});

	// Space = Jump (VK_SPACE = 0x20). Walking 중에만 effective (CharacterMovement::Jump 가 guard).
	// 몽타주 재생 중(공격 스윙 등)엔 점프도 잠금 — 이동 잠금과 동일 정책 (말미 해제 포함).
	InputComponent->AddActionMapping("Jump", 0x20);
	InputComponent->BindAction("Jump", EInputEvent::Pressed, [this]()
	{
		if (IsMovementLockedByMontage()) return;
		TryMovementCancelMontage();
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

	// R = 무쌍기 — 게이지(킬 적립) 가득 시 발동. 진행 중 동작 전부 캔슬하는 최우선 입력.
	InputComponent->AddActionMapping("Ultimate", 'R');
	InputComponent->BindAction("Ultimate", EInputEvent::Pressed, [this]()
	{
		OnUltimatePressed();
	});

	// Shift = 구르기 (VK_SHIFT = 0x10) — 입력 방향으로 회피, 후딜(콤보 윈도우/말미) 캔슬 가능.
	InputComponent->AddActionMapping("Dodge", 0x10);
	InputComponent->BindAction("Dodge", EInputEvent::Pressed, [this]()
	{
		OnDodgePressed();
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

	// launcher 저글 상태 해제 — 착지하면 다음 공중 진입은 일반 점프 공격으로.
	if (bJuggleAirborne && !IsFalling())
	{
		bJuggleAirborne = false;
	}

	// 무쌍기 난무 — 현재 슬롯이 끝났으면 다음 슬롯 자동 재생 / 체인 소진 시 정리.
	UpdateUltimateChain();

	// 구르기 종료 — 몽타주가 끝나면 무적 해제.
	if (bRolling && !IsAnyMontagePlaying())
	{
		EndRoll();
	}

	// 피격 리액션 쿨다운 소모.
	if (HitReactCooldownRemaining > 0.0f)
	{
		HitReactCooldownRemaining -= DeltaTime;
	}

	// 콤보 리셋 — 체인 끊김(윈도우 내 미입력)/완주, 또는 지상 체인 중 낙하(절벽 등).
	// 공중 체인은 낙하가 전제라 낙하 리셋에서 제외 — 몽타주 종료로만 리셋.
	// 입력 콜백(InputComponent 틱)보다 늦게 돌더라도 다음 입력 전에 정리되면 충분.
	if (ComboComponent->IsComboActive())
	{
		const bool bAirChain = (ActiveChainContext == EAttackContext::Airborne
			|| ActiveChainContext == EAttackContext::AirborneJuggle);
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

	// 공중 콤보 행 타임 — 공중 체인 진행 중 중력 감쇠 적용/원복.
	UpdateAirComboHang();
}

void AMusouCharacter::UpdateAirComboHang()
{
	if (!CharacterMovement)
	{
		return;
	}

	// 저글 체인 콤보가 살아 있고, 공중에서 "하강 중"일 때만 — 상승 구간까지 감쇠하면
	// 점프/launcher 의 위쪽 속도가 안 깎여 공격 중 계속 떠오르는 느낌이 난다 (정점 이후만 적용).
	// 일반 점프 공격(Airborne 단발)은 행 타임 없음 — 기존 낙하감 유지.
	const bool bWantHang = ComboComponent
		&& ComboComponent->IsComboActive()
		&& ActiveChainContext == EAttackContext::AirborneJuggle
		&& IsFalling()
		&& CharacterMovement->GetVelocity().Z <= 0.0f;

	if (bWantHang && !bAirComboHangActive)
	{
		const float Scale = FAttackDataRegistry::Get().GetFeedback().AirComboGravityScale;
		SavedGravity = CharacterMovement->Gravity;
		CharacterMovement->Gravity = SavedGravity * Scale;
		bAirComboHangActive = true;
	}
	else if (!bWantHang && bAirComboHangActive)
	{
		CharacterMovement->Gravity = SavedGravity;
		bAirComboHangActive = false;
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
	// 데이터 핫리로드 체크도 여기 — 콤보 진행 중엔 체인이 바뀌지 않는다.
	if (!ComboComponent->IsComboActive())
	{
		FAttackDataRegistry& Data = FAttackDataRegistry::Get();
		Data.EnsureFresh();

		ActiveChainContext = ResolveAttackContext();
		const TArray<FMusouAttackSlot>& Chain = Data.GetLightChain(ActiveChainContext);
		if (Chain.empty())
		{
			return;
		}
		ComboComponent->SetMaxComboSteps(static_cast<int32>(Chain.size()));
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

	FAttackDataRegistry& Data = FAttackDataRegistry::Get();
	Data.EnsureFresh();

	if (const FMusouAttackSlot* Slot = Data.GetHeavySlot(ResolveAttackContext()))
	{
		PlayAttackSlot(*Slot);
	}
}

void AMusouCharacter::OnUltimatePressed()
{
	// 지상에서만, 중복 발동 금지. 진행 중인 콤보/몽타주는 발동 시 전부 캔슬 (최우선 입력).
	if (bUltimateActive || IsFalling())
	{
		return;
	}

	FAttackDataRegistry& Data = FAttackDataRegistry::Get();
	Data.EnsureFresh();

	const TArray<FMusouAttackSlot>& Chain = Data.GetUltimateChain();
	if (Chain.empty())
	{
		return;   // lua 에 chains.ultimate 미정의 — 기능 비활성
	}

	AMusouGameMode* GameMode = GetWorld() ? Cast<AMusouGameMode>(GetWorld()->GetGameMode()) : nullptr;
	AMusouGameState* GameState = GameMode ? GameMode->GetMusouGameState() : nullptr;
	if (!GameState || !GameState->TryConsumeMusouGauge())
	{
		return;   // 게이지 부족
	}

	if (ComboComponent)
	{
		ComboComponent->ResetCombo();
	}
	EndRoll();   // 구르기 중 발동 — 구르기 상태 정리 (무적은 아래서 다시 켠다)

	bUltimateActive = true;
	UltimateStep = 1;   // 0번은 지금 즉시 재생 — Tick 이 1번부터 이어간다

	// 난무 동안 무적 — 군체에 둘러싸여 발동하는 기술이라 피격으로 끊기면 의미가 없다.
	if (BattleComponent)
	{
		BattleComponent->SetInvincible(true);
	}

	// 발동 연출 — 짧은 글로벌 슬로모 + 강셰이크 (킬 버스트 인프라 재사용).
	if (UActionComponent* Action = GetComponentByClass<UActionComponent>())
	{
		Action->Slomo(0.4f, 0.3f);
	}
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
		{
			CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(0.5f);
		}
	}

	// 첫 슬롯 즉시 재생 — 진행 중 몽타주가 있어도 PlayMontage 가 cross-fade 로 교체.
	if (!PlayAttackSlot(Chain[0]))
	{
		EndUltimate();   // 재생 실패 — 무적 고착 방지
	}
}

void AMusouCharacter::UpdateUltimateChain()
{
	if (!bUltimateActive)
	{
		return;
	}

	// 현재 슬롯이 아직 본 재생 중이면 대기. blend-out 에 들어갔으면 다음 슬롯으로
	// cross-fade — 완전히 끝나길 기다리면 슬롯 사이에 idle 포즈가 새어 나온다.
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetMontageInstance() : nullptr;
	if (MontageInstance && MontageInstance->IsActive() && !MontageInstance->IsBlendingOut())
	{
		return;
	}

	const TArray<FMusouAttackSlot>& Chain = FAttackDataRegistry::Get().GetUltimateChain();
	if (UltimateStep >= static_cast<int32>(Chain.size()))
	{
		EndUltimate();
		return;
	}

	const int32 Step = UltimateStep++;
	if (!PlayAttackSlot(Chain[Step]))
	{
		EndUltimate();
	}
}

void AMusouCharacter::EndUltimate()
{
	bUltimateActive = false;
	UltimateStep = 0;

	if (BattleComponent)
	{
		BattleComponent->SetInvincible(false);
	}
}

void AMusouCharacter::OnDodgePressed()
{
	// 지상 전용. 무쌍기 중엔 불가, 구르기 중복 불가.
	if (bUltimateActive || bRolling || IsFalling())
	{
		return;
	}

	// 캔슬 규칙 — 후딜만 캔슬 가능: 몽타주 없음 / 말미 unlock·blend-out / 콤보 윈도우 개방.
	// 스윙 본체(선딜+히트 전)는 캔슬 불가 — 공격 커밋은 유지한다.
	const bool bCanRoll = !IsMovementLockedByMontage()
		|| (ComboComponent && ComboComponent->IsComboWindowOpen());
	if (!bCanRoll)
	{
		return;
	}

	FAttackDataRegistry& Data = FAttackDataRegistry::Get();
	Data.EnsureFresh();

	const FMusouAttackSlot* Dodge = Data.GetDodgeSlot();
	if (!Dodge)
	{
		return;   // lua 에 chains.dodge 미정의 — 기능 비활성
	}

	if (ComboComponent)
	{
		ComboComponent->ResetCombo();
	}

	// PlayAttackSlot 의 회전 스냅이 입력 방향으로 굴러가게 해준다 (입력 없으면 전방).
	if (!PlayAttackSlot(*Dodge))
	{
		return;   // 에셋 미임포트 등 — 로그는 ResolveStepMontage 경로가 남김
	}

	// 전 구간 무적 — 회피 보상. 몽타주 종료 시 Tick 이 해제.
	bRolling = true;
	if (BattleComponent)
	{
		BattleComponent->SetInvincible(true);
	}
}

void AMusouCharacter::EndRoll()
{
	if (!bRolling)
	{
		return;
	}
	bRolling = false;

	// 무쌍기가 무적을 쓰는 중이면 유지 — 그 외엔 해제.
	if (BattleComponent && !bUltimateActive)
	{
		BattleComponent->SetInvincible(false);
	}
}

void AMusouCharacter::PlayHitReaction()
{
	// 모션 우선 — 공격/구르기/무쌍기/공중 중 피격은 휘청 없이 진행 (무쌍식 슈퍼아머 느낌).
	// 몽타주 재생 중 체크가 이들 대부분을 커버하지만 상태 플래그도 명시적으로 가드.
	if (bUltimateActive || bRolling || IsFalling() || IsAnyMontagePlaying())
	{
		return;
	}

	if (HitReactCooldownRemaining > 0.0f)
	{
		return;
	}

	const FMusouAttackSlot* Slot = FAttackDataRegistry::Get().GetHitReactSlot();
	if (!Slot)
	{
		return;   // lua 에 chains.hit_react 미정의 — 기능 비활성
	}

	const FMusouAttackStep* Step = PickVariant(*Slot);
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Step || !AnimInstance)
	{
		return;
	}

	UAnimMontage* Montage = ResolveStepMontage(*Step);
	if (!Montage)
	{
		return;
	}

	// PlayAttackStep 을 안 쓰는 이유: 피격은 입력 방향 회전 스냅이 부적절.
	float PlayRate = Step->PlayRateMin;
	if (Step->PlayRateMax > Step->PlayRateMin)
	{
		const float T = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		PlayRate = Step->PlayRateMin + (Step->PlayRateMax - Step->PlayRateMin) * T;
	}
	AnimInstance->PlayMontage(Montage, FName::None, PlayRate, Step->BlendIn);

	HitReactCooldownRemaining = FAttackDataRegistry::Get().GetFeedback().HitReactCooldown;
}

EAttackContext AMusouCharacter::ResolveAttackContext() const
{
	if (IsFalling())
	{
		// launcher 로 떠올랐으면 저글 체인, 일반 점프/낙하는 단발 점프 공격.
		return bJuggleAirborne ? EAttackContext::AirborneJuggle : EAttackContext::Airborne;
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

bool AMusouCharacter::IsMovementLockedByMontage() const
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontageInstance* MI = AnimInstance ? AnimInstance->GetMontageInstance() : nullptr;
	if (!MI || !MI->IsActive())
	{
		return false;
	}

	// Blend-out 중 = 이미 로코모션으로 복귀 중 — 컨트롤도 같이 복귀.
	if (MI->IsBlendingOut())
	{
		return false;
	}

	// 말미 여유 구간 — 클립이 끝나기 전 미리 컨트롤 반환 (UE BlendOutTriggerTime 대응).
	return MI->GetSectionRemainingTime() > MontageMoveUnlockTail;
}

void AMusouCharacter::TryMovementCancelMontage()
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontageInstance* MI = AnimInstance ? AnimInstance->GetMontageInstance() : nullptr;
	if (!MI || !MI->IsActive() || MI->IsBlendingOut())
	{
		return;
	}

	// 콤보 전진/분기 예약이 살아 있으면 캔슬 보류 — 다음 스텝이 곧 재생되므로 체인 우선.
	if (ComboComponent && ComboComponent->HasQueuedInput())
	{
		return;
	}

	// 몽타주 기본 BlendOutTime 으로 조기 blend-out — 이동 모션과 cross-fade 되며 복귀.
	AnimInstance->StopMontage();
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

const FMusouAttackStep* AMusouCharacter::PickVariant(const FMusouAttackSlot& Slot)
{
	const int32 Num = static_cast<int32>(Slot.Variants.size());
	if (Num <= 0)
	{
		return nullptr;
	}
	if (Num == 1)
	{
		return &Slot.Variants[0];
	}

	int32 Index = std::rand() % Num;

	// 직전 변주 반복 회피 — 같은 슬롯에서 연속으로 같은 모션이 나오면 변주 체감이 죽는다.
	// (key = 슬롯 주소. 데이터 핫리로드로 슬롯이 재구성되면 미스 → 새로 기록.)
	bool bRecorded = false;
	for (auto& Entry : LastVariantPick)
	{
		if (Entry.first == &Slot)
		{
			if (Index == Entry.second)
			{
				Index = (Index + 1) % Num;
			}
			Entry.second = Index;
			bRecorded = true;
			break;
		}
	}
	if (!bRecorded)
	{
		LastVariantPick.push_back({ &Slot, Index });
	}

	return &Slot.Variants[Index];
}

bool AMusouCharacter::PlayAttackSlot(const FMusouAttackSlot& Slot)
{
	const FMusouAttackStep* Step = PickVariant(Slot);
	return Step && PlayAttackStep(*Step);
}

bool AMusouCharacter::PlayAttackStep(const FMusouAttackStep& Step)
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

	// 재생속도 변주 — [Min, Max] 균등 랜덤. notify/RM 은 시퀀스 시간축이라 비율 유지.
	float PlayRate = Step.PlayRateMin;
	if (Step.PlayRateMax > Step.PlayRateMin)
	{
		const float T = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		PlayRate = Step.PlayRateMin + (Step.PlayRateMax - Step.PlayRateMin) * T;
	}

	AnimInstance->PlayMontage(Montage, FName::None, PlayRate, Step.BlendIn);
	return true;
}

UAnimMontage* AMusouCharacter::ResolveStepMontage(const FMusouAttackStep& Step)
{
	FAnimationManager& Manager = FAnimationManager::Get();

	// RM 강제 (lua force_root_motion) — 임포트 직후 RM 꺼진 에셋을 에디터 재저장 없이 사용.
	// (SetEnableRootMotion 이 bForceRootLock 상호 배제도 처리)
	auto ApplyRootMotionOverride = [&Step](UAnimSequence* Sequence)
	{
		if (Step.bForceRootMotion && Sequence && !Sequence->GetEnableRootMotion())
		{
			Sequence->SetEnableRootMotion(true);
		}
	};

	// 1) 런타임 생성 캐시 — fallback 으로 한 번 만든 몽타주 재사용 (실패 로그 반복 방지).
	//    데이터 핫리로드로 버전이 바뀌었으면 주입 notify 만 갱신 (Inject 가 버전 비교).
	if (!Step.SequencePath.empty())
	{
		for (const auto& Entry : RuntimeAttackMontages)
		{
			if (Entry.first == Step.SequencePath)
			{
				ApplyRootMotionOverride(Entry.second->GetSourceSequence());
				InjectDefaultAttackNotifies(Entry.second->GetSourceSequence(), Step);
				return Entry.second;
			}
		}
	}

	// 2) 에디터 저작 몽타주 우선. 단 소스 시퀀스에 notify 가 하나도 저작 안 된 경우
	//    (Horizontal/360 Low 등 모션만 있는 에셋) 는 fallback 과 동일하게 기본 notify 주입 —
	//    InjectDefaultAttackNotifies 가 저작본 존재 시 스스로 스킵하므로 무조건 호출해도 안전.
	if (!Step.MontagePath.empty())
	{
		if (UAnimMontage* Montage = Manager.LoadMontage(Step.MontagePath))
		{
			ApplyRootMotionOverride(Montage->GetSourceSequence());
			InjectDefaultAttackNotifies(Montage->GetSourceSequence(), Step);
			return Montage;
		}
	}

	// 3) Fallback — 시퀀스에서 런타임 생성 + 기본 notify 주입.
	if (Step.SequencePath.empty())
	{
		return nullptr;
	}

	UAnimSequence* Sequence = Manager.LoadAnimation(Step.SequencePath);
	if (!Sequence)
	{
		return nullptr;
	}

	ApplyRootMotionOverride(Sequence);
	InjectDefaultAttackNotifies(Sequence, Step);

	UAnimMontage* Montage = Manager.CreateMontage(Sequence, Sequence->GetName() + "_RuntimeMontage");
	if (!Montage)
	{
		return nullptr;
	}
	Montage->EnsureDefaultSection();

	RuntimeAttackMontages.push_back({ Step.SequencePath, Montage });
	return Montage;
}

void AMusouCharacter::InjectDefaultAttackNotifies(UAnimSequence* Sequence, const FMusouAttackStep& Step)
{
	if (!Sequence)
	{
		return;
	}

	// 저작 notify 존중 — 우리가 주입한 Auto* 외의 notify 가 하나라도 있으면 손대지 않는다.
	static const FName AutoHitName("AutoMusouAttack");
	static const FName AutoWindowName("AutoComboWindow");
	for (const FAnimNotifyEvent& Existing : Sequence->GetNotifies())
	{
		if (Existing.NotifyName != AutoHitName && Existing.NotifyName != AutoWindowName)
		{
			return;
		}
	}

	// 주입 이력 — 같은 데이터 버전이면 그대로 사용, 바뀌었으면 Auto* 걷어내고 재주입.
	const int32 CurVersion = FAttackDataRegistry::Get().GetVersion();
	bool bRecorded = false;
	for (auto& Entry : InjectedSequenceVersions)
	{
		if (Entry.first == Sequence)
		{
			if (Entry.second == CurVersion)
			{
				return;
			}
			Entry.second = CurVersion;
			bRecorded = true;
			break;
		}
	}

	UAnimDataModel* Model = Sequence->GetDataModel();
	const float Length = Sequence->GetPlayLength();
	if (!Model || Length <= 0.0f)
	{
		return;
	}

	if (!bRecorded)
	{
		InjectedSequenceVersions.push_back({ Sequence, CurVersion });
	}

	TArray<FAnimNotifyEvent>& ModelNotifies = Sequence->GetMutableModelNotifies();

	// 핫리로드 재주입 — 기존 Auto* 제거 (위 가드로 이 시점엔 Auto* 만 존재).
	ModelNotifies.erase(
		std::remove_if(ModelNotifies.begin(), ModelNotifies.end(),
			[](const FAnimNotifyEvent& Ev)
			{
				return Ev.NotifyName == AutoHitName || Ev.NotifyName == AutoWindowName;
			}),
		ModelNotifies.end());

	// 히트 판정 — 스윙 임팩트 추정 지점. 정확한 타이밍은 에디터 저작으로 대체 권장.
	if (!Step.AttackId.empty() && Step.HitFrac >= 0.0f)
	{
		UAnimNotify_MusouAttack* Hit = UObjectManager::Get().CreateObject<UAnimNotify_MusouAttack>(Model);
		Hit->AttackId = Step.AttackId;

		FAnimNotifyEvent Event;
		Event.NotifyName  = AutoHitName;
		Event.TriggerTime = Length * Step.HitFrac;
		Event.Notify      = Hit;
		ModelNotifies.push_back(Event);
	}

	// 콤보 윈도우 — 후딜 구간.
	if (Step.WindowBeginFrac >= 0.0f && Step.WindowEndFrac > Step.WindowBeginFrac)
	{
		FAnimNotifyEvent Event;
		Event.NotifyName  = AutoWindowName;
		Event.TriggerTime = Length * Step.WindowBeginFrac;
		Event.Duration    = Length * (Step.WindowEndFrac - Step.WindowBeginFrac);
		Event.NotifyState = UObjectManager::Get().CreateObject<UAnimNotifyState_ComboWindow>(Model);
		ModelNotifies.push_back(Event);
	}

	Sequence->RefreshRuntimeNotifies();
}

void AMusouCharacter::PlayComboStep(int32 Step)
{
	const TArray<FMusouAttackSlot>& Chain = FAttackDataRegistry::Get().GetLightChain(ActiveChainContext);
	if (Step < 1 || Step > static_cast<int32>(Chain.size()))
	{
		return;
	}

	PlayAttackSlot(Chain[Step - 1]);
}

void AMusouCharacter::PlayBranchFinisher(int32 BranchStep)
{
	// 테이블보다 깊은 단수는 마지막 분기로 clamp — Registry 가 처리.
	if (const FMusouAttackSlot* Slot = FAttackDataRegistry::Get().GetBranchFinisher(BranchStep))
	{
		PlayAttackSlot(*Slot);
	}
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
