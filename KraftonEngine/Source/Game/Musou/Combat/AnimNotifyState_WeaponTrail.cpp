#include "Game/Musou/Combat/AnimNotifyState_WeaponTrail.h"

#include "Component/Primitive/WeaponTrailComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"

UWeaponTrailComponent* UAnimNotifyState_WeaponTrail::ResolveWeaponTrailComponent(
	USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp)
	{
		return nullptr;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	UWeaponTrailComponent* FirstTrail = nullptr;

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		UWeaponTrailComponent* Trail = Cast<UWeaponTrailComponent>(Component);
		if (!Trail)
		{
			continue;
		}

		if (!FirstTrail)
		{
			FirstTrail = Trail;
		}

		if (TrailComponentName.IsValid() &&
			TrailComponentName != FName::None &&
			Trail->GetFName() == TrailComponentName)
		{
			return Trail;
		}
	}

	return FirstTrail;
}

void UAnimNotifyState_WeaponTrail::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration)
{
	(void)Anim;
	(void)TotalDuration;

	UWeaponTrailComponent* Trail = ResolveWeaponTrailComponent(MeshComp);
	if (!Trail)
	{
		return;
	}

	if (bClearTrailOnBegin)
	{
		Trail->ClearTrail();
	}

	Trail->SetTrailEnabled(true);
}

void UAnimNotifyState_WeaponTrail::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	UWeaponTrailComponent* Trail = ResolveWeaponTrailComponent(MeshComp);
	if (!Trail)
	{
		return;
	}

	Trail->SetTrailEnabled(false);
}
