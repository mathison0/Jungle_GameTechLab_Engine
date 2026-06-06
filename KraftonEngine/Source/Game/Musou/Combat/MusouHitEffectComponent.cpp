#include "Game/Musou/Combat/MusouHitEffectComponent.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Particles/ParticleSystemManager.h"

#include <algorithm>

void UMusouHitEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HitParticlePath.IsNull())
	{
		CachedHitParticle = FParticleSystemManager::Get().Load(HitParticlePath.ToString());
	}

	UWorld* World = GetWorld();
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	if (GameMode)
	{
		HitListenerHandle = GameMode->OnHitConfirmed.AddUObject(this, &UMusouHitEffectComponent::HandleHitConfirmed);
	}
}

void UMusouHitEffectComponent::EndPlay()
{
	if (HitListenerHandle.IsValid())
	{
		UWorld* World = GetWorld();
		AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
		if (GameMode)
		{
			GameMode->OnHitConfirmed.Remove(HitListenerHandle);
		}
		HitListenerHandle.Reset();
	}

	PendingHits.clear();

	for (FPooledHitParticle& Pooled : ParticlePool)
	{
		if (Pooled.Component)
		{
			Pooled.Component->Deactivate();
		}
	}

	Super::EndPlay();
}

void UMusouHitEffectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateActiveParticles(DeltaTime);
	FlushHits(DeltaTime);
}

void UMusouHitEffectComponent::HandleHitConfirmed(const FMusouHitEvent& Hit)
{
	PendingHits.push_back(Hit);
}

void UMusouHitEffectComponent::FlushHits(float DeltaTime)
{
	if (PendingHits.empty()) return;
	
	const int32 Budget = std::max(0, MaxHitParticles);
	const int32 PlayCount = std::min(static_cast<int32>(PendingHits.size()), Budget);

	for (int32 i = 0; i < PlayCount; ++i)
	{
		PlayHitParticle(PendingHits[i]);
	}

	PendingHits.clear();
}

UParticleSystemComponent* UMusouHitEffectComponent::AcquireParticle()
{
	for (FPooledHitParticle& Pooled : ParticlePool)
	{
		if (!Pooled.bInUse && Pooled.Component)
		{
			Pooled.bInUse = true;
			Pooled.RemainingLife = ParticleLife;
			return Pooled.Component;
		}
	}

	if (static_cast<int32>(ParticlePool.size()) >= PoolSize) return nullptr;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return nullptr;

	UParticleSystemComponent* Component = OwnerActor->AddComponent<UParticleSystemComponent>();
	if (!Component) return nullptr;

	Component->SetTemplate(CachedHitParticle);
	Component->Deactivate();

	FPooledHitParticle NewEntry;
	NewEntry.Component = Component;
	NewEntry.RemainingLife = ParticleLife;
	NewEntry.bInUse = true;
	ParticlePool.push_back(NewEntry);

	return Component;
}

void UMusouHitEffectComponent::PlayHitParticle(const FMusouHitEvent& Hit)
{
	if (!CachedHitParticle) return;
	
	UParticleSystemComponent* Particle = AcquireParticle();
	if (!Particle) return;

	FVector Location = Hit.HitLocation;
	Location.Z += HitHeightOffset;

	Particle->SetWorldLocation(Location);
	Particle->ResetSystem();
	Particle->Activate();
}

void UMusouHitEffectComponent::UpdateActiveParticles(float DeltaTime)
{
	for (FPooledHitParticle& Pooled : ParticlePool)
	{
		if (!Pooled.bInUse) continue;

		Pooled.RemainingLife -= DeltaTime;
		if (Pooled.RemainingLife <= 0.0f)
		{
			Pooled.bInUse = false;
			Pooled.RemainingLife = 0.0f;

			if (Pooled.Component)
			{
				Pooled.Component->Deactivate();
			}
		}
	}
}
