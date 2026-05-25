#pragma once

#include "Object/FName.h"
#include "Particle/ParticleModule.h"
#include "Render/Resource/Material.h"

#include <algorithm>

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleRequired, UParticleModule)

	UParticleModuleRequired();
	void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

	int32 GetMaxParticles() const { return MaxParticles; }
	float GetEmitterDuration() const { return EmitterDuration; }
	bool IsLooping() const { return bLooping; }
	bool UseLocalSpace() const { return bUseLocalSpace; }
	UMaterialInterface* GetMaterial() const { return Material; }
	const FName& GetSubUVName() const { return SubUVName; }
	int32 GetSubImagesHorizontal() const { return std::max(SubImagesHorizontal, 1); }
	int32 GetSubImagesVertical() const { return std::max(SubImagesVertical, 1); }
	EParticleEmitterRenderMode GetRenderMode() const { return RenderMode; }

private:
	UPROPERTY(DisplayName = "Material", Category = "Emitter", ReferenceKind = Asset)
	UMaterialInterface* Material = nullptr;

	UPROPERTY(DisplayName = "Max Particles", Min = 1)
	int32 MaxParticles = 128;

	UPROPERTY(DisplayName = "Emitter Duration", Min = 0.0f)
	float EmitterDuration = 1.0f;

	UPROPERTY(DisplayName = "Looping")
	bool bLooping = true;

	UPROPERTY(DisplayName = "Use Local Space")
	bool bUseLocalSpace = false;

	UPROPERTY(DisplayName = "SubUV", Category = "Sub UV")
	FName SubUVName;

	UPROPERTY(DisplayName = "Sub Images Horizontal", Category = "Sub UV", Min = 1)
	int32 SubImagesHorizontal = 1;

	UPROPERTY(DisplayName = "Sub Images Vertical", Category = "Sub UV", Min = 1)
	int32 SubImagesVertical = 1;

	EParticleEmitterRenderMode RenderMode = EParticleEmitterRenderMode::Sprite;
};

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSpawn, UParticleModule)

	UParticleModuleSpawn();
	int32 ComputeSpawnCount(FParticleEmitterInstance* Owner, float DeltaTime);

private:
	UPROPERTY(DisplayName = "Rate", Min = 0.0f)
	float Rate = 10.0f;
};

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLifetime, UParticleModule)

	UParticleModuleLifetime();
	void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

private:
	UPROPERTY(DisplayName = "Lifetime Min", Min = 0.01f)
	float LifetimeMin = 1.0f;

	UPROPERTY(DisplayName = "Lifetime Max", Min = 0.01f)
	float LifetimeMax = 1.0f;
};

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleLocation, UParticleModule)

	UParticleModuleLocation();
	void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

private:
	UPROPERTY(DisplayName = "Start Location Min")
	FVector StartLocationMin = FVector::ZeroVector;

	UPROPERTY(DisplayName = "Start Location Max")
	FVector StartLocationMax = FVector::ZeroVector;
};

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleVelocity, UParticleModule)

	UParticleModuleVelocity();
	void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

private:
	UPROPERTY(DisplayName = "Start Velocity Min")
	FVector StartVelocityMin = FVector(0.0f, 0.0f, 50.0f);

	UPROPERTY(DisplayName = "Start Velocity Max")
	FVector StartVelocityMax = FVector(0.0f, 0.0f, 100.0f);
};

UCLASS()
class UParticleModuleColor : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleColor, UParticleModule)

	UParticleModuleColor();
	void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;
	void Update(FParticleEmitterInstance* Owner, float DeltaTime) override;

private:
	UPROPERTY(DisplayName = "Start Color")
	FColor StartColor = FColor::White();

	UPROPERTY(DisplayName = "End Color")
	FColor EndColor = FColor(1.0f, 1.0f, 1.0f, 0.0f);
};

UCLASS()
class UParticleModuleSize : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleSize, UParticleModule)

	UParticleModuleSize();
	void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;
	void Update(FParticleEmitterInstance* Owner, float DeltaTime) override;

private:
	UPROPERTY(DisplayName = "Start Size")
	FVector StartSize = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(DisplayName = "End Size")
	FVector EndSize = FVector(1.0f, 1.0f, 1.0f);
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleCollision, UParticleModule)

	UParticleModuleCollision();
	void Update(FParticleEmitterInstance* Owner, float DeltaTime) override;

private:
	UPROPERTY(DisplayName = "Plane Z")
	float CollisionPlaneZ = 0.0f;

	UPROPERTY(DisplayName = "Restitution", Min = 0.0f, Max = 1.0f)
	float Restitution = 0.25f;

	UPROPERTY(DisplayName = "Kill On Collision")
	bool bKillOnCollision = false;

	UPROPERTY(DisplayName = "Generate Events")
	bool bGenerateCollisionEvents = true;
};

UCLASS()
class UParticleModuleEventGenerator : public UParticleModule
{
public:
	GENERATED_BODY(UParticleModuleEventGenerator, UParticleModule)

	UParticleModuleEventGenerator();
	void Update(FParticleEmitterInstance* Owner, float DeltaTime) override;
};
