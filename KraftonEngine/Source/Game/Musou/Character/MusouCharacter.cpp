#include "Game/Musou/Character/MusouCharacter.h"

#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/Combat/ComboComponent.h"
#include "Game/Musou/Combat/AnimNotify_MusouAttack.h"
#include "Game/Musou/Combat/AnimNotify_GroundSlamShockwave.h"
#include "Game/Musou/Combat/AnimNotify_UltimateLeap.h"
#include "Game/Musou/Combat/AnimNotify_UltimateAdvance.h"
#include "Game/Musou/Combat/AnimNotifyState_ComboWindow.h"
#include "Game/Musou/Effect/SlashEffectActor.h"
#include "Game/Musou/Boss/MusouBossCharacter.h"
#include "Game/Crowd/LargeScaleUnitManagerComponent.h"
#include "Game/Crowd/CrowdUnitTypes.h"
#include "Game/Musou/Camera/AnimNotifyState_CameraShot.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Game/Musou/GameMode/MusouGameState.h"
#include "Game/Musou/MusouGameSettings.h"
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
#include "Component/Camera/CameraComponent.h"
#include "Component/Camera/CineCameraComponent.h"
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
	// 연출 카메라는 Super::BeginPlay 의 컴포넌트 BeginPlay 루프 *앞*에 생성 —
	// UCameraComponent::BeginPlay(카메라 매니저 등록) 를 정상 통과시킨다.
	EnsureCinematicCameras();

	Super::BeginPlay();

	if (HitFlashComponent && Mesh)
	{
		HitFlashComponent->InitializeFromSkinnedMesh(Mesh);
	}

	// 시작 무기 상태 동기화 — 기본 납도(등에 멘 상태). 첫 공격/X 입력으로 발도.
	ApplyWeaponState();

	// 전역 설정 — 카메라 연출(몽타주 카메라 샷) 사용 여부 적용.
	// (크레딧 게임모드는 StartMatch 에서 다시 true 로 덮어 고정 시점을 유지한다.)
	SetCameraShotsSuppressed(!FMusouGameSettings::Get().IsCameraDirectionEnabled());
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
	// 공중 저글 콤보 중엔 AddMovementInput 차단(IsInAirCombo) — WASD 공중 제어로
	// 콤보가 옆으로 흐르지 않게. MoveInputThisFrame 재구축은 유지해 스텝 재조준은 가능.
	InputComponent->BindAxis("MoveForward", [this](float Value)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		MoveInputThisFrame = YawOnly.GetForwardVector() * Value;
		if (Value == 0.0f || IsMovementLockedByMontage() || IsInAirCombo()) return;
		TryMovementCancelMontage();
		AddMovementInput(YawOnly.GetForwardVector(), Value);
	});
	InputComponent->BindAxis("MoveRight", [this](float Value)
	{
		const FRotator YawOnly(0.0f, GetControlRotation().Yaw, 0.0f);
		MoveInputThisFrame += YawOnly.GetRightVector() * Value;
		if (Value == 0.0f || IsMovementLockedByMontage() || IsInAirCombo()) return;
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

	// X = 발도/납도 토글 — 납도 상태에선 로코모션이 비무장 세트로 바뀌고 (lua 플래그),
	// 공격 입력이 발도로 변환된다. 무기는 모션 중간에 손↔등으로 옮겨짐.
	InputComponent->AddActionMapping("ToggleWeapon", 'X');
	InputComponent->BindAction("ToggleWeapon", EInputEvent::Pressed, [this]()
	{
		OnToggleWeaponPressed();
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

	// 충격파는 몽타주/콤보 상태와 무관하게 진행 — 항상 먼저 갱신.
	UpdateShockwave(DeltaTime);

	// 궁극기 마무리 슬램 — 내리꽂기가 지면에 닿는 순간 착지 임팩트 1회.
	if (bUltimateLandingPending && !IsFalling())
	{
		bUltimateLandingPending = false;
		TriggerUltimateLandingImpact();
	}

	if (!ComboComponent)
	{
		// 콤보 컴포넌트가 없어도 진행 중 카메라 샷은 유지/복귀해야 한다.
		UpdateCameraShot();
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

	// 발도/납도 본 스왑 — 모션 중간(손이 등에 닿는 타이밍)에 무기 손↔등 이동.
	if (WeaponSwapDelay >= 0.0f)
	{
		WeaponSwapDelay -= DeltaTime;
		if (WeaponSwapDelay < 0.0f)
		{
			bWeaponDrawn = bPendingWeaponDrawn;
			ApplyWeaponState();
		}
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

	// 몽타주 카메라 샷 — look_at/월드 고정 유지 + 종료 안전망.
	// ※ 콤보 전진(SnapFacingToInput 캡슐 yaw 스냅) *이후* 에 갱신해야 look-at 회전이
	//   같은 프레임의 최종 캡슐 yaw 를 반영한다 (스냅 전에 갱신하면 1프레임 회전 튐).
	UpdateCameraShot();
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
	// 납도 상태 — 공격 대신 발도 (발도 후 다시 누르면 콤보 시작).
	if (!bWeaponDrawn)
	{
		OnToggleWeaponPressed();
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
	// 납도 상태 — 공격 대신 발도.
	if (!bWeaponDrawn)
	{
		OnToggleWeaponPressed();
		return;
	}

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
		PlayAttackSlot(*Slot, /*bFaceCameraIfNoInput=*/true);
	}
}

void AMusouCharacter::OnUltimatePressed()
{
	// 중복 발동만 금지 — 공중 콤보 중에도 발동 가능 (좌좌우 → 궁 등). 진행 중인 콤보/몽타주는
	// 발동 시 전부 캔슬 (최우선 입력). 공중에서 발동하면 백플립/강타/충격파 후 슬램 착지로 마무리.
	if (bUltimateActive)
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

	// 공중 콤보 중 발동 — 저글/행 타임 상태 정리. 행 타임은 Gravity 를 직접 줄여 두므로
	// 명시 복원해야 궁극기 leap 의 GravityScale 과 이중 감쇠되지 않는다.
	bJuggleAirborne = false;
	if (CharacterMovement && bAirComboHangActive)
	{
		CharacterMovement->Gravity = SavedGravity;
		bAirComboHangActive = false;
	}

	// 납도 상태 발동 — 즉시 발도 (난무가 모션을 점유하므로 발도 모션은 생략).
	bWeaponDrawn = true;
	WeaponSwapDelay = -1.0f;
	ApplyWeaponState();

	bUltimateActive = true;
	UltimateStep = 1;   // 0번은 지금 즉시 재생 — Tick 이 1번부터 이어간다

	// 난무 동안 무적 — 군체에 둘러싸여 발동하는 기술이라 피격으로 끊기면 의미가 없다.
	if (BattleComponent)
	{
		BattleComponent->SetInvincible(true);
	}

	// 난무 동안 orient-to-movement 차단 — 백플립이 뒤로 launch 되면 PhysOrientToMovement 가
	// 속도(후방) 방향으로 캡슐을 돌려 카메라 쪽으로 휙 도는 증상을 막는다 (facing 은 발동 시점
	// 고정). EndUltimate 에서 복원.
	if (UCharacterMovementComponent* Movement = GetComponentByClass<UCharacterMovementComponent>())
	{
		Movement->bOrientRotationToMovement = false;
	}

	// 발동 연출 — "잠시 멈추고 시퀀스" 비트. 글로벌 슬로모를 거의 정지에 가깝게 깔아
	// 백플립 도입을 시네마틱하게 끌었다가 풀린다 (+ 강셰이크는 킬 버스트 인프라 재사용).
	if (UActionComponent* Action = GetComponentByClass<UActionComponent>())
	{
		Action->Slomo(0.5f, 0.06f);
	}
	if (FMusouGameSettings::Get().IsCameraShakeEnabled())
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
			{
				CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(0.5f);
			}
		}
	}

	// 첫 슬롯 즉시 재생 — 진행 중 몽타주가 있어도 PlayMontage 가 cross-fade 로 교체.
	// (PlayAttackSlot 이 입력/카메라 정면으로 facing 을 먼저 스냅한다.)
	if (!PlayAttackSlot(Chain[0]))
	{
		EndUltimate();   // 재생 실패 — 무적 고착 방지
		return;
	}

	// 충격파 지면 높이 — 발동 시점은 지상이므로 캐릭터 Z 가 지면 기준. 타겟이 있으면 그 높이.
	UltimateWaveZ = GetActorLocation().Z;

	// 락온 — 전방 최근접 적(보스 우선)을 향해 facing 고정. 검기 진행/카메라 사선/강타 방향이
	// 모두 이 facing 을 따른다 (orient-to-movement 는 위에서 껐으므로 궁극기 내내 유지).
	// 적이 없으면 직전 스냅(카메라 정면) 그대로.
	FVector TargetPos;
	if (CapsuleComponent && FindNearestEnemyTarget(TargetPos))
	{
		UltimateWaveZ = TargetPos.Z;   // 타겟(지상 적) 높이에 검기/판정을 깔아 확실히 맞춘다.

		FVector To = TargetPos - GetActorLocation();
		To.Z = 0.0f;
		if (To.X * To.X + To.Y * To.Y > 1e-4f)
		{
			FRotator R = CapsuleComponent->GetRelativeRotation();
			R.Yaw = std::atan2(To.Y, To.X) * (180.0f / 3.14159265f);
			CapsuleComponent->SetRelativeRotation(R);
		}
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
	// 후속 슬롯은 카메라 재조준 끔 — 발동 시점 facing 유지.
	if (!PlayAttackSlot(Chain[Step], /*bFaceCameraIfNoInput=*/false))
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

	// 발동 시 끈 회전/중력 복원 — 백플립 leap 의 약한 중력과 orient-to-movement 차단을 되돌린다.
	if (UCharacterMovementComponent* Movement = GetComponentByClass<UCharacterMovementComponent>())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->SetGravityScale(1.0f);

		// 강한 마무리 착지 — 공중(체공 중)이면 빠르게 내리꽂고, 지면에 닿는 순간 임팩트(Tick).
		if (Movement->IsFalling())
		{
			Movement->LaunchAerial(FVector(0.0f, 0.0f, -18.0f));
			bUltimateLandingPending = true;
		}
	}
}

void AMusouCharacter::AdvanceUltimateNow()
{
	// 백플립 advance notify → 현재 슬롯 종료를 기다리지 않고 다음 슬롯으로 즉시 cross-fade.
	// 이후는 UpdateUltimateChain 이 이어받아 체인 소진 시 정리 (UltimateStep 증가로 중복 방지).
	if (!bUltimateActive)
	{
		return;
	}

	const TArray<FMusouAttackSlot>& Chain = FAttackDataRegistry::Get().GetUltimateChain();
	if (UltimateStep >= static_cast<int32>(Chain.size()))
	{
		return;
	}

	const int32 Step = UltimateStep++;
	// 후속 슬롯은 카메라 재조준 끔 — 발동 시점 facing 을 유지(공중에서 휙 도는 것 방지).
	if (!PlayAttackSlot(Chain[Step], /*bFaceCameraIfNoInput=*/false))
	{
		EndUltimate();
	}
}

bool AMusouCharacter::FindNearestEnemyTarget(FVector& OutPos) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Self = GetActorLocation();
	const FVector Fwd  = GetActorForward();   // 발동 시점 facing(카메라 정면 스냅 직후) = 전방 기준
	constexpr float MaxRangeSq  = 30.0f * 30.0f;   // 락온 최대 거리 (m)
	constexpr float FrontDotMin = -0.25f;          // 약간 뒤까지 허용, 정반대 방향은 제외

	bool   bFound    = false;
	float  BestScore = 3.4e38f;
	FVector Best(0.0f, 0.0f, 0.0f);

	// 거리 - 전방성 보너스 점수로 "가까우면서 정면에 가까운" 적을 고른다.
	auto Consider = [&](const FVector& Pos)
	{
		FVector To = Pos - Self;
		To.Z = 0.0f;
		const float DistSq = To.X * To.X + To.Y * To.Y;
		if (DistSq > MaxRangeSq || DistSq < 1e-4f)
		{
			return;
		}
		const float Dist = std::sqrt(DistSq);
		const float Dot  = (To.X * Fwd.X + To.Y * Fwd.Y) / Dist;
		if (Dot < FrontDotMin)
		{
			return;
		}
		const float Score = Dist - Dot * 3.0f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best      = Pos;
			bFound    = true;
		}
	};

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		// 보스 — 살아있으면 후보 (우선순위는 점수로 자연 처리).
		if (AMusouBossCharacter* Boss = Cast<AMusouBossCharacter>(Actor))
		{
			UBattleComponent* BC = Boss->GetBattleComponent();
			if (!BC || !BC->IsDead())
			{
				Consider(Boss->GetActorLocation());
			}
			continue;
		}

		// 군중 — 유닛 매니저 렌더데이터에서 살아있는 적 유닛.
		if (ULargeScaleUnitManagerComponent* Mgr = Actor->GetComponentByClass<ULargeScaleUnitManagerComponent>())
		{
			for (const FUnitRenderData& U : Mgr->GetRenderData())
			{
				if (U.Team != EUnitTeam::Enemy || U.State == EUnitState::Dead)
				{
					continue;
				}
				Consider(U.Position);
			}
		}
	}

	if (bFound)
	{
		OutPos = Best;
	}
	return bFound;
}

