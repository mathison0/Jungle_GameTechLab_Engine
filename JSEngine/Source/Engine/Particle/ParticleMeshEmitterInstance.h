#pragma once

#include "Particle/ParticleEmitterInstance.h"
#include "Render/Resource/VertexTypes.h"

struct FMeshRotationPayload;

// Mesh emitter용 instance (Cycle 11, 옵션 B).
// base FParticleEmitterInstance 파생 — Tick/KillParticle은 base 그대로 사용 (Mesh는 swap-pop 안전).
// override 3종:
//   SpawnParticles : base 호출 후 신규 SlotIndex의 payload(FMeshRotationPayload) 초기화.
//   BuildInstanceData : payload Rotation += RotRate * dt 누적 후 MeshInstanceDataBuffer 채움.
//   GetMeshInstanceData : Builder가 RenderCommand 슬롯에 매핑하도록 buffer 노출.
struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
    FParticleMeshEmitterInstance() = default;
    ~FParticleMeshEmitterInstance() override = default;

    void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
                        const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr) override;
    void BuildInstanceData() override;
    const FMeshParticleInstanceData* GetMeshInstanceData(uint32& OutCount) const override;

    // Cycle 14 (M2, 결정 20 옵션 A): payload access path public 화.
    // UParticleModuleMeshRotationRate::Spawn / Update 에서 Cast<FParticleMeshEmitterInstance>(Owner) 후 호출.
    // base class 변경 0건 — Mesh derived 만 노출, 다른 emitter (Sprite/Ribbon/Beam) 와 무관.
    // SlotIndex(physical) 기반 — swap-pop 안전.
    FMeshRotationPayload* GetMeshPayload(int32 SlotIndex);

    // Cycle 14 (M2): ActiveIdx (compact list) → SlotIndex 변환 + payload 회수 편의 helper.
    // Update 루프에서 `for (int32 i = 0; i < ActiveCount; ++i)` 형태일 때 사용.
    FMeshRotationPayload* GetMeshPayloadAt(int32 ActiveIdx);

private:

    // base의 SpriteInstanceDataBuffer 대응. 매 frame BuildInstanceData에서 clear → reserve → push_back.
    // silent bug λ 패턴 유지 (본 cycle 작업 범위 외 — 결정 8 별도 cycle).
    TArray<FMeshParticleInstanceData> MeshInstanceDataBuffer;
};
