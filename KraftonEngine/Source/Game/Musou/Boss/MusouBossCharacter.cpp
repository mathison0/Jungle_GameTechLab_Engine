#include "Game/Musou/Boss/MusouBossCharacter.h"

#include "Animation/AnimationMode.h"
#include "Animation/Instance/LuaAnimInstance.h"
#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Musou/Boss/BossPatternComponent.h"
#include "Game/Musou/Boss/BossPatternDataRegistry.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Mesh/MeshManager.h"
#include "Runtime/Engine.h"

void AMusouBossCharacter::InitDefaultComponents()
{
	InitDefaultComponents(FString(DefaultMeshPath));
}

void AMusouBossCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	ALuaCharacter::InitDefaultComponents(SkeletalMeshFileName, FString(DefaultBossScript));

	bAutoInputMouseLook = false;
	bUseControllerRotationYaw = false;

	if (Mesh)
	{
		EnsureBossAnimation(FString(DefaultAnimScript));
	}

	if (CharacterMovement)
	{
		CharacterMovement->bOrientRotationToMovement = false;
		CharacterMovement->MaxWalkSpeed = 3.0f;
	}

	BattleComponent = AddComponent<UBattleComponent>();
	BattleComponent->bIsPlayerTeam = false;
	BattleComponent->bAcceptKnockback = false;

	PatternComponent = AddComponent<UBossPatternComponent>();
	PatternComponent->BossId = BossId;

	HitFlashComponent = AddComponent<UHitFlashComponent>();

	ApplyBossDefinition();
	InitializeHitFlash();
}

void AMusouBossCharacter::BeginPlay()
{
	ApplyBossDefinition();
	InitializeHitFlash();
	Super::BeginPlay();
}

void AMusouBossCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	PatternComponent = GetComponentByClass<UBossPatternComponent>();
	HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
	ApplyBossDefinition();
	InitializeHitFlash();
}

void AMusouBossCharacter::PostLoad()
{
	Super::PostLoad();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	PatternComponent = GetComponentByClass<UBossPatternComponent>();
	HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
	ApplyBossDefinition();
	InitializeHitFlash();
}

void AMusouBossCharacter::ApplyBossDefinition()
{
	if (!PatternComponent)
	{
		return;
	}

	PatternComponent->BossId = BossId;

	FBossPatternDataRegistry& Registry = FBossPatternDataRegistry::Get();
	Registry.EnsureFresh();

	if (const FBossDefinition* Definition = Registry.FindBoss(BossId))
	{
		if (Mesh && !Definition->MeshPath.empty() && GEngine)
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			if (USkeletalMesh* Asset = FMeshManager::LoadSkeletalMesh(Definition->MeshPath, Device))
			{
				Mesh->SetSkeletalMesh(Asset);
			}
		}
		EnsureBossAnimation(Definition->AnimScript.empty() ? FString(DefaultAnimScript) : Definition->AnimScript);
		PatternComponent->ConfigureFromDefinition(*Definition);
	}
	else
	{
		EnsureBossAnimation(FString(DefaultAnimScript));
		PatternComponent->ConfigureFromBossId(BossId);
	}
}

void AMusouBossCharacter::EnsureBossAnimation(const FString& AnimScript)
{
	if (!Mesh)
	{
		return;
	}

	Mesh->SetAnimInstanceClass(ULuaAnimInstance::StaticClass());
	Mesh->SetAnimationMode(EAnimationMode::AnimationCustom);

	if (ULuaAnimInstance* LuaAnim = Cast<ULuaAnimInstance>(Mesh->GetAnimInstance()))
	{
		LuaAnim->ScriptFile = AnimScript;
		Mesh->InitializeAnimation();
	}
}

void AMusouBossCharacter::InitializeHitFlash()
{
	if (!HitFlashComponent)
	{
		HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
	}

	if (!HitFlashComponent)
	{
		HitFlashComponent = AddComponent<UHitFlashComponent>();
	}

	if (HitFlashComponent && Mesh)
	{
		HitFlashComponent->InitializeFromSkinnedMesh(Mesh);
	}
}