void AMusouCharacter::LaunchBackflip(float BackSpeed, float UpSpeed, float GravityScale)
{
	// 제자리 백플립을 후방+상방 임펄스로 실제로 빼준다 (백플립 leap notify).
	// 약한 중력으로 체공을 늘려 다음 강타 몽타주가 공중에서 돌게 한다 (EndUltimate 에서 1.0 복원).
	if (UCharacterMovementComponent* Movement = GetComponentByClass<UCharacterMovementComponent>())
	{
		const FVector Back = GetActorForward() * (-BackSpeed);
		Movement->LaunchAerial(FVector(Back.X, Back.Y, UpSpeed));
		Movement->SetGravityScale(GravityScale);
	}
}

void AMusouCharacter::TriggerUltimateLandingImpact()
{
	UWorld* World = GetWorld();
	const FVector Origin = GetActorLocation();

	// 임팩트 — 짧은 히트스톱 + 강셰이크로 "쾅" 하고 내리꽂는 무게감.
	if (UActionComponent* Action = GetComponentByClass<UActionComponent>())
	{
		Action->HitStop(0.07f, 0.0f);
	}
	if (FMusouGameSettings::Get().IsCameraShakeEnabled())
	{
		if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (APlayerCameraManager* CamMgr = PC->GetPlayerCameraManager())
			{
				CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(0.6f);
			}
		}
	}

	// 방사형 검기 링 — 착지 지점에서 사방으로 퍼지는 충격 (placeholder).
	if (World)
	{
		constexpr int32 RingCount = 8;
		for (int32 i = 0; i < RingCount; ++i)
		{
			const float Ang = (2.0f * 3.14159265f) * (static_cast<float>(i) / RingCount);
			const FVector Dir(std::cos(Ang), std::sin(Ang), 0.0f);
			ASlashEffectActor* Slash = World->SpawnActor<ASlashEffectActor>();
			if (!Slash)
			{
				continue;
			}
			const float YawDeg = Ang * (180.0f / 3.14159265f);
			Slash->SetDestroyOnFinish(true);
			Slash->InitDefaultComponents();
			Slash->SetMotion(7.0f, 0.35f);
			Slash->ActivateSlash(Origin + FVector(0.0f, 0.0f, 0.4f), FVector(0.0f, YawDeg + 90.0f, 0.0f), Dir);
		}
	}

	// 착지 AOE — 주변 적에게 슬램 판정 한 방.
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	if (GameMode)
	{
		if (const FAttackSpec* Spec = FindMusouAttackSpec(FName("musou_slam")))
		{
			const float AttackPower = BattleComponent ? BattleComponent->GetAttackPower() : 10.0f;
			FMusouAttackEvent Event;
			Event.Attacker    = this;
			Event.Spec        = *Spec;
			Event.Origin      = Origin;
			Event.Forward     = GetActorForward();
			Event.Damage      = AttackPower * Spec->DamageMult;
			Event.bFromPlayer = IsPossessed();
			GameMode->BroadcastAttack(Event);
		}
	}
}

