#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Common/RenderTypes.h"
#include "Engine/Geometry/Ray.h"
#include "Core/CollisionTypes.h"
#include "Engine/Geometry/AABB.h"

class UPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	/* For Property window */
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char * PropertyName) override;

	virtual void Serialize(FArchive& Ar) override;

	/* Visibility */
	void SetVisibility(bool bVisible);
	bool IsVisible() const { return bIsVisible; }

	void SetEnableCull(const bool bInEnableCull) { bEnableCull = bInEnableCull; }
	bool IsEnableCull() const { return bEnableCull; }

	/* Getter */
	virtual const FAABB& GetWorldAABB() const 
	{ 
		UpdateWorldAABB();
		return WorldAABB;
	}

	/* For Collision(Ray-casting) */
	virtual void UpdateWorldAABB() const = 0;
	bool Raycast(const FRay& Ray, FHitResult& OutHitResult);
	bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1,
						   const FVector& V2, float& OutT);
	virtual bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) = 0;

	/* For Transform */
	void UpdateWorldMatrix() const override;
	void AddWorldOffset(const FVector& WorldDelta) override;
	virtual EPrimitiveType GetPrimitiveType() const = 0;

	/* For Material */
	virtual int32 GetNumMaterials() const { return 0; }
	virtual class UMaterialInterface* GetMaterial(int32 SlotIndex) const { return nullptr; }
	virtual void SetMaterial(int32 SlotIndex, class UMaterialInterface* InMaterial) {}

	virtual bool SupportsOutline() const { return true; }

	/* for BVH Query */
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	/* For Collision */
	bool IsGenerateOverlapEvents() const { return bGenerateOverlapEvents; }
	bool IsBlockComponent() const { return bBlockComponent; }
	const TArray<FOverlapResult>& GetOverlapInfos() const { return OverlapInfos; }
	bool IsOverlappingActor(const AActor* Other) const;

	void AddOverlapInfo(AActor* OtherActor, UPrimitiveComponent* OtherComp);
	void RemoveOverlapInfo(AActor* OtherActor, UPrimitiveComponent* OtherComp);

protected:
	void OnTransformDirty() override;
	void NotifySpatialIndexDirty() const;

protected:
	mutable FAABB WorldAABB;
	bool bIsVisible = true;
	bool bEnableCull = true; // frustum, occlusion culling으로 컬링될지 여부 판정
	
	bool bGenerateOverlapEvents = true; // 켜지면 Overlap 이벤트를 발생시킵니다.
	bool bBlockComponent = false;       // 켜지면 다른 컴포넌트를 밀어냅니다.
	TArray<FOverlapResult> OverlapInfos;
};
