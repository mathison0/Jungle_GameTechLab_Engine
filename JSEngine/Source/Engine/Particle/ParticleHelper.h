#pragma once

#include "Particle/ParticleTypes.h"

#define PARTICLE_PTR(Owner, ActiveIndex) \
	reinterpret_cast<FBaseParticle*>((Owner)->ParticleData + \
		(Owner)->ParticleIndices[(ActiveIndex)] * (Owner)->ParticleStride)

#define DECLARE_PARTICLE_PTR \
	FBaseParticle& Particle = *PARTICLE_PTR(Owner, ParticleIndex)

#define BEGIN_UPDATE_LOOP \
	for (int32 ParticleIndex = 0; ParticleIndex < Owner->ActiveParticles; )

#define END_UPDATE_LOOP \
	++ParticleIndex

inline FBaseParticle* GetParticleDirect(FParticleEmitterInstance* Owner, int32 ActiveIndex)
{
	return PARTICLE_PTR(Owner, ActiveIndex);
}
