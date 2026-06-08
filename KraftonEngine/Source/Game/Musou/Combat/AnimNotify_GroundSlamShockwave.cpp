#include "Game/Musou/Combat/AnimNotify_GroundSlamShockwave.h"

#include "Game/Musou/Character/MusouCharacter.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/CoreTypes.h"

void UAnimNotify_GroundSlamShockwave::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
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

	// 시작점 = 캐릭터(지면 강타 위치), 방향 = 전방. 이후 캐릭터 Tick 이 전방 진행을 구동.
	Character->StartGroundSlamShockwave(
		Character->GetActorLocation(),
		Character->GetActorForward(),
		Distance, Duration, Pulses, FName(AttackId), SlashSpeed, SlashLife, SlashYaw);
}