void AMusouCharacter::StartGroundSlamShockwave(const FVector& Origin, const FVector& Dir, float Distance,
	float Duration, int32 Pulses, FName SpecId, float SlashSpeed, float SlashLife, float SlashYaw)
{
	FVector D = Dir;
	D.Z = 0.0f;
	const float Len = std::sqrt(D.X * D.X + D.Y * D.Y);

	ShockwaveRun.bActive    = true;
	// XY 는 강타 위치(공중일 수 있음), Z 는 발동 시 캡처한 지면/타겟 높이 — 검기/판정이 지상에 깔린다.
	ShockwaveRun.StartPos   = FVector(Origin.X, Origin.Y, UltimateWaveZ);
	ShockwaveRun.Dir        = (Len > 0.001f) ? (D * (1.0f / Len)) : FVector(1.0f, 0.0f, 0.0f);
	ShockwaveRun.Distance   = Distance;
	ShockwaveRun.Duration   = (Duration > 0.01f) ? Duration : 0.01f;
	ShockwaveRun.Elapsed    = 0.0f;
	ShockwaveRun.Pulses     = (Pulses < 1) ? 1 : Pulses;
	ShockwaveRun.NextPulse  = 0;
	ShockwaveRun.SpecId     = SpecId;
	ShockwaveRun.SlashSpeed = SlashSpeed;
	ShockwaveRun.SlashLife  = SlashLife;
	ShockwaveRun.SlashYaw   = SlashYaw;

	// 첫 펄스(시작점)는 강타와 동시에 즉시 발사.
	UpdateShockwave(0.0f);
}

