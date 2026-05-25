#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleEmitterInstance.h"

DECLARE_DELEGATE(FOnParticleCollide, const FParticleEventCollideData&);

UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent() = default;
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }
	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; } // component가 사용하는 emitter instance들
	TArray<FParticleEventCollideData>& GetPendingCollisionEvents() { return PendingCollisionEvents; }
	const TArray<FParticleEventCollideData>& GetPendingCollisionEvents() const { return PendingCollisionEvents; }

	void RecreateEmitterInstances();
	void ClearEmitterInstances();
	float ComputeEmitterLODDistance() const;
	void QueueCollisionEvent(const FParticleEventCollideData& EventData);
	void DispatchQueuedParticleEvents();

	// Cycle 10b (사용자 결정 C+X): 모든 instance data build/getter는 instance owns + Builder가 직접 호출.
	// 기존 BuildSpriteInstanceData / GetEmitterInstanceData / BuildInstanceData / EmitterInstanceData 모두 제거.
	// PrimitiveDrawCommandBuilder가 emitter 루프 안에서 Instance->BuildInstanceData(Cmd) 직접 호출.

	EPrimitiveType GetPrimitiveType() const override { return EPrimitiveType::EPT_ParticleSystem; }
	void UpdateWorldAABB() const override;
	bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult) override;
	bool SupportsOutline() const override { return false; }

	FOnParticleCollide OnParticleCollide;

	int32 GetTotalActiveParticleCount() const;

	int32 GetEmitterInstanceCount() const;
	FParticleEmitterInstance* GetEmitterInstance(int32 Index);
	const FParticleEmitterInstance* GetEmitterInstance(int32 Index) const;

protected:
	void TickComponent(float DeltaTime) override;

private:

    /*
	Template 포인터는 “리플렉션을 통해 에셋을 할당받는 통로”이자, 
	“런타임 인스턴스들이 언제든 원본 설계를 참조하여 스스로를 재구성(Reload)할 수 있게 만드는 기준점”입니다. 
	포인터 자체는 단순한 주소값이지만, 이를 통해 컴포넌트는 복잡한 인스턴스 시뮬레이션을 통제할 수 있게 됩니다.
	*/
	UPROPERTY(DisplayName = "Template")
	UParticleSystem* Template = nullptr;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FParticleEventCollideData> PendingCollisionEvents;
};
