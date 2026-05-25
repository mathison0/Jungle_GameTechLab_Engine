#pragma once

#include "Particle/ParticleSystem.h"
#include "Render/Resource/VertexTypes.h"

class UParticleSystemComponent;
struct FRenderCommand;

struct FParticleEmitterInstance
{
public:
    FParticleEmitterInstance() = default;
    // 가상 소멸자 — Mesh/Ribbon/Beam 파생 instance가 base 포인터로 delete 될 때 derived 소멸자 호출 보장.
    // 누락 시 Cycle 11+에서 leak 발현하므로 base 단독 cycle에서 미리 도입.
    virtual ~FParticleEmitterInstance();

    void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, int32 InEmitterIndex);
    void Reset();
    virtual void Tick(float DeltaTime);
    void SelectLODLevel(float Distance);
    virtual void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
                                const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr);
    virtual void KillParticle(int32 Index);

    // TypeData payload byte 수 조회 helper. TypeDataModule 부재 시 0.
    // Init에서 ParticleStride 계산에 사용. 향후 Ribbon/Beam이 override 가능성 있으나 본 cycle은 default.
    virtual int32 GetRequiredPayloadBytes() const;

    // Cycle 10b: type-specific instance data를 빌드해 OutCmd의 type별 슬롯에 채움.
    // base 구현 = Sprite path (SpriteInstanceDataBuffer 채우고 OutCmd.ParticleInstances + VertexFactoryType=SpriteParticle).
    // Mesh/Ribbon/Beam derived instance가 override해 자기 type의 슬롯을 채움.
    // Builder는 generic 필드(SourcePrimitive, PerObjectConstants, WorldAABB, atlas)만 채우고 이 메서드에 위임.
    // 비대칭 인지: 데이터 생성은 polymorphism(여기), 렌더 분기는 procedural switch(RenderPass) — 의도된 비대칭(사용자 결정 4).
    virtual void BuildInstanceData(FRenderCommand& OutCmd);

	// Getter
    int32 GetActiveParticleCount() const { return ActiveParticles; }
    int32 GetMaxActiveParticleCount() const { return MaxActiveParticles; }
    int32 GetParticleStride() const { return ParticleStride; }
    int32 GetParticleSize() const { return ParticleSize; }

	const uint8* GetParticleData() const { return ParticleData; }
    const uint16* GetParticleIndices() const { return ParticleIndices; }

	UParticleEmitter* GetTemplate() const { return SpriteTemplate; }
    UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }
    int32 GetCurrentLODLevelIndex() const { return CurrentLODLevelIndex; }
    int32 GetEmitterIndex() const { return EmitterIndex; }
    uint32 GetParticleCounter() const { return ParticleCounter; }
    virtual FParticleEmitterRuntimeView GetRuntimeView() const;

	FBaseParticle* GetParticle(int32 ActiveIndex);
    const FBaseParticle* GetParticle(int32 ActiveIndex) const;

	UParticleSystemComponent* GetComponent() const { return Component; }
    FVector GetComponentWorldLocation() const;
	UParticleSystemComponent* GetOwningComponent() const { return Component; }


    void QueueCollisionEvent(const FParticleEventCollideData& EventData);
    void DispatchQueuedParticleEvents();
    int32 ConsumeSpawnCount(float Rate, float DeltaTime);

private:
	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;
	int32 EmitterIndex = -1;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	// 실제 데이터들, memory pool and live data
	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
	uint8* InstanceData = nullptr;
	int32 InstancePayloadSize = 0;
	int32 PayloadOffset = 0;
	int32 ParticleSize = sizeof(FBaseParticle);
	int32 ParticleStride = sizeof(FBaseParticle);
	int32 ActiveParticles = 0;
	uint32 ParticleCounter = 0;
	int32 MaxActiveParticles = 0;
	float SpawnFraction = 0.0f;

	// Cycle 10b: per-instance Sprite instance data 버퍼 (buffer ownership을 Component → Instance로 이전, 사용자 결정 X).
	// base/Sprite 전용. Mesh/Ribbon/Beam derived instance는 자기 type 버퍼를 별도 멤버로 보유 (Cycle 11+에서 추가).
	// 매 프레임 BuildInstanceData에서 clear → 활성 particle 순회 → push_back.
	TArray<FSpriteParticleInstanceData> SpriteInstanceDataBuffer;
};