void AMusouCharacter::UpdateShockwave(float DeltaTime)
{
	if (!ShockwaveRun.bActive)
	{
		return;
	}

	ShockwaveRun.Elapsed += DeltaTime;

	// 펄스 i 는 t_i = Duration * i/(N-1) 시점, origin = Start + Dir*Distance*(i/(N-1)) 에서 발사.
	// Elapsed 가 지난 펄스를 모두 따라잡아 발사 — 프레임 드랍에도 빠짐 없이 순차 진행.
	const int32 N = ShockwaveRun.Pulses;
	const float Denom = (N > 1) ? static_cast<float>(N - 1) : 1.0f;
	while (ShockwaveRun.NextPulse < N)
	{
		const float Frac = (N > 1) ? (static_cast<float>(ShockwaveRun.NextPulse) / Denom) : 0.0f;
		const float TPulse = ShockwaveRun.Duration * Frac;
		if (ShockwaveRun.Elapsed + 1e-4f < TPulse)
		{
			break; // 아직 이 펄스 시간 전 — 다음 Tick 에서.
		}

		const FVector PulseOrigin = ShockwaveRun.StartPos + ShockwaveRun.Dir * (ShockwaveRun.Distance * Frac);
		EmitShockwavePulse(PulseOrigin, ShockwaveRun.Dir);
		++ShockwaveRun.NextPulse;
	}

	if (ShockwaveRun.NextPulse >= N && ShockwaveRun.Elapsed >= ShockwaveRun.Duration)
	{
		ShockwaveRun.bActive = false;
	}
}

void AMusouCharacter::EmitShockwavePulse(const FVector& WorldOrigin, const FVector& Dir)
{
	UWorld* World = GetWorld();

	// 데미지 — spec 기하를 그대로 쓰되 origin 만 전방으로 전진시켜 broadcast.
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	if (GameMode)
	{
		const FAttackSpec* Spec = FindMusouAttackSpec(ShockwaveRun.SpecId);
		if (Spec)
		{
			float AttackPower = BattleComponent ? BattleComponent->GetAttackPower() : 10.0f;

			FMusouAttackEvent Event;
			Event.Attacker    = this;
			Event.Spec        = *Spec;
			Event.Origin      = WorldOrigin;
			Event.Forward     = Dir;
			Event.Damage      = AttackPower * Spec->DamageMult;
			Event.bFromPlayer = IsPossessed();
			GameMode->BroadcastAttack(Event);
		}
	}

	// 검기(placeholder) — 펄스 위치에서 전방으로 날아가는 슬래시. 실제 충격파 파티클로 교체 예정.
	if (World)
	{
		ASlashEffectActor* Slash = World->SpawnActor<ASlashEffectActor>();
		if (Slash)
		{
			// 시각 회전 — 진행 yaw + SlashYaw 보정 (기존 검기 규칙: GetActorRotation + (0,90,0)).
			// 진행 방향 Dir 은 전방 그대로. 메시가 틀어져 보이면 lua slash_yaw 로 조정.
			const float YawDeg = std::atan2(Dir.Y, Dir.X) * (180.0f / 3.14159265f);
			Slash->SetDestroyOnFinish(true);
			Slash->InitDefaultComponents();
			Slash->SetMotion(ShockwaveRun.SlashSpeed, ShockwaveRun.SlashLife);
			Slash->ActivateSlash(WorldOrigin + FVector(0.0f, 0.0f, 0.5f),
				FVector(0.0f, YawDeg + ShockwaveRun.SlashYaw, 0.0f), Dir);
		}
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

void AMusouCharacter::OnToggleWeaponPressed()
{
	// 다른 동작 중엔 토글 금지 — 진행 중 몽타주(공격/구르기/발도납도 자신 포함) 우선.
	if (bUltimateActive || bRolling || IsAnyMontagePlaying())
	{
		return;
	}

	FAttackDataRegistry& Data = FAttackDataRegistry::Get();
	Data.EnsureFresh();

	bPendingWeaponDrawn = !bWeaponDrawn;

	// 발도/납도는 UpperBody 슬롯으로 — 상반신이 칼을 뽑/넣는 동안 하반신은 로코모션 유지.
	// (이동 잠금은 DefaultSlot 기준이라 자동으로 안 걸리고, IsAnyMontagePlaying 이 UpperBody
	//  도 보므로 모션 중 재토글/공격만 막힌다.)
	const FMusouAttackSlot* Slot = bWeaponDrawn ? Data.GetWeaponSheatheSlot() : Data.GetWeaponDrawSlot();
	if (!Slot || !PlayAttackSlot(*Slot, /*bFaceCameraIfNoInput=*/false, FName("UpperBody")))
	{
		// 모션 미정의/재생 실패 — 즉시 스왑 폴백.
		bWeaponDrawn = bPendingWeaponDrawn;
		WeaponSwapDelay = -1.0f;
		ApplyWeaponState();
		return;
	}

	// 본 스왑은 모션 중간 (손이 등에 닿는 타이밍) — 남은 재생 시간 × swap_frac 뒤에 적용.
	// 발도납도는 UpperBody 슬롯이므로 그 슬롯의 instance 에서 남은 시간을 읽는다.
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontageInstance* MontageInstance = AnimInstance ? AnimInstance->GetMontageInstanceForSlot(FName("UpperBody")) : nullptr;
	const float Remaining = MontageInstance ? MontageInstance->GetSectionRemainingTime() : 0.0f;
	WeaponSwapDelay = Remaining * Data.GetFeedback().WeaponSwapFrac;
}

void AMusouCharacter::ApplyWeaponState()
{
	if (WeaponComponent)
	{
		WeaponComponent->SetSheathed(!bWeaponDrawn);
	}

	// lua 로코모션 분기용 플래그 — player_anim.lua 가 Anim.get_flag("WeaponDrawn") 으로 읽는다.
	if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(Mesh ? Mesh->GetAnimInstance() : nullptr))
	{
		LuaAnim->SetAnimFlag("WeaponDrawn", bWeaponDrawn);
	}
}

// ─────────────────────────────────────────────────────────────────
// 몽타주 카메라 샷 — AnimNotifyState_CameraShot 구동부
// ─────────────────────────────────────────────────────────────────
APlayerCameraManager* AMusouCharacter::GetLocalCameraManager() const
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPlayerCameraManager() : nullptr;
}

