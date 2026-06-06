#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Object/Ptr/SoftObjectPtr.h"

#include "Game/Musou/Combat/HitTypes.h"

#include "Source/Game/Musou/Combat/MusouHitEffectComponent.generated.h"

class UParticleSystem;
class UParticleSystemComponent;

UCLASS()
class UMusouHitEffectComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	void BeginPlay() override;
	void EndPlay() override;

protected:
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	struct FPooledHitParticle
	{
		UParticleSystemComponent* Component = nullptr;
		float RemainingLife = 0.0f;
		bool bInUse = false;
	};

	void HandleHitConfirmed(const FMusouHitEvent& Hit);
	void FlushHits(float DeltaTime);

	UParticleSystemComponent* AcquireParticle();
	void PlayHitParticle(const FMusouHitEvent& Hit);
	void UpdateActiveParticles(float DeltaTime);

private:
	TArray<FMusouHitEvent> PendingHits;
	TArray<FPooledHitParticle> ParticlePool;
	FDelegateHandle HitListenerHandle;

	UParticleSystem* CachedHitParticle = nullptr;

	UPROPERTY(Edit, Save, Category="HitEffect", DisplayName="Hit Particle", AssetType="UParticleSystem")
	FSoftObjectPtr HitParticlePath = "Content/Particle/HitParticle.uasset";

	UPROPERTY(Edit, Save, Category="HitEffect", DisplayName="Max Hit Particles")
	int32 MaxHitParticles = 16;

	UPROPERTY(Edit, Save, Category="HitEffect", DisplayName="Pool Size")
	int32 PoolSize = 24;

	UPROPERTY(Edit, Save, Category="HitEffect", DisplayName="Particle Life")
	float ParticleLife = 0.35f;

	UPROPERTY(Edit, Save, Category="HitEffect", DisplayName="Hit Height Offset")
	float HitHeightOffset = 0.8f;
};
