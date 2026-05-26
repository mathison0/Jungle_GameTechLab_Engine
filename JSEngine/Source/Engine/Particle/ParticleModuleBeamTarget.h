#pragma once

#include "Component/SceneComponent.h"
#include "Object/ObjectPtr.h"
#include "Particle/ParticleModule.h"

// Beam emitter 의 Target 위치 공급 모듈 (Cycle 13a, 결정 10 옵션 A + 결정 11 옵션 B + 결정 15 옵션 B).
// Target 이 set 되면 TargetComponent->GetWorldLocation() 으로 매 frame 추적.
// 미설정 (nullptr) 시 Beam instance 는 Source + Forward * UBeamTypeData::FallbackDistance 로 fallback
// (PEB2M_Target only 채택 — PEB2M_Distance 분기는 본 fallback 으로 흡수).
//
// UParticleModuleBeamSource 와 패턴 1:1 — 코드 중복은 의도된 단순성 (각 모듈이 독립 UCLASS).
UCLASS()
class UParticleModuleBeamTarget : public UParticleModule
{
public:
    GENERATED_BODY(UParticleModuleBeamTarget, UParticleModule)

    USceneComponent* GetTargetComponent() const { return TargetComponent.Get(); }
    void SetTargetComponent(USceneComponent* InComponent) { TargetComponent.Set(InComponent); }

private:
    UPROPERTY(DisplayName = "Target Component", Category = "Beam Target")
    TObjectPtr<USceneComponent> TargetComponent;
};