void AMusouCharacter::EnsureCinematicCameras()
{
	// 씬 직렬화 대상이 아님 — 매 플레이 런타임 생성. 캡슐에 붙여 캐릭터 위치/yaw 를
	// 따라간다 (오프셋이 "바라보는 방향 기준" 으로 동작).
	if (!CapsuleComponent)
	{
		return;
	}

	auto MakeCam = [this]() -> UCineCameraComponent*
	{
		UCineCameraComponent* Cam = AddComponent<UCineCameraComponent>();
		Cam->AttachToComponent(CapsuleComponent);
		return Cam;
	};

	if (!CineCamA) { CineCamA = MakeCam(); }
	if (!CineCamB) { CineCamB = MakeCam(); }
}

void AMusouCharacter::StartCameraShot(const FMusouCameraShot& Shot, const void* Token)
{
	// 크레딧 아웃트로 등 고정 시점 — 몽타주 카메라 샷 전환을 통째로 무시.
	if (bSuppressCameraShots)
	{
		return;
	}

	APlayerCameraManager* CamMgr = GetLocalCameraManager();
	UCameraComponent* MainCam = GetCamera();
	if (!CamMgr || !MainCam || !CineCamA || !CineCamB)
	{
		return;
	}

	// 핑퐁 — 직전 샷 카메라는 블렌드 source 일 수 있으므로 반대쪽을 쓴다.
	UCineCameraComponent* Cam = (ActiveShotCam == CineCamA) ? CineCamB : CineCamA;

	ActiveShot      = Shot;
	ActiveShotCam   = Cam;
	ActiveShotToken = Token;

	// 프레이밍 yaw 를 시작 시점에 동결. 시네캠은 캡슐 자식이지만 위치/시선을 이 동결
	// yaw 로 매 프레임 월드 구동(AimShotCamera)해, 공격 중 캡슐 yaw 스냅과 디커플 → 튐 제거.
	// 앵커: 기본은 카메라 뷰(ControlRotation) 기준 — 캐릭터가 옆을 봐도 화면 기준 일관 +
	//   메인캠과 가까워 블렌드 호가 작아 튐 감소. anchor="character" 면 캐릭터 facing 기준.
	ShotYaw = Shot.bCameraRelative ? GetControlRotation().Yaw : GetActorRotation().Yaw;

	Cam->SetFOV(Shot.FOVRad > 0.0f ? Shot.FOVRad : MainCam->GetFOV());

	// 레터박스 — ActiveCamera 로 swap 완료된 시점부터 렌더러가 읽는다.
	Cam->SetLetterboxEnabled(Shot.Letterbox > 0.0f);
	if (Shot.Letterbox > 0.0f)
	{
		Cam->SetLetterboxAmount(1.0f);
		Cam->SetLetterboxThickness(Shot.Letterbox);
	}

	// 샷 동안 마우스 룩 동결 — ControlRotation 이 누적되면 블렌드로 메인 복귀할 때
	// 샷 중 돌린 각도가 그대로 반영돼 카메라가 튄다. 핑퐁 연속 컷에서도 토큰 불일치
	// End 는 복귀를 안 걸어 체인 내내 false 유지 (마지막 End/안전망에서만 복원).
	bAutoInputMouseLook = false;

	if (!Shot.bFollow)
	{
		// 월드 고정 샷 — 동결 yaw 기준 시작 위치를 캡처. 이후 AimShotCamera 가 매 틱 되돌린다.
		const FVector YawOffset = FRotator(0.0f, ShotYaw, 0.0f).ToQuaternion().RotateVector(Shot.Offset);
		ShotWorldLock = GetActorLocation() + YawOffset;
	}

	// 위치/시선 1프레임 선적용 — 블렌드 목적지 POV 가 처음부터 정답이도록.
	AimShotCamera();

	if (!CamMgr->SetActiveCameraWithBlend(
		Cam,
		Shot.BlendIn,
		EViewTargetBlendFunction::VTBlend_EaseInOut,
		ECameraRequestPriority::Attack))
	{
		ActiveShotToken = nullptr;
		ActiveShotCam = nullptr;
		bAutoInputMouseLook = true;
	}
}

void AMusouCharacter::EndCameraShot(const void* Token)
{
	// 이미 끝났거나, 같은 몽타주의 뒷 샷이 인수한 경우 (token 불일치) 복귀를 걸지 않는다.
	if (!ActiveShotToken || Token != ActiveShotToken)
	{
		return;
	}
	ActiveShotToken = nullptr;
	// ActiveShotCam 은 유지 — 직후 새 샷이 시작될 때 핑퐁 판단에 쓰인다.

	// 마우스 룩 복원 — 이 시점의 ControlRotation(샷 동안 동결돼 시작 시점 그대로)으로
	// 메인 카메라가 블렌드 복귀하므로, 복귀 후 카메라 각도가 샷 직전과 동일하다.
	bAutoInputMouseLook = true;

	APlayerCameraManager* CamMgr = GetLocalCameraManager();
	UCameraComponent* MainCam = GetCamera();
	if (CamMgr && MainCam)
	{
		CamMgr->SetActiveCameraWithBlend(
			MainCam,
			ActiveShot.BlendOut,
			EViewTargetBlendFunction::VTBlend_EaseInOut,
			ECameraRequestPriority::Attack);
	}
}

