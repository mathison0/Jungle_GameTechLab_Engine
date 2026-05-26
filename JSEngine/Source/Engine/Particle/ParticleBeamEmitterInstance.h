#pragma once

#include "Particle/ParticleBeamTypes.h"
#include "Particle/ParticleEmitterInstance.h"

// Beam emitter용 instance (Cycle 13a, 결정 11 옵션 B + 결정 13 옵션 A + 결정 15 옵션 B).
// base FParticleEmitterInstance 파생.
// override 3종:
//   SpawnParticles : base 호출 후 신규 SlotIndex 의 payload 에 BeamIndex round-robin 분배.
//   Tick           : base Tick 호출 후 Source/Target Component 위치 추적 + BuildVertexBuffer.
//   GetBeamVertexData : Builder가 RenderCommand 슬롯에 매핑하도록 VertexBuffer 노출.
//
// KillParticle 와 BuildInstanceData 는 override 하지 않음 — Beam 은 linked list 의존 없음, base swap-pop 안전.
struct FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
    FParticleBeamEmitterInstance() = default;
    ~FParticleBeamEmitterInstance() override = default;

    void Tick(float DeltaTime, bool bAllowSpawning) override;
    void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation,
                        const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload = nullptr) override;
    const FBeamParticleVertex* GetBeamVertexData(uint32& OutCount) const override;

private:
    // SlotIndex(physical) 기반 payload 포인터. swap-pop이 ParticleIndices 만 swap 하므로 SlotIndex 불변 → 안전.
    FParticleBeamPayload* GetBeamPayload(int32 SlotIndex);

    // BeamStates 재구성 — MaxBeamCount 변경 또는 첫 Tick 진입 시 호출 (Ribbon 의 EnsureTrailState 패턴 답습).
    // base Init 이 non-virtual 이므로 lazy init 패턴 채택 (Cycle 12 회귀 안전 §5 부합).
    void EnsureBeamState();

    // strip 정점 매 frame rebuild — silent bug λ 패턴 유지 (Mesh/Ribbon 와 동일).
    // 위험 5/7/8 방어 코드를 본 함수 내부에 포함.
    void BuildVertexBuffer();

private:
    // 결정 13 옵션 A: BeamStates size = MaxBeamCount. 본 cycle (13a) 에서는 단순 marker — 향후 13b 에서
    // per-beam noise 시드 또는 상태 보존 용도로 확장 예정. 현재는 sized only.
    TArray<int32> BeamStates;

    // round-robin BeamIndex 분배 — SpawnParticles 에서 ++NextBeamIndex % MaxBeamCount (위험 9 방어).
    int32 NextBeamIndex = 0;

    // 매 frame BuildVertexBuffer 가 채우는 strip 정점 버퍼 — slot 0 dynamic VB 의 source.
    TArray<FBeamParticleVertex> VertexBuffer;
};
