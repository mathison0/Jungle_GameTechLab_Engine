#include "Game/Musou/Camera/AnimNotify_Slomo.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Input/ActionComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"

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

	APawn* Pawn = Cast<APawn>(OwnerActor);

	// 적/군체 애님이 전역 시간을 늦추는 것을 막는 가드.
	if (bOnlyIfPlayer && (!Pawn || !Pawn->IsPossessed()))
	{
		return;
	}

	// 맞았을 때만 — 즉시 발동하지 않고 GameMode 큐에 예약. 이 공격자가 HitWindow 내
	// 실제 히트를 내면(NotifyAttackHitFeedback) 그 순간 슬로모, 빗나가면 폐기.
	if (bOnlyOnHit)
	{
		UWorld* World = OwnerActor->GetWorld();
		AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
		if (GameMode && Pawn)
		{
			GameMode->RequestHitSlomo(Pawn, Duration, TimeDilation, HitWindow);
		}
		return;
	}

	// 즉시 발동 — 전역 TimeDilation 은 ActionComponent 가 정적 레지스트리로 관리하므로
	// 폰에 달린 컴포넌트 하나만 호출하면 된다. 없으면(에디터 프리뷰 등) 조용히 무시.
	if (UActionComponent* Action = OwnerActor->GetComponentByClass<UActionComponent>())
	{
		Action->Slomo(Duration, TimeDilation);
	}
}
