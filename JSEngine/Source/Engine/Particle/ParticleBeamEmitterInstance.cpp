#include "Particle/ParticleBeamEmitterInstance.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "Component/SceneComponent.h"
#include "Math/Utils.h"
#include "Particle/ParticleBeamTypes.h"
#include "Particle/ParticleModuleBeamNoise.h"
#include "Particle/ParticleModuleBeamSource.h"
#include "Particle/ParticleModuleBeamTarget.h"
#include "Particle/ParticleModuleTypeDataBeam.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleSystemComponent.h"

namespace
{
    // 위험 7 방어용 epsilon — Ribbon 의 RibbonSmallNumber ([ParticleRibbonEmitterInstance.cpp:11]) 패턴 답습.
    constexpr float BeamSmallNumber = 1.0e-6f;

    // 위험 8 방어용 InterpolationPoints clamp 상한 — UPROPERTY Min/Max(0..64) 와 동일.
    // UPROPERTY 가 1차, BuildVertexBuffer 가 2차 방어 (사용자가 reflection 우회로 값을 set 했을 가능성).
    constexpr int32 BeamInterpolationPointsMax = 64;

    // 위험 11 방어용 임계값 — Tangent 가 WorldUp 과 얼마나 평행해야 axis 전환할지 (|dot| > 0.99).
    // ComputeBeamLocalAxes 에서 사용.
    constexpr float BeamAxisParallelDot = 0.99f;

    // strip 폭 방향 perpendicular 생성 — Ribbon 의 ComputePerpendicular ([ParticleRibbonEmitterInstance.cpp:16-26]) 와 동일.
    // world Up (0,0,1) reference. Tangent 가 Up 과 평행한 degenerate 의 경우 X 축 fallback.
    FVector ComputePerpendicular(const FVector& Tangent)
    {
        const FVector Up(0.0f, 0.0f, 1.0f);
        FVector Perp = FVector::CrossProduct(Tangent, Up);
        if (Perp.SizeSquared() < BeamSmallNumber)
        {
            Perp = FVector::CrossProduct(Tangent, FVector(1.0f, 0.0f, 0.0f));
        }
        Perp.Normalize();
        return Perp;
    }

    // Cycle 13b 분기 3 B (Beam-local 좌표): Tangent 직교 평면의 두 perp 축 계산.
    // 위험 11 방어 (perp axis singular): Tangent 가 WorldUp 과 거의 평행 (|dot| > BeamAxisParallelDot)
    //   → Cross(WorldUp) ≈ 0 → 정규화 시 NaN. 이 경우 reference axis 를 WorldRight (1,0,0) 로 자동 전환.
    void ComputeBeamLocalAxes(const FVector& Tangent, FVector& OutPerp1, FVector& OutPerp2)
    {
        const FVector WorldUp(0.0f, 0.0f, 1.0f);
        const FVector WorldRight(1.0f, 0.0f, 0.0f);
        const FVector RefAxis = (MathUtil::Abs(Tangent.DotProduct(WorldUp)) > BeamAxisParallelDot)
            ? WorldRight
            : WorldUp;
        OutPerp1 = Tangent.CrossProduct(RefAxis).GetSafeNormal();
        OutPerp2 = Tangent.CrossProduct(OutPerp1).GetSafeNormal();
    }

    // Cycle 13b 분기 1 B-2 + 분기 6 A: per-particle 영구 NoiseSamples 생성.
    // random source cascade: 본 엔진 FEngineRandom 존재하나 singleton + 전역 SetSeed 라서 per-particle
    // deterministic seed 적용 시 다른 시스템의 random 호출과 race. 따라서 local std::mt19937 채택 (cascade fallback).
    // seed source: Particle->ParticleId (base SpawnParticles 에서 ++ParticleCounter 로 instance-wide 유일성).
    //
    // 위험 6 (Noise determinism) 방어 메커니즘: 본 함수가 spawn 시 1회만 호출 → sample 영구 캡처 →
    //   lifetime 동안 변동 0 → frame-rate 비종속. 추가로 같은 ParticleId 면 같은 seed → 머신 간 결정성 보장.
    void GenerateNoiseSamples(FVector* OutSamples, int32 Frequency, uint32 Seed)
    {
        if (!OutSamples)
        {
            return;
        }
        // payload 의 NoiseSamples[BeamNoiseMaxFrequency] 슬롯 모두 zero-init — 사용자가 Frequency 늘려도 garbage 노출 0.
        // (방어 메커니즘: payload 미초기화 garbage 가 BuildVertexBuffer 로 누출되지 않음.)
        for (int32 i = 0; i < BeamNoiseMaxFrequency; ++i)
        {
            OutSamples[i] = FVector::ZeroVector;
        }

        const int32 ClampedFreq = MathUtil::Clamp(Frequency, 0, BeamNoiseMaxFrequency);
        if (ClampedFreq <= 0)
        {
            return;
        }

        std::mt19937 Rng(Seed);
        std::uniform_real_distribution<float> Dist(-1.0f, 1.0f);
        for (int32 i = 0; i < ClampedFreq; ++i)
        {
            OutSamples[i] = FVector(Dist(Rng), Dist(Rng), Dist(Rng));
        }
    }

