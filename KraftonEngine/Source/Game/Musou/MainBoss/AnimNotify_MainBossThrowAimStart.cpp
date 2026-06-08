#include "Game/Musou/MainBoss/AnimNotify_MainBossThrowAimStart.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Musou/MainBoss/MainBossPatternComponent.h"
#include "GameFramework/AActor.h"

void UAnimNotify_MainBossThrowAimStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UMainBossPatternComponent* PatternComponent = Owner ? Owner->GetComponentByClass<UMainBossPatternComponent>() : nullptr;
	if (PatternComponent)
	{
		PatternComponent->NotifyThrowAimStart();
	}
}
