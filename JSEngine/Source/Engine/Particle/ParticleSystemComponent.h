#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleEmitterInstance.h"
#include "Render/Resource/VertexTypes.h"

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
	void TickPreview(float DeltaTime, bool bAllowSpawning);
	float ComputeEmitterLODDistance() const;
	void QueueCollisionEvent(const FParticleEventCollideData& EventData);
	void DispatchQueuedParticleEvents();

	// 활성 파티클을 emitter별 FSpriteParticleInstanceData 배열로 변환합니다.
	// PrimitiveDrawCommandBuilder가 FRenderCommand 발행 직전에 호출합니다.
	void BuildSpriteInstanceData();
	const TArray<FSpriteParticleInstanceData>& GetEmitterInstanceData(int32 EmitterIndex) const;

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

	// Per-emitter Sprite instance data 캐시. 매 프레임 BuildSpriteInstanceData에서 갱신.
	TArray<TArray<FSpriteParticleInstanceData>> EmitterInstanceData;
};
