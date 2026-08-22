#include "Game/Musou/Combat/AnimNotifyState_ComboWindow.h"

#include "Game/Musou/Combat/ComboComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"

UComboComponent* UAnimNotifyState_ComboWindow::ResolveComboComponent(USkeletalMeshComponent* MeshComp)
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	return OwnerActor ? OwnerActor->GetComponentByClass<UComboComponent>() : nullptr;
}

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration)
{
	(void)Anim; (void)TotalDuration;

	if (UComboComponent* Combo = ResolveComboComponent(MeshComp))
	{
		Combo->OpenComboWindow();
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	if (UComboComponent* Combo = ResolveComboComponent(MeshComp))
	{
		Combo->CloseComboWindow();
	}
}
