#pragma once

#include "Particle/ParticleEmitterInstance.h"

#define PARTICLE_PTR(Owner, ActiveIndex) \
	((Owner)->GetParticle(ActiveIndex))

#define DECLARE_PARTICLE_PTR \
	FBaseParticle& Particle = *PARTICLE_PTR(Owner, ParticleIndex)

#define BEGIN_UPDATE_LOOP \
	for (int32 ParticleIndex = 0; ParticleIndex < Owner->GetActiveParticleCount(); )

#define END_UPDATE_LOOP \
	++ParticleIndex

// Function : Get particle data directly from emitter instance storage
// input : Owner, ActiveIndex
// Owner : emitter instance that owns particle data and index buffers
// ActiveIndex : active particle index in compact active list
// output : Pointer to particle data stored at the active index
inline FBaseParticle* GetParticleDirect(FParticleEmitterInstance* Owner, int32 ActiveIndex)
{
	return Owner ? Owner->GetParticle(ActiveIndex) : nullptr;
}
