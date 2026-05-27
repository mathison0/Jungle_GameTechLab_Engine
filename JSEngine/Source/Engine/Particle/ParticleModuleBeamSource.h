#pragma once

#include "Component/SceneComponent.h"
#include "Math/Vector.h"
#include "Object/ObjectPtr.h"
#include "Particle/ParticleModule.h"

// Beam emitter 의 Source 위치 공급 모듈 (Cycle 13a, 결정 10 옵션 A + 결정 11 옵션 B).
// Source 가 set 되면 SourceComponent->GetWorldLocation() 으로 매 frame 추적 (instance Tick override).
// 미설정 (nullptr) 시 Beam instance 는 owning particle component 의 위치를 fallback 으로 사용.
//
// 결정 10 옵션 A 채택 이유:
//   - 본 엔진 reflection 에 AActor* UPROPERTY 패턴 0건, ReferenceKind enum 에 Actor 없음 (REFLECTION_GUIDE.md §2.2).
//   - 검증된 TObjectPtr<USceneComponent> 패턴 (MovementComponent::UpdatedComponent) 답습 → picker UI 자동.
//   - actor 의 root scene component 를 picker 로 선택 → GetWorldLocation() 으로 actor 위치 추적 가능.
//
// Tangent 확장 (Hermite 곡선 control): SourceTangent + SourceTangentStrength.
// emitter-local 단위 방향 * strength → world tangent. Strength=0 이면 linear fallback (BuildFromInstance 분기).
UCLASS()
class UParticleModuleBeamSource : public UParticleModule
{
public:
    GENERATED_BODY(UParticleModuleBeamSource, UParticleModule)

    USceneComponent* GetSourceComponent() const { return SourceComponent.Get(); }
    void SetSourceComponent(USceneComponent* InComponent) { SourceComponent.Set(InComponent); }

    FVector GetSourceTangent() const                  { return SourceTangent; }
    void    SetSourceTangent(const FVector& In)       { SourceTangent = In; }

    float   GetSourceTangentStrength() const          { return SourceTangentStrength; }
    void    SetSourceTangentStrength(float In)        { SourceTangentStrength = In; }

private:
    // TObjectPtr<USceneComponent> 는 reflection 생성기가 자동으로 EObjectReferenceKind::ActorComponent 로 등록.
    // 명시 ReferenceKind 불요 (MovementComponent.gen.cpp 의 UpdatedComponent 처럼 자동 도출).
    UPROPERTY(DisplayName = "Source Component", Category = "Beam Source")
    TObjectPtr<USceneComponent> SourceComponent;

    // emitter-local 단위 방향 (Forward/Right/Up 기반). bUseLocalTarget 의 TargetLocalVector 패턴과 동일 좌표계.
    // Default (1,0,0): emitter forward — strength=0 일 때 의미 없으므로 임의 default.
    UPROPERTY(DisplayName = "Source Tangent", Category = "Beam Source")
    FVector SourceTangent = FVector(1.0f, 0.0f, 0.0f);

    // Hermite tangent 의 크기 (cm). 0 이면 linear fallback (TargetTangentStrength 도 0 일 때).
    UPROPERTY(DisplayName = "Source Tangent Strength", Category = "Beam Source", Min = 0.0f)
    float SourceTangentStrength = 0.0f;
};