    // LOD 의 모듈 배열에서 첫 번째 T 타입 모듈 찾기.
    // Cycle 13a 의 Source/Target 모듈 lookup + Cycle 13b 의 Noise 모듈 lookup 에 사용. nullptr 면 모듈 없음.
    template <typename T>
    T* FindFirstModule(UParticleLODLevel* LOD)
    {
        if (!LOD)
        {
            return nullptr;
        }
        for (UParticleModule* Module : LOD->GetModules())
        {
            T* Casted = Cast<T>(Module);
            if (Casted)
            {
                return Casted;
            }
        }
        return nullptr;
    }
}

// Function : Lookup beam payload by physical slot index
// input : SlotIndex (physical slot in ParticleStorage.ParticleData)
// output : pointer to interleaved FParticleBeamPayload, or nullptr when storage not ready / SlotIndex invalid
//
// 위험 1 방어 (진단 §5.3 일반화): SlotIndex 음수 또는 MaxParticles 초과면 nullptr.
FParticleBeamPayload* FParticleBeamEmitterInstance::GetBeamPayload(int32 SlotIndex)
{
    if (!ParticleStorage.ParticleData || SlotIndex < 0 || SlotIndex >= GetMaxActiveParticleCount())
    {
        return nullptr;
    }
    uint8* ParticleBase = ParticleStorage.ParticleData + SlotIndex * ParticleStorage.GetStride();
    return reinterpret_cast<FParticleBeamPayload*>(ParticleBase + PayloadOffset);
}

// Function : Resize BeamStates to MaxBeamCount and reset NextBeamIndex
// input : None
// output : BeamStates.size() == MaxBeamCount, all entries 0, NextBeamIndex = 0 (when re-sized)
//
// Ribbon 의 EnsureTrailState ([ParticleRibbonEmitterInstance.cpp:61-77]) 패턴 답습.
// 첫 Tick / SpawnParticles 진입 시 호출. TypeData 의 MaxBeamCount 가 frame 중 변하지 않는다고 가정.
void FParticleBeamEmitterInstance::EnsureBeamState()
{
    int32 MaxBeams = 1;
    if (UParticleLODLevel* LOD = GetCurrentLODLevel())
    {
        if (const UBeamTypeData* BeamTD = Cast<UBeamTypeData>(LOD->GetTypeDataModule()))
        {
            MaxBeams = std::max(BeamTD->GetMaxBeamCount(), 1);
        }
    }

    if (static_cast<int32>(BeamStates.size()) != MaxBeams)
    {
        BeamStates.assign(MaxBeams, 0);
        NextBeamIndex = 0;
    }
}

