#include "Particle/ParticleEvent.h"

void AParticleEventManager::PushCollisionEvent(const FParticleEventCollideData& EventData)
{
	CollisionEvents.push_back(EventData);
}

void AParticleEventManager::DispatchEvents()
{
	for (const FParticleEventCollideData& EventData : CollisionEvents)
	{
		OnParticleCollide.Broadcast(EventData);
	}
	CollisionEvents.clear();
}
