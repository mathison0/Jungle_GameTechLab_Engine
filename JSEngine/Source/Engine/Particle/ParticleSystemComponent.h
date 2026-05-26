#pragma once

#include "Component/PrimitiveComponent.h"
#include "Particle/ParticleEmitterInstance.h"

class FRenderBus;

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

	// Cycle 14 (M1, 결정 18 옵션 β): Builder 가 BuildInstanceData() 호출 직전에 호출.
	// RenderBus 의 camera 4 vector + position 을 Component 멤버에 캐싱 →
	// derived Mesh instance 의 BuildInstanceData 가 alignment 계산에 사용 (PSA_FacingCameraPosition).
	// signature 변경 0건 보장 — 옵션 α (BuildInstanceData 인자 확장) 회피.
	// 첫 frame 또는 RenderBus 부재 frame 에서는 bCachedCameraValid=false → derived 가 PSA_Velocity fallback (위험 12 방어).
	void CacheCameraFromRenderBus(const FRenderBus& InRenderBus);

	// Cycle 14 (M1): cached camera accessor. derived instance 가 read.
	// bCachedCameraValid 가 false 면 다른 4 vector 값은 의미 없음 (zero-init).
	bool IsCachedCameraValid() const { return bCachedCameraValid; }
	const FVector& GetCachedCameraPosition() const { return CachedCameraPosition; }
	const FVector& GetCachedCameraForward() const { return CachedCameraForward; }
	const FVector& GetCachedCameraUp() const { return CachedCameraUp; }
	const FVector& GetCachedCameraRight() const { return CachedCameraRight; }

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

	// Cycle 14 (M1, 결정 18 옵션 β): RenderBus → Component → derived instance 캐싱 경로.
	// 첫 frame 또는 외부 호출자 (예: EditorMainPanelDebug) 가 CacheCameraFromRenderBus 미호출 시 bCachedCameraValid=false 유지 →
	// PSA_FacingCameraPosition 모드는 PSA_Velocity 로 fallback (silent bug 위험 12 방어).
	// frame-단위 갱신: Builder 가 매 frame BuildInstanceData 직전 호출하므로 Builder path 는 항상 valid.
	FVector CachedCameraPosition = FVector::ZeroVector;
	FVector CachedCameraForward = FVector::ZeroVector;
	FVector CachedCameraUp = FVector::ZeroVector;
	FVector CachedCameraRight = FVector::ZeroVector;
	bool bCachedCameraValid = false;
};
