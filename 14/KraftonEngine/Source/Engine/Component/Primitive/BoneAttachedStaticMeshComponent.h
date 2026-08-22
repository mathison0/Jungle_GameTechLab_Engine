#pragma once

#include "Component/Primitive/StaticMeshComponent.h"
#include "Math/Rotator.h"

#include "Source/Engine/Component/Primitive/BoneAttachedStaticMeshComponent.generated.h"

class USkinnedMeshComponent;

// ============================================================
// UBoneAttachedStaticMeshComponent — 본 추적 스태틱 메시 (무기/장신구 부착용)
//
// Skeleton 에셋 소켓 없이, 매 틱 대상 본의 월드 트랜스폼(+로컬 오프셋)을
// 따라간다. "데이터 없는 소켓" — 본 이름/오프셋은 에디터 프로퍼티로 편집.
//
//   사용: 캐릭터 액터에 AddComponent → StaticMesh(무기) 지정 →
//         Target Bone / Attach Offset 조정. 무기 교체는 SetStaticMesh로.
//
// 대상 메시는 기본적으로 owner의 USkinnedMeshComponent를 자동 탐색하며,
// SetTargetMeshComponent로 명시 지정도 가능 (다른 액터의 메시 등).
//
// 틱 순서 주의: 본 포즈는 SkeletalMeshComponent 틱에서 평가되므로,
// 이 컴포넌트가 메시보다 늦게 추가(틱 등록)되어야 같은 프레임 포즈를 따른다.
// (ACharacter 계열은 Mesh가 먼저 구성되므로 일반적인 AddComponent 순서면 OK)
// ============================================================
UCLASS()
class UBoneAttachedStaticMeshComponent : public UStaticMeshComponent
{
public:
	GENERATED_BODY()
	UBoneAttachedStaticMeshComponent()
	{
		// 에디터에서 본 이름/오프셋 편집 시 즉시 반영되도록 (PIE 무관 컴포넌트 단위 editor tick)
		bTickInEditor = true;
	}
	~UBoneAttachedStaticMeshComponent() override = default;

	void PostDuplicate() override;

	// 대상 메시 명시 지정 — null이면 owner의 USkinnedMeshComponent 자동 탐색으로 복귀.
	void SetTargetMeshComponent(USkinnedMeshComponent* InMesh) { TargetMeshComponent = InMesh; }

	UPROPERTY(Edit, Save, Category = "BoneAttach", DisplayName = "Follow Bone")
	bool bFollowBone = true;

	UPROPERTY(Edit, Save, Category = "BoneAttach", DisplayName = "Target Bone")
	FString TargetBoneName = "Bip001 R Hand";

	// 그립 보정 — 본 로컬 기준 오프셋
	UPROPERTY(Edit, Save, Category = "BoneAttach", DisplayName = "Attach Offset Location", Type = Vec3, Speed = 0.01f)
	FVector AttachOffsetLocation = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category = "BoneAttach", DisplayName = "Attach Offset Rotation", Type = Rotator, Speed = 0.1f)
	FRotator AttachOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(Edit, Save, Category = "BoneAttach", DisplayName = "Attach Offset Scale", Type = Vec3, Speed = 0.01f)
	FVector AttachOffsetScale = FVector(1.0f, 1.0f, 1.0f);

	// ── 칼집 프로필 — 납도 상태(등에 멘 무기)용 본/오프셋. bSheathed 로 전환 ──
	// 에디터에서 bSheathed 를 켜고 오프셋을 조정하면 등 위치를 실시간 튜닝할 수 있다.
	UPROPERTY(Edit, Save, Category = "BoneAttach|Sheath", DisplayName = "Sheathed")
	bool bSheathed = false;

	UPROPERTY(Edit, Save, Category = "BoneAttach|Sheath", DisplayName = "Sheath Bone")
	FString SheathBoneName = "spine_03";

	UPROPERTY(Edit, Save, Category = "BoneAttach|Sheath", DisplayName = "Sheath Offset Location", Type = Vec3, Speed = 0.01f)
	FVector SheathOffsetLocation = FVector::ZeroVector;

	UPROPERTY(Edit, Save, Category = "BoneAttach|Sheath", DisplayName = "Sheath Offset Rotation", Type = Rotator, Speed = 0.1f)
	FRotator SheathOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(Edit, Save, Category = "BoneAttach|Sheath", DisplayName = "Sheath Offset Scale", Type = Vec3, Speed = 0.01f)
	FVector SheathOffsetScale = FVector(1.0f, 1.0f, 1.0f);

	void SetSheathed(bool bInSheathed) { bSheathed = bInSheathed; }
	bool IsSheathed() const { return bSheathed; }

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	// 본 소켓 월드 트랜스폼을 자기 트랜스폼에 반영 (부모가 있으면 relative로 변환).
	void UpdateBoneAttachment();

	USkinnedMeshComponent* ResolveTargetMeshComponent();

	// 자동 탐색 캐시 — 직렬화 제외 (PostDuplicate에서 초기화).
	USkinnedMeshComponent* TargetMeshComponent = nullptr;
};
