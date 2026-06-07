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

	// 본 월드는 raw 행렬로 — GetBoneSocketWorldTransform 경로는 중간에 TRS 분해→재합성을
	// 거치며 (스케일/시어 소실 + 분해 오차 누적) 특정 포즈에서 회전이 틀어진다.
	// 행렬로만 합성하고 분해는 마지막 SetRelativeTransform 직전 한 번만.
	FMatrix BoneWorldMatrix;
	if (!TargetMesh->GetBoneWorldMatrixByName(TargetBoneName, BoneWorldMatrix))
	{
		return;
	}

	const FTransform AttachLocalOffset(AttachOffsetLocation, AttachOffsetRotation, AttachOffsetScale);
	const FMatrix SocketWorldMatrix = AttachLocalOffset.ToMatrix() * BoneWorldMatrix;

	// 부모가 있으면 부모 기준 relative로 변환 — UClothComponent::UpdateBoneAttachment와 동일 패턴.
	FMatrix TargetRelativeMatrix = SocketWorldMatrix;
	if (USceneComponent* ParentComponent = GetParent())
	{
		TargetRelativeMatrix = SocketWorldMatrix * ParentComponent->GetWorldMatrix().GetInverse();
	}

	SetRelativeTransform(FTransform(TargetRelativeMatrix));
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