// Function : Spawn beam particles — base spawn + BeamIndex round-robin distribution
// input : Count, StartTime, Increment, InitialLocation, InitialVelocity, EventPayload
// output : Base particles spawned + payload BeamIndex 분배
//
// 위험 9 방어: NextBeamIndex = (NextBeamIndex + 1) % MaxBeamCount — overflow 방지.
// 위험 1/4 방어: payload nullptr 검사 + 명시 초기화.
void FParticleBeamEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment,
                                                  const FVector& InitialLocation, const FVector& InitialVelocity,
                                                  FParticleEventInstancePayload* EventPayload)
{
    EnsureBeamState();

    const int32 OldActiveCount = ActiveParticles;
    FParticleEmitterInstance::SpawnParticles(Count, StartTime, Increment, InitialLocation, InitialVelocity, EventPayload);

    const int32 MaxBeams = std::max(static_cast<int32>(BeamStates.size()), 1);

    // Cycle 13b: NoiseModule lookup (1회). 없으면 NoiseSamples zero-init 만 (BuildVertexBuffer 가 perturb 안 함).
    UParticleLODLevel* LOD = GetCurrentLODLevel();
    const UParticleModuleBeamNoise* NoiseModule = FindFirstModule<UParticleModuleBeamNoise>(LOD);
    const int32 NoiseFrequency = NoiseModule ? NoiseModule->GetFrequency() : 0;

    // base 가 spawn 한 신규 particle range [OldActiveCount, ActiveParticles) — payload 초기화 + BeamIndex 분배 + Noise capture.
    for (int32 ActiveIdx = OldActiveCount; ActiveIdx < ActiveParticles; ++ActiveIdx)
    {
        const int32 SlotIndex = static_cast<int32>(ParticleStorage.ParticleIndices[ActiveIdx]);
        FParticleBeamPayload* Payload = GetBeamPayload(SlotIndex);
        if (!Payload)
        {
            continue; // 위험 1 방어
        }

        // 결정 13 옵션 A: BeamIndex round-robin 분배. 위험 9 방어 (MaxBeamCount overflow).
        Payload->BeamIndex = NextBeamIndex;
        NextBeamIndex = (NextBeamIndex + 1) % MaxBeams;

        // 분기 1 B-2 + 분기 6 A (Cycle 13b): NoiseSamples per-particle 영구 캡처.
        // seed = Particle->ParticleId (base SpawnParticles 에서 ++ParticleCounter 로 unique). 결정성 보장.
        // NoiseModule 미존재면 NoiseFrequency=0 → GenerateNoiseSamples 가 모두 zero-init 만 (garbage 노출 0).
        const FBaseParticle* Particle = GetParticle(ActiveIdx);
        const uint32 Seed = Particle ? Particle->ParticleId : static_cast<uint32>(SlotIndex);
        GenerateNoiseSamples(Payload->NoiseSamples, NoiseFrequency, Seed);
    }
}

// Function : Tick beam emitter — base update + Source/Target tracking + strip rebuild
// input : DeltaTime, bAllowSpawning
// output : Particle simulation advanced (base) + VertexBuffer rebuilt with current Source/Target positions
//
// 결정 11 옵션 B (Tick 추적): BuildVertexBuffer 가 매 frame Source/Target Component 의
// GetWorldLocation() 호출 → Component 가 움직이면 beam 끝점 실시간 추적.
void FParticleBeamEmitterInstance::Tick(float DeltaTime, bool bAllowSpawning)
{
    FParticleEmitterInstance::Tick(DeltaTime, bAllowSpawning);

    EnsureBeamState();

    BuildVertexBuffer();
}

