#include "Particle/ParticleSystemComponent.h"

#include "Camera/ViewportCamera.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

UParticleSystemComponent::~UParticleSystemComponent()
{
	ClearEmitterInstances();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (Template == InTemplate)
	{
		return;
	}

	Template = InTemplate;
	RecreateEmitterInstances();
}

void UParticleSystemComponent::RecreateEmitterInstances()
{
	ClearEmitterInstances();
	if (!Template)
	{
		return;
	}

	const TArray<UParticleEmitter*>& Emitters = Template->GetEmitters();
	EmitterInstances.reserve(Emitters.size());
	for (int32 Index = 0; Index < static_cast<int32>(Emitters.size()); ++Index)
	{
		FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
		Instance->Init(Emitters[Index], this, Index);
		EmitterInstances.push_back(Instance);
	}
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}
	EmitterInstances.clear();
	PendingCollisionEvents.clear();
}

float UParticleSystemComponent::ComputeEmitterLODDistance() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = OwnerActor ? OwnerActor->GetFocusedWorld() : nullptr;
	if (!World || !World->GetActiveCamera())
	{
		return 0.0f;
	}

	return FVector::Dist(GetWorldLocation(), World->GetActiveCamera()->GetLocation());
}

void UParticleSystemComponent::QueueCollisionEvent(const FParticleEventCollideData& EventData)
{
	PendingCollisionEvents.push_back(EventData);
}

void UParticleSystemComponent::DispatchQueuedParticleEvents()
{
	for (const FParticleEventCollideData& EventData : PendingCollisionEvents)
	{
		OnParticleCollide.Broadcast(EventData);
	}
	PendingCollisionEvents.clear();
}

void UParticleSystemComponent::UpdateWorldAABB() const
{
	WorldAABB.Reset();
	const FVector ComponentLocation = GetWorldLocation();
	WorldAABB.Expand(ComponentLocation - FVector(100.0f, 100.0f, 100.0f));
	WorldAABB.Expand(ComponentLocation + FVector(100.0f, 100.0f, 100.0f));

	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
		{
			continue;
		}

		for (int32 ParticleIndex = 0; ParticleIndex < Instance->ActiveParticles; ++ParticleIndex)
		{
			const FBaseParticle* Particle = Instance->GetParticle(ParticleIndex);
			if (Particle)
			{
				WorldAABB.Expand(Particle->Location);
			}
		}
	}
}

bool UParticleSystemComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	(void)Ray;
	OutHitResult.Reset();
	return false;
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->Tick(DeltaTime);
		}
	}
	NotifySpatialIndexDirty();
}
