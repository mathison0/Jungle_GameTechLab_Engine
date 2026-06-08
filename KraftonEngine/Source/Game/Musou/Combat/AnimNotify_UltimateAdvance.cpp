#include "Game/Musou/Combat/AnimNotify_UltimateAdvance.h"

#include "Game/Musou/Character/MusouCharacter.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/CoreTypes.h"

void UAnimNotify_UltimateAdvance::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;
	if (!MeshComp)
	{
		return;
	}

	AMusouCharacter* Character = Cast<AMusouCharacter>(MeshComp->GetOwner());
	if (!Character)
	{
		return;
	}

	Character->AdvanceUltimateNow();
}
