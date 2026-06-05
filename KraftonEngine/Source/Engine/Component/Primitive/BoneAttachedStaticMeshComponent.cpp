#include "Component/Primitive/BoneAttachedStaticMeshComponent.h"

#include "Component/Primitive/SkinnedMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Math/Transform.h"

void UBoneAttachedStaticMeshComponent::PostDuplicate()
{
	Super::PostDuplicate();
	// 복제본은 자기 owner의 메시를 다시 찾아야 한다 — 캐시 초기화.
	TargetMeshComponent = nullptr;
}

void UBoneAttachedStaticMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateBoneAttachment();
}

void UBoneAttachedStaticMeshComponent::UpdateBoneAttachment()
{
	if (!bFollowBone || TargetBoneName.empty())
	{
		return;
	}

	USkinnedMeshComponent* TargetMesh = ResolveTargetMeshComponent();
	if (!TargetMesh)
	{
		return;
	}

	const FTransform AttachLocalOffset(AttachOffsetLocation, AttachOffsetRotation, AttachOffsetScale);

	FTransform SocketWorldTransform;
	if (!TargetMesh->GetBoneSocketWorldTransform(TargetBoneName, AttachLocalOffset, SocketWorldTransform))
	{
		return;
	}

	// 부모가 있으면 부모 기준 relative로 변환 — UClothComponent::UpdateBoneAttachment와 동일 패턴.
	FTransform TargetRelativeTransform = SocketWorldTransform;
	if (USceneComponent* ParentComponent = GetParent())
	{
		const FMatrix TargetRelativeMatrix = SocketWorldTransform.ToMatrix() * ParentComponent->GetWorldMatrix().GetInverse();
		TargetRelativeTransform = FTransform(TargetRelativeMatrix);
	}

	SetRelativeTransform(TargetRelativeTransform);
}

USkinnedMeshComponent* UBoneAttachedStaticMeshComponent::ResolveTargetMeshComponent()
{
	if (!TargetMeshComponent)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			TargetMeshComponent = OwnerActor->GetComponentByClass<USkinnedMeshComponent>();
		}
	}

	return TargetMeshComponent;
}