// Function : Build strip vertices for all beams — slot 0 dynamic VB source
// input : None
// output : VertexBuffer cleared + filled with 2 vertices per (interpolation point + 1) per active beam
//          + degenerate seams between beams
//
// topology = TRIANGLESTRIP. multi-beam 사이에 마지막 vertex 1개 복제 (degenerate triangle) → strip 연결 끊김.
// Ribbon 의 BuildVertexBuffer ([ParticleRibbonEmitterInstance.cpp:260-308]) 패턴 답습.
//
// 위험 5 방어: Source/Target Component nullptr 시 fallback (Component nullptr 면 emitter 위치 fallback,
//             Target nullptr 면 Source + Forward * FallbackDistance fallback).
// 위험 7 방어: |Target - Source| < epsilon 이면 해당 beam skip (NaN tangent 회피).
// 위험 8 방어: InterpolationPoints clamp(0..BeamInterpolationPointsMax).
void FParticleBeamEmitterInstance::BuildVertexBuffer()
{
    VertexBuffer.clear();

    if (ActiveParticles <= 0)
    {
        return;
    }

    // TypeData / Source / Target 모듈 lookup (frame 단위 1회).
    UParticleLODLevel* LOD = GetCurrentLODLevel();
    const UBeamTypeData* BeamTD = LOD ? Cast<UBeamTypeData>(LOD->GetTypeDataModule()) : nullptr;
    if (!BeamTD)
    {
        return;
    }

    UParticleModuleBeamSource* SourceModule = FindFirstModule<UParticleModuleBeamSource>(LOD);
    UParticleModuleBeamTarget* TargetModule = FindFirstModule<UParticleModuleBeamTarget>(LOD);

    // Cycle 13b: NoiseModule lookup (frame 단위 1회). 미존재면 perturbation 분기 자체 스킵 (Cycle 13a 동작 그대로 — 회귀 안전).
    const UParticleModuleBeamNoise* NoiseModule = FindFirstModule<UParticleModuleBeamNoise>(LOD);
    const int32 NoiseFrequency = NoiseModule ? MathUtil::Clamp(NoiseModule->GetFrequency(), 0, BeamNoiseMaxFrequency) : 0;
    const FVector NoiseRange = NoiseModule ? NoiseModule->GetNoiseRange() : FVector::ZeroVector;
    const bool bTargetNoise = NoiseModule ? NoiseModule->IsTargetNoise() : false;
    const bool bSmooth = NoiseModule ? NoiseModule->IsSmooth() : false;
    const bool bApplyNoise = (NoiseModule != nullptr) && (NoiseFrequency > 0);

    // 위험 5 방어: Source/Target Component nullptr 시 fallback 위치 계산용 emitter 위치 + forward.
    // emitter 위치는 base instance 의 GetComponentWorldLocation() 사용 (Sprite/Ribbon 와 동일 출처).
    USceneComponent* SourceComp = SourceModule ? SourceModule->GetSourceComponent() : nullptr;
    USceneComponent* TargetComp = TargetModule ? TargetModule->GetTargetComponent() : nullptr;

    const FVector EmitterLocation = GetComponentWorldLocation();
    const UParticleSystemComponent* OwningComp = GetOwningComponent();
    const FVector EmitterForward = OwningComp ? OwningComp->GetForwardVector() : FVector(1.0f, 0.0f, 0.0f);

    const FVector SourceLocation = SourceComp ? SourceComp->GetWorldLocation() : EmitterLocation;

    // 위험 8 방어: UPROPERTY Min/Max 가 1차, 본 clamp 가 2차 방어.
    const int32 InterpCount = std::clamp(BeamTD->GetInterpolationPoints(), 0, BeamInterpolationPointsMax);
    // strip 정점 segment 수 (InterpCount=0 이면 Source→Target 직접 2 정점, InterpCount=N 이면 N+2 정점).
    const int32 SegmentCount = InterpCount + 1;

    const float TextureTile = std::max(BeamTD->GetTextureTile(), 0.0f);
    const float TextureTileDistance = std::max(BeamTD->GetTextureTileDistance(), 0.0f);

    // 각 active particle = 1 beam (1:1). Source/Target 위치는 emitter-wide (모든 beam 공유 — 13a 단순화).
    // 결정 13 옵션 A (multi-beam) 의 per-beam 시각 차이는 Cycle 13b 의 Noise 도입으로 가시화됨 (per-particle 영구 sample 이 ParticleId 기반 결정성).
    for (int32 ActiveIdx = 0; ActiveIdx < ActiveParticles; ++ActiveIdx)
    {
        const FBaseParticle* Particle = GetParticle(ActiveIdx);
        if (!Particle)
        {
            continue;
        }

        // Target 결정: TargetComp 있으면 그 위치, 없으면 Source + EmitterForward * FallbackDistance (결정 15 B fallback).
        FVector TargetLocation;
        if (TargetComp)
        {
            TargetLocation = TargetComp->GetWorldLocation();
        }
        else
        {
            TargetLocation = SourceLocation + EmitterForward * BeamTD->GetFallbackDistance();
        }

        // 위험 7 방어: zero-length beam 회피 (NaN tangent → invalid geometry).
        const FVector BeamVector = TargetLocation - SourceLocation;
        const float BeamLength = BeamVector.Size();
        if (BeamLength < BeamSmallNumber)
        {
            continue;
        }

        const FVector Tangent = BeamVector / BeamLength;
        const FVector Perp = ComputePerpendicular(Tangent);
        const float HalfSize = Particle->Size.X * 0.5f;

        // Cycle 13b 분기 3 B: Beam-local 좌표축 (Tangent + Perp1 + Perp2). 위험 11 방어는 helper 내부.
        // NoiseModule 미존재면 Perp1/Perp2 사용되지 않음 — 계산 생략 가능하나 분기 단순화 위해 항상 계산.
        FVector Perp1 = FVector::ZeroVector;
        FVector Perp2 = FVector::ZeroVector;
        const FParticleBeamPayload* NoisePayload = nullptr;
        if (bApplyNoise)
        {
            ComputeBeamLocalAxes(Tangent, Perp1, Perp2);
            // payload 의 NoiseSamples 접근 — SlotIndex 기반 (Cycle 13a 의 GetBeamPayload 패턴).
            // const_cast 회피 위해 GetBeamPayload 호출 — 본 함수는 non-const 이므로 OK.
            const int32 SlotIndex = static_cast<int32>(ParticleStorage.ParticleIndices[ActiveIdx]);
            NoisePayload = GetBeamPayload(SlotIndex);
        }

        // 각 strip segment 의 boundary point (총 SegmentCount+1 = InterpCount+2 개) 마다 2 vertex (V0/V1) 생성.
        const size_t BeamStartCount = VertexBuffer.size();
        for (int32 SegIdx = 0; SegIdx <= SegmentCount; ++SegIdx)
        {
            const float Alpha = static_cast<float>(SegIdx) / static_cast<float>(SegmentCount);
            FVector CenterPos = SourceLocation + BeamVector * Alpha;
            const float AccumDist = BeamLength * Alpha;

            // Cycle 13b: Noise perturbation 적용.
            //   - SegIdx == 0 (Source 끝점): 항상 noise 0 (Source 는 고정).
            //   - SegIdx == SegmentCount (Target 끝점) && !bTargetNoise (분기 4 A): noise 0 (Target 고정).
            //   - 그 외 중간점: NoiseSamples lookup + WorldOffset 적용.
            if (bApplyNoise && NoisePayload && NoiseFrequency > 0 &&
                SegIdx > 0 && !(SegIdx == SegmentCount && !bTargetNoise))
            {
                // Frequency 개 sample 을 strip 전체 (Alpha 0~1) 에 매핑.
                // Alpha 가 [0, 1] → sample index float = Alpha * (Frequency - 1) 범위 [0, Frequency-1].
                const float SampleIdxF = Alpha * static_cast<float>(NoiseFrequency - 1);
                FVector Sample;
                if (bSmooth && NoiseFrequency >= 2)
                {
                    // linear interp 사이 인접 2 sample.
                    const int32 IdxLo = std::clamp(static_cast<int32>(std::floor(SampleIdxF)), 0, NoiseFrequency - 1);
                    const int32 IdxHi = std::clamp(IdxLo + 1, 0, NoiseFrequency - 1);
                    const float Frac = SampleIdxF - static_cast<float>(IdxLo);
                    Sample = NoisePayload->NoiseSamples[IdxLo] * (1.0f - Frac) + NoisePayload->NoiseSamples[IdxHi] * Frac;
                }
                else
                {
                    // nearest sample.
                    const int32 Idx = std::clamp(static_cast<int32>(std::round(SampleIdxF)), 0, NoiseFrequency - 1);
                    Sample = NoisePayload->NoiseSamples[Idx];
                }

                // 분기 3 B: Beam-local 좌표 → World offset.
                //   sample.X * range.X * Tangent + sample.Y * range.Y * Perp1 + sample.Z * range.Z * Perp2
                const FVector WorldOffset =
                    Tangent * (Sample.X * NoiseRange.X) +
                    Perp1   * (Sample.Y * NoiseRange.Y) +
                    Perp2   * (Sample.Z * NoiseRange.Z);
                CenterPos = CenterPos + WorldOffset;
            }

            // UV.U: TextureTileDistance > 0 이면 distance 기반 누적 반복, 아니면 stretch (Alpha * TextureTile).
            const float TexU = (TextureTileDistance > BeamSmallNumber)
                ? (AccumDist / TextureTileDistance)
                : (Alpha * TextureTile);

            FBeamParticleVertex V0;
            V0.Position = CenterPos + Perp * HalfSize;
            V0.Tangent = Tangent;
            V0.Color = Particle->Color;
            V0.TexCoordU = TexU;
            V0.Size = Particle->Size.X;
            VertexBuffer.push_back(V0);

            FBeamParticleVertex V1 = V0;
            V1.Position = CenterPos - Perp * HalfSize;
            VertexBuffer.push_back(V1);
        }

        // multi-beam seam — 다음 active particle 이 존재하고 현재 beam 이 vertex 생성했다면 마지막 vertex 복제 (degenerate).
        // Ribbon 의 동일 패턴 ([ParticleRibbonEmitterInstance.cpp:301-306]).
        if (VertexBuffer.size() > BeamStartCount &&
            ActiveIdx + 1 < ActiveParticles)
        {
            VertexBuffer.push_back(VertexBuffer.back());
        }
    }
}

// Function : Expose beam vertex buffer to Builder (Beam path override)
// input : OutCount (out-param)
// output : Pointer to first element + count, or nullptr/0 when empty
const FBeamParticleVertex* FParticleBeamEmitterInstance::GetBeamVertexData(uint32& OutCount) const
{
    OutCount = static_cast<uint32>(VertexBuffer.size());
    return VertexBuffer.empty() ? nullptr : VertexBuffer.data();
}
