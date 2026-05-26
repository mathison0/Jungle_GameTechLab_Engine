#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleEmitterInstance.h"

DECLARE_DELEGATE(FOnParticleCollide, const FParticleEventCollideData&);

UCLASS(SpawnableComponent, DisplayName = "Particle System Component", Category = "Effects")
class UParticleSystemComponent : public UPrimitiveComponent
{
public:
	GENERATED_BODY(UParticleSystemComponent, UPrimitiveComponent)

	UParticleSystemComponent();
	~UParticleSystemComponent() override;

	void SetTemplate(UParticleSystem* InTemplate);
	void PostEditProperty(const char* PropertyName) override;
	UParticleSystem* GetTemplate() const { return Template; }
	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; } // component가 사용하는 emitter instance들
	TArray<FParticleEventCollideData>& GetPendingCollisionEvents() { return PendingCollisionEvents; }
	const TArray<FParticleEventCollideData>& GetPendingCollisionEvents() const { return PendingCollisionEvents; }

	void RecreateEmitterInstances();
	void ClearEmitterInstances();
	void TickPreview(float DeltaTime, bool bAllowSpawning);
	float ComputeEmitterLODDistance() const;
	void QueueCollisionEvent(const FParticleEventCollideData& EventData);
	void DispatchQueuedParticleEvents();

	// Cycle 10c 계층 분리: type-agnostic dispatch hook.
	// 모든 emitter instance에 대해 Instance->BuildInstanceData()를 호출만 함 (내부 buffer 갱신).
	// RenderCommand 매핑은 Builder가 별도 수행 — Component는 FRenderCommand 모름.
	void BuildInstanceData();

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
	UPROPERTY(DisplayName = "Template", ReferenceKind = Asset)
	UParticleSystem* Template = nullptr;

	TArray<FParticleEmitterInstance*> EmitterInstances;
	TArray<FParticleEventCollideData> PendingCollisionEvents;
};
