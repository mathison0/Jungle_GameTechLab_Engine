#include "Game/Musou/MainBoss/MainBossCharacter.h"

#include "Component/Movement/CharacterMovementComponent.h"
#include "Component/Primitive/HitFlashComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/MainBoss/MainBossPatternComponent.h"

void AMainBossCharacter::InitDefaultComponents()
{
	InitDefaultComponents(FString(DefaultMeshPath));
}

void AMainBossCharacter::InitDefaultComponents(const FString& SkeletalMeshFileName)
{
	ALuaCharacter::InitDefaultComponents(SkeletalMeshFileName, FString());

	bAutoInputMouseLook = false;
	bUseControllerRotationYaw = false;

	if (CharacterMovement)
	{
		CharacterMovement->bOrientRotationToMovement = false;
		CharacterMovement->MaxWalkSpeed = 2.4f;
	}

	BattleComponent = AddComponent<UBattleComponent>();
	BattleComponent->MaxHealth = 2200.0f;
	BattleComponent->AttackPower = 22.0f;
	BattleComponent->bIsPlayerTeam = false;
	BattleComponent->bAcceptKnockback = false;

	PatternComponent = AddComponent<UMainBossPatternComponent>();
	HitFlashComponent = AddComponent<UHitFlashComponent>();

	InitializeHitFlash();
}

void AMainBossCharacter::BeginPlay()
{
	InitializeHitFlash();
	Super::BeginPlay();
}

void AMainBossCharacter::PostDuplicate()
{
	Super::PostDuplicate();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	PatternComponent = GetComponentByClass<UMainBossPatternComponent>();
	HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
	InitializeHitFlash();
}

void AMainBossCharacter::PostLoad()
{
	Super::PostLoad();
	BattleComponent = GetComponentByClass<UBattleComponent>();
	PatternComponent = GetComponentByClass<UMainBossPatternComponent>();
	HitFlashComponent = GetComponentByClass<UHitFlashComponent>();
	InitializeHitFlash();
}

void AMainBossCharacter::InitializeHitFlash()
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
