#pragma once

#include "Particle/ParticleSystem.h"
#include "Render/Resource/VertexTypes.h"

class UParticleSystemComponent;

// Cycle 10c 계층 분리: Instance는 RenderCommand를 모름.
// 4종 type별 getter의 반환 타입에 필요한 forward declaration.
// 실제 struct 정의는 각 emitter cycle에서 (Mesh: Cycle 11, Ribbon: Cycle 12b, Beam: Cycle 13b).
// 본 cycle은 Sprite buffer만 실제 노출, 나머지 3개는 base default nullptr.
struct FMeshParticleInstanceData;
struct FRibbonParticleVertex;
struct FBeamParticleVertex;

struct FParticleEmitterInstance
{
public:
    FParticleEmitterInstance() = default;
    // 가상 소멸자 — Mesh/Ribbon/Beam 파생 instance가 base 포인터로 delete 될 때 derived 소멸자 호출 보장.
    // 누락 시 Cycle 11+에서 leak 발현하므로 base 단독 cycle에서 미리 도입.
    virtual ~FParticleEmitterInstance();

    void Init(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent, int32 InEmitterIndex);
    void Reset();
    virtual void Tick(float DeltaTime, bool bAllowSpawning);
    void SelectLODLevel(float Distance);
    virtual void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
                                const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr);
    virtual void KillParticle(int32 Index);

    // RendererProperties payload byte 수 조회 helper. RendererProperties 부재 시 0.
    // Init에서 ParticleStride 계산에 사용. 향후 Ribbon/Beam이 override 가능성 있으나 본 cycle은 default.
    virtual int32 GetRequiredPayloadBytes() const;

    // Cycle 10c 계층 분리: 인자 없는 build. 내부 buffer 갱신만 — FRenderCommand를 인자로 받지 않음.
    // base 구현 = Sprite path (SpriteInstanceDataBuffer 채움). Mesh/Ribbon/Beam derived는 자기 buffer override.
    // RenderCommand 매핑은 Builder 책임 (instance는 데이터 노출만, 매핑은 모름).
    virtual void BuildInstanceData();

    // Cycle 10c 계층 분리: type별 명시 getter 4종 — Builder가 RenderMode로 switch해 적절한 getter 호출.
    // base 구현 = Sprite만 실제 buffer 노출 (sprite renderer가 base instance 사용).
    // 다른 3개는 nullptr 반환. Mesh/Ribbon/Beam derived가 Cycle 11+에서 자기 메서드 override.
    // 사용자 결정 2: void* 사용 금지 (type-safe), Visitor 금지.
    virtual const FSpriteParticleInstanceData* GetSpriteInstanceData(uint32& OutCount) const;
    virtual const FMeshParticleInstanceData* GetMeshInstanceData(uint32& OutCount) const;
    virtual const FRibbonParticleVertex* GetRibbonVertexData(uint32& OutCount) const;
    virtual const FBeamParticleVertex* GetBeamVertexData(uint32& OutCount) const;

    // Getter
    int32 GetActiveParticleCount() const { return ActiveParticles; }
    int32 GetMaxActiveParticleCount() const { return MaxActiveParticles; }
    int32 GetParticleStride() const { return ParticleStorage.GetStride(); } // Cycle 10d: container로 위임
    int32 GetParticleSize() const { return ParticleSize; }
    int32 GetParticleMemoryBytes() const { return ParticleStorage.GetMemoryBytes(); }

    const uint8* GetParticleData() const { return ParticleStorage.ParticleData; }
    const uint16* GetParticleIndices() const { return ParticleStorage.ParticleIndices; }

    UParticleEmitter* GetTemplate() const { return SpriteTemplate; }
    UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }
    const FCompiledParticleLODData* GetCurrentCompiledLODData() const { return CurrentCompiledLOD; }
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

	bool CanRebindCompiledLOD(const FCompiledParticleLODData* NewLOD) const;
    void RebindCompiledLOD(float Distance);

protected:
    // Cycle 11: derived (Mesh/Ribbon/Beam) instance가 payload 영역에 접근하기 위해 protected로 노출.
    // ParticleStorage는 container 자체가 public 멤버 (ParticleData/Indices/Stride)를 제공.
    // PayloadOffset/ActiveParticles는 derived의 Spawn override + BuildInstanceData에 필요.
    // 외부(component/builder)는 여전히 public getter만 사용.
    FParticleDataContainer ParticleStorage;
    int32 PayloadOffset = 0;
    int32 ActiveParticles = 0;

private:
    UParticleEmitter* SpriteTemplate = nullptr;
    UParticleSystemComponent* Component = nullptr;
    int32 EmitterIndex = -1;

    int32 CurrentLODLevelIndex = 0;
    UParticleLODLevel* CurrentLODLevel = nullptr;
    const FCompiledParticleLODData* CurrentCompiledLOD = nullptr;
    // 실제 데이터들, memory pool and live data — ParticleStorage/PayloadOffset/ActiveParticles는 위 protected로 이동.
    uint8* InstanceData = nullptr;
    int32 InstancePayloadSize = 0;
    int32 ParticleSize = sizeof(FBaseParticle);
    // Cycle 10d: ParticleStride 멤버 삭제 — source-of-truth가 FParticleDataContainer로 이전.
    // 외부 read는 GetParticleStride() (container.GetStride() 위임) 또는 ParticleStorage.GetStride() 직접.
    uint32 ParticleCounter = 0;
    int32 MaxActiveParticles = 0;
    float SpawnFraction = 0.0f;

	uint32 ObservedCompiledRevision = 0; // Cycle 10e: CompiledRevision 관찰용 (LOD 변경 감지) — Init에서 초기화, Tick에서 비교 후 필요 시 LOD 재선택.
    int32 ObservedPayloadSize = 0;
    int32 ObservedParticleStride = 0;
    EParticleEmitterRenderMode ObservedRenderMode = EParticleEmitterRenderMode::Sprite;

    // Cycle 10b: per-instance Sprite instance data 버퍼 (buffer ownership을 Component → Instance로 이전, 사용자 결정 X).
    // base/Sprite 전용. Mesh/Ribbon/Beam derived instance는 자기 type 버퍼를 별도 멤버로 보유 (Cycle 11+에서 추가).
    // 매 프레임 BuildInstanceData에서 clear → 활성 particle 순회 → push_back.
    TArray<FSpriteParticleInstanceData> SpriteInstanceDataBuffer;
};
