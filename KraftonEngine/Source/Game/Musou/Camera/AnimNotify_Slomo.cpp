#include "Game/Musou/Camera/AnimNotify_Slomo.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Input/ActionComponent.h"
#include "GameFramework/Pawn/Pawn.h"

void UAnimNotify_Slomo::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	if (!MeshComp)
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 적/군체 애님이 전역 시간을 늦추는 것을 막는 가드.
	if (bOnlyIfPlayer)
	{
		APawn* Pawn = Cast<APawn>(OwnerActor);
		if (!Pawn || !Pawn->IsPossessed())
		{
			return;
		}
	}

	// 전역 TimeDilation 은 ActionComponent 가 정적 레지스트리로 관리 — 폰에 달린
	// 컴포넌트 하나만 호출하면 된다. 없으면(에디터 프리뷰 등) 조용히 무시.
	if (UActionComponent* Action = OwnerActor->GetComponentByClass<UActionComponent>())
	{
		Action->Slomo(Duration, TimeDilation);
	}
}
