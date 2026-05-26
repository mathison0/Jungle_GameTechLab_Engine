#pragma once

#include "Component/SceneComponent.h"
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
UCLASS()
class UParticleModuleBeamSource : public UParticleModule
{
public:
    GENERATED_BODY(UParticleModuleBeamSource, UParticleModule)

    USceneComponent* GetSourceComponent() const { return SourceComponent.Get(); }
    void SetSourceComponent(USceneComponent* InComponent) { SourceComponent.Set(InComponent); }

private:
    // TObjectPtr<USceneComponent> 는 reflection 생성기가 자동으로 EObjectReferenceKind::ActorComponent 로 등록.
    // 명시 ReferenceKind 불요 (MovementComponent.gen.cpp 의 UpdatedComponent 처럼 자동 도출).
    UPROPERTY(DisplayName = "Source Component", Category = "Beam Source")
    TObjectPtr<USceneComponent> SourceComponent;
};