void AMusouCharacter::UpdateCameraShot()
{
	if (!ActiveShotToken)
	{
		return;
	}

	// 안전망 — NotifyEnd 를 거치지 않는 예외 경로(몽타주 인스턴스 즉사 등)에서도 복귀.
	if (!IsAnyMontagePlaying())
	{
		EndCameraShot(ActiveShotToken);
		return;
	}

	AimShotCamera();
}

void AMusouCharacter::AimShotCamera()
{
	UCineCameraComponent* Cam = ActiveShotCam;
	if (!Cam || !ActiveShotToken)
	{
		return;
	}

	constexpr float Rad2Deg = 180.0f / 3.14159265f;

	// ── 위치 — 동결 ShotYaw 기준 offset 을 월드로 구동. 캡슐 yaw 스냅과 무관(튐 제거).
	//   bFollow : 캐릭터 위치 + 동결 yaw offset 을 매 틱 추적.
	//   !bFollow: 시작 시점 캡처한 ShotWorldLock 으로 고정 (부모 이동 상쇄).
	FVector WorldPos;
	if (ActiveShot.bFollow)
	{
		const FVector YawOffset = FRotator(0.0f, ShotYaw, 0.0f).ToQuaternion().RotateVector(ActiveShot.Offset);
		WorldPos = GetActorLocation() + YawOffset;
	}
	else
	{
		WorldPos = ShotWorldLock;
	}
	Cam->SetWorldLocation(WorldPos);

	// ── 회전 — SetRelativeRotation 은 부모(캡슐) 기준이므로 액터 yaw 를 빼 월드 기준으로 환산.
	//   덕분에 캡슐 yaw 가 스냅돼도 (parentYaw + Rel) 가 의도한 월드 각도로 유지된다.
	if (ActiveShot.bLookAt)
	{
		// 기본은 캐릭터를 바라본다. LookAhead>0 이면 동결 ShotYaw 전방 그 거리만큼 앞 지점을
		// 바라본다 — 궁극기 검기가 전방으로 날아가는 사선을 화면에 담기 위해.
		FVector Target = GetActorLocation() + FVector(0.0f, 0.0f, ActiveShot.LookAtHeight);
		if (ActiveShot.LookAhead > 0.0f)
		{
			const FVector AheadDir = FRotator(0.0f, ShotYaw, 0.0f).ToQuaternion().RotateVector(FVector(1.0f, 0.0f, 0.0f));
			Target += AheadDir * ActiveShot.LookAhead;
			// 궁극기 중엔 캐릭터가 공중에 떠 있어도 검기는 지면(UltimateWaveZ)에 깔린다 —
			// 시선 높이를 지면 기준으로 내려 전방 사선을 화면에 담는다.
			if (bUltimateActive)
			{
				Target.Z = UltimateWaveZ + ActiveShot.LookAtHeight;
			}
		}
		const FVector Diff   = Target - WorldPos;
		const float Len = std::sqrt(Diff.X * Diff.X + Diff.Y * Diff.Y + Diff.Z * Diff.Z);
		if (Len < 0.01f)
		{
			return;
		}
		const FVector Dir = Diff * (1.0f / Len);
		FRotator Rel = FRotator::ZeroRotator;
		Rel.Pitch = -std::asin(Dir.Z) * Rad2Deg;                  // UCameraComponent::LookAt 과 동일 부호 규약
		if (std::fabs(Dir.Z) < 0.999f)
		{
			Rel.Yaw = std::atan2(Dir.Y, Dir.X) * Rad2Deg - GetActorRotation().Yaw;
		}
		Cam->SetRelativeRotation(Rel);
	}
	else
	{
		// 고정 시선 — 월드 회전 = 동결 ShotYaw + 저작 Rotation. 부모 yaw 차감으로 환산.
		FRotator Rel = ActiveShot.Rotation;
		Rel.Yaw = ShotYaw + ActiveShot.Rotation.Yaw - GetActorRotation().Yaw;
		Cam->SetRelativeRotation(Rel);
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

void AMusouCharacter::PlayDeathAnimation()
{
	if (bDead)
	{
		return;   // 1회만
	}
	bDead = true;

	// 진행 중 상태 정리 — 충격파/무쌍기 잔여가 사망 연출에 끼어들지 않게.
	ShockwaveRun.bActive = false;
	bUltimateActive = false;

	const FMusouAttackSlot* Slot = FAttackDataRegistry::Get().GetDeathSlot();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Slot || !AnimInstance)
	{
		return;   // lua 에 chains.death 미정의 — 연출 없이 사망 (게임 흐름은 GameMode 가 처리)
	}

	const FMusouAttackStep* Step = PickVariant(*Slot);
	if (!Step)
	{
		return;
	}

	UAnimMontage* Montage = ResolveStepMontage(*Step);
	if (!Montage)
	{
		return;
	}

	// 쓰러진 마지막 포즈 유지 — 끝에서 idle 로 복귀하면 안 되므로 blend-out 을 아주 길게.
	// (몽타주는 blend-out 중 SectionTime 을 끝 프레임에 clamp → 쓰러진 포즈가 그대로 남는다.)
	Montage->SetBlendOutTime(600.0f);
	AnimInstance->PlayMontage(Montage, FName::None, 1.0f, Step->BlendIn);
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

void AMusouCharacter::SnapFacingToInput(bool bDefaultToCameraForward)
{
	if (!CapsuleComponent)
	{
		return;
	}

	const FVector& Input = MoveInputThisFrame;
	const float LenSq2D = Input.X * Input.X + Input.Y * Input.Y;

	float TargetYaw;
	if (LenSq2D >= 1e-4f)
	{
		// PhysOrientToMovement 와 동일한 yaw 좌표계 — atan2(Y, X), +X 가 0°.
		// WASD 입력은 ControlRotation.Yaw(카메라) 기준이라 이미 카메라 상대 방향.
		TargetYaw = std::atan2(Input.Y, Input.X) * (180.0f / 3.14159265f);
	}
	else if (bDefaultToCameraForward && !bSuppressCameraFacing)
	{
		// 공격 시 입력 없으면 카메라 정면으로 재조준 — "공격하면 카메라 쪽을 본다".
		// (크레딧 자동 전투처럼 bSuppressCameraFacing 면 이 재조준을 끄고 현재 facing 유지.)
		TargetYaw = GetControlRotation().Yaw;
	}
	else
	{
		// 그 외(구르기/발도납도)는 입력 없으면 현재 facing 유지. (RM 전진이 새 facing
		// 방향으로 나가고, PhysOrientToMovement 도 같은 방향 속도를 따라간다.)
		return;
	}

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

bool AMusouCharacter::PlayAttackSlot(const FMusouAttackSlot& Slot, bool bFaceCameraIfNoInput, FName SlotName)
{
	const FMusouAttackStep* Step = PickVariant(Slot);
	return Step && PlayAttackStep(*Step, bFaceCameraIfNoInput, SlotName);
}

bool AMusouCharacter::PlayAttackStep(const FMusouAttackStep& Step, bool bFaceCameraIfNoInput, FName SlotName)
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
	// 공격은 입력 없을 때 카메라 정면으로 재조준(bFaceCameraIfNoInput).
	SnapFacingToInput(bFaceCameraIfNoInput);

	// 재생속도 변주 — [Min, Max] 균등 랜덤. notify/RM 은 시퀀스 시간축이라 비율 유지.
	float PlayRate = Step.PlayRateMin;
	if (Step.PlayRateMax > Step.PlayRateMin)
	{
		const float T = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		PlayRate = Step.PlayRateMin + (Step.PlayRateMax - Step.PlayRateMin) * T;
	}

	// SlotName=None → DefaultSlot(풀바디). "UpperBody" → 상반신만 (하체는 로코모션 유지).
	AnimInstance->PlayMontage(Montage, FName::None, PlayRate, Step.BlendIn, SlotName);

	// 궁극기 강타 제자리 고정 — 백플립 leap 의 잔여 후방 속도로 강타가 뒤로 밀리는 것을 막는다.
	// 속도 0 + 중력 0 으로 공중에 못 박고, EndUltimate 가 중력을 복원한다.
	if (bUltimateActive && Step.bPlantInAir)
	{
		if (UCharacterMovementComponent* Movement = GetComponentByClass<UCharacterMovementComponent>())
		{
			Movement->SetVelocity(FVector(0.0f, 0.0f, 0.0f));
			Movement->SetGravityScale(0.0f);
		}
	}

	// 마지막 재생 기술 기록 — 튜토리얼이 카운터 증가 + AttackId 일치로 기술 발동을 감지.
	LastPlayedAttackId = Step.AttackId;
	++AttackPlayCounter;
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
				InjectCameraShotNotifies(Entry.second->GetSourceSequence(), Step);
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
			InjectCameraShotNotifies(Montage->GetSourceSequence(), Step);
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
	InjectCameraShotNotifies(Sequence, Step);

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
	// (AutoCameraShot 은 별도 패스 InjectCameraShotNotifies 의 주입물 — 저작본 취급 금지)
	static const FName AutoHitName("AutoMusouAttack");
	static const FName AutoWindowName("AutoComboWindow");
	static const FName AutoShotName("AutoCameraShot");
	static const FName AutoSlamName("AutoGroundSlam");
	static const FName AutoLeapName("AutoUltimateLeap");
	static const FName AutoAdvanceName("AutoUltimateAdvance");
	for (const FAnimNotifyEvent& Existing : Sequence->GetNotifies())
	{
		if (Existing.NotifyName != AutoHitName && Existing.NotifyName != AutoWindowName
			&& Existing.NotifyName != AutoShotName && Existing.NotifyName != AutoSlamName
			&& Existing.NotifyName != AutoLeapName && Existing.NotifyName != AutoAdvanceName)
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
				return Ev.NotifyName == AutoHitName || Ev.NotifyName == AutoWindowName
					|| Ev.NotifyName == AutoSlamName || Ev.NotifyName == AutoLeapName
					|| Ev.NotifyName == AutoAdvanceName;
			}),
		ModelNotifies.end());

	// 궁극기 백플립 도약 — lua leap 블록 → trigger_frac 에 주입.
	if (Step.Leap.IsValid())
	{
		const FMusouLeap& Lp = Step.Leap;
		UAnimNotify_UltimateLeap* Leap = UObjectManager::Get().CreateObject<UAnimNotify_UltimateLeap>(Model);
		Leap->BackSpeed    = Lp.Back;
		Leap->UpSpeed      = Lp.Up;
		Leap->GravityScale = Lp.Gravity;

		FAnimNotifyEvent Event;
		Event.NotifyName  = AutoLeapName;
		Event.TriggerTime = Length * Lp.TriggerFrac;
		Event.Notify      = Leap;
		ModelNotifies.push_back(Event);
	}

	// 궁극기 다음 슬롯 조기 전환 — lua advance 블록 → trigger_frac 에 주입.
	if (Step.Advance.IsValid())
	{
		UAnimNotify_UltimateAdvance* Advance = UObjectManager::Get().CreateObject<UAnimNotify_UltimateAdvance>(Model);

		FAnimNotifyEvent Event;
		Event.NotifyName  = AutoAdvanceName;
		Event.TriggerTime = Length * Step.Advance.TriggerFrac;
		Event.Notify      = Advance;
		ModelNotifies.push_back(Event);
	}

	// 전방 진행 충격파 — 궁극기 지면 강타. lua shockwave 블록 → trigger_frac 에 주입.
	if (Step.Shockwave.IsValid())
	{
		const FMusouShockwave& Sw = Step.Shockwave;
		UAnimNotify_GroundSlamShockwave* Slam = UObjectManager::Get().CreateObject<UAnimNotify_GroundSlamShockwave>(Model);
		Slam->Distance   = Sw.Distance;
		Slam->Duration   = Sw.Duration;
		Slam->Pulses     = Sw.Pulses;
		Slam->AttackId   = Sw.AttackId.empty() ? Step.AttackId : Sw.AttackId;
		Slam->SlashSpeed = Sw.SlashSpeed;
		Slam->SlashLife  = Sw.SlashLife;
		Slam->SlashYaw   = Sw.SlashYaw;

		FAnimNotifyEvent Event;
		Event.NotifyName  = AutoSlamName;
		Event.TriggerTime = Length * Sw.TriggerFrac;
		Event.Notify      = Slam;
		ModelNotifies.push_back(Event);
	}

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

void AMusouCharacter::InjectCameraShotNotifies(UAnimSequence* Sequence, const FMusouAttackStep& Step)
{
	if (!Sequence)
	{
		return;
	}

	static const FName AutoShotName("AutoCameraShot");

	// 저작 카메라 샷 존중 — Auto 가 아닌 CameraShot notify 가 하나라도 있으면 양보.
	// (공격 notify 주입과 가드가 다른 이유: 히트/윈도우가 저작된 시퀀스에도 카메라
	//  연출은 lua 로 얹고 싶다 — 같은 종류의 저작물이 있을 때만 물러난다.)
	for (const FAnimNotifyEvent& Existing : Sequence->GetNotifies())
	{
		if (Existing.NotifyState && Cast<UAnimNotifyState_CameraShot>(Existing.NotifyState)
			&& Existing.NotifyName != AutoShotName)
		{
			return;
		}
	}

	// 주입 이력 — 같은 데이터 버전이면 그대로, 바뀌었으면 AutoCameraShot 걷어내고 재주입.
	const int32 CurVersion = FAttackDataRegistry::Get().GetVersion();
	bool bRecorded = false;
	for (auto& Entry : CameraShotInjectedVersions)
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
		CameraShotInjectedVersions.push_back({ Sequence, CurVersion });
	}

	TArray<FAnimNotifyEvent>& ModelNotifies = Sequence->GetMutableModelNotifies();
	ModelNotifies.erase(
		std::remove_if(ModelNotifies.begin(), ModelNotifies.end(),
			[](const FAnimNotifyEvent& Ev) { return Ev.NotifyName == AutoShotName; }),
		ModelNotifies.end());

	for (const FMusouCameraShot& Shot : Step.CameraShots)
	{
		if (!Shot.IsValid())
		{
			continue;
		}

		UAnimNotifyState_CameraShot* State = UObjectManager::Get().CreateObject<UAnimNotifyState_CameraShot>(Model);
		State->BlendIn      = Shot.BlendIn;
		State->BlendOut     = Shot.BlendOut;
		State->Offset       = Shot.Offset;
		State->Rotation     = Shot.Rotation;
		State->FOV          = Shot.FOVRad;
		State->bLookAt         = Shot.bLookAt;
		State->LookAtHeight    = Shot.LookAtHeight;
		State->LookAhead       = Shot.LookAhead;
		State->bFollow         = Shot.bFollow;
		State->Letterbox       = Shot.Letterbox;
		State->bCameraRelative = Shot.bCameraRelative;

		FAnimNotifyEvent Event;
		Event.NotifyName  = AutoShotName;
		Event.TriggerTime = Length * Shot.BeginFrac;
		Event.Duration    = Length * (Shot.EndFrac - Shot.BeginFrac);
		Event.NotifyState = State;
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

	// 공격 — 입력 없으면 카메라 정면으로 재조준.
	PlayAttackSlot(Chain[Step - 1], /*bFaceCameraIfNoInput=*/true);
}

void AMusouCharacter::PlayBranchFinisher(int32 BranchStep)
{
	// 테이블보다 깊은 단수는 마지막 분기로 clamp — Registry 가 처리.
	if (const FMusouAttackSlot* Slot = FAttackDataRegistry::Get().GetBranchFinisher(BranchStep))
	{
		PlayAttackSlot(*Slot, /*bFaceCameraIfNoInput=*/true);
	}
}

bool AMusouCharacter::IsAnyMontagePlaying() const
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}
	// DefaultSlot(공격/구르기) 또는 UpperBody(발도/납도) 어느 쪽이든 재생 중이면 busy.
	// 이동 잠금(IsMovementLockedByMontage)은 DefaultSlot 만 보므로 발도납도 중 이동은 허용.
	return AnimInstance->IsMontagePlaying()
		|| AnimInstance->IsMontagePlaying(nullptr, FName("UpperBody"));
}

bool AMusouCharacter::IsFalling() const
{
	return CharacterMovement && CharacterMovement->IsFalling();
}

float AMusouCharacter::GetPlanarSpeed() const
{
	if (!CharacterMovement)
	{
		return 0.0f;
	}
	const FVector V = CharacterMovement->GetVelocity();
	return std::sqrt(V.X * V.X + V.Y * V.Y);
}

bool AMusouCharacter::IsInAirCombo() const
{
	// 저글(launcher) 상태이면서 공중일 때 = 공중 콤보 전 구간. 착지 시 bJuggleAirborne
	// 가 해제되고 IsFalling 도 false 라 자연히 이동이 다시 허용된다.
	return bJuggleAirborne && IsFalling();
}
