# Particle Module Execution Order

This document records the first-pass Particle System execution contract.

## Emitter Tick

1. Select the active `UParticleLODLevel`.
2. Let `UParticleModuleSpawn` compute the frame spawn count.
3. Call `FParticleEmitterInstance::SpawnParticles`.
4. Advance `FBaseParticle::RelativeTime`.
5. Kill expired particles.
6. Store `OldLocation`.
7. Integrate `Location += Velocity * DeltaTime`.
8. Run update modules.
9. Queue collision events.
10. Dispatch queued events through the event generator or component delegate.

## Spawn Module Order

1. `UParticleModuleRequired`
2. `UParticleModuleLifetime`
3. `UParticleModuleLocation`
4. `UParticleModuleVelocity`
5. `UParticleModuleColor`
6. `UParticleModuleSize`

## Update Module Order

1. `UParticleModuleColor`
2. `UParticleModuleSize`
3. `UParticleModuleCollision`
4. `UParticleModuleEventGenerator`

## Minimal Implementation Notes

- Collision currently uses a simple configurable Z plane so tests can exercise
  `FParticleEventCollideData` without depending on world collision.
- Rendering is intentionally exposed through `UParticleSystemComponent` runtime
  data and `EPrimitiveType::EPT_ParticleSystem`; sprite, mesh, beam, and ribbon
  submission can be implemented by the rendering owner on top of this data.
