#include "Game/Musou/Effect/SlashEffectActor.h"

#include "Component/Primitive/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Render/Pipeline/Renderer.h"

void ASlashEffectActor::InitDefaultComponents()
{
	SlashRootComponent = AddComponent<USceneComponent>();
	SetRootComponent(SlashRootComponent);

	CoreMeshComponent = AddComponent<UStaticMeshComponent>();
	CoreMeshComponent->AttachToComponent(SlashRootComponent);

	GlowMeshComponent = AddComponent<UStaticMeshComponent>();
	GlowMeshComponent->AttachToComponent(SlashRootComponent);

	RefractionMeshComponent = AddComponent<UStaticMeshComponent>();
	RefractionMeshComponent->AttachToComponent(SlashRootComponent);

	LoadSlashAssets();

	SetVisible(false);
	bActive = false;
}

void ASlashEffectActor::BeginPlay()
{
	Super::BeginPlay();

	if (!CoreMeshComponent || !GlowMeshComponent)
	{
		ResolveComponents();
	}

	LoadSlashAssets();
	SetVisible(false);
}

void ASlashEffectActor::LoadSlashAssets()
{
	if (!GEngine)
	{
		return;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = FMeshManager::LoadStaticMesh(MeshPath, Device);

	CoreMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(CoreMaterialPath);
	GlowMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(GlowMaterialPath);

	if (CoreMeshComponent)
	{
		CoreMeshComponent->SetStaticMesh(Mesh);
		CoreMeshComponent->SetMaterial(0, CoreMaterial);
		CoreMeshComponent->SetRelativeScale(CoreRelativeScale);
	}

	if (GlowMeshComponent)
	{
		GlowMeshComponent->SetStaticMesh(Mesh);
		GlowMeshComponent->SetMaterial(0, GlowMaterial);
		GlowMeshComponent->SetRelativeScale(GlowRelativeScale);
	}

	RefractionMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(RefractionMaterialPath);

	if (RefractionMeshComponent)
	{
		RefractionMeshComponent->SetStaticMesh(Mesh);
		RefractionMeshComponent->SetMaterial(0, RefractionMaterial);
		RefractionMeshComponent->SetRelativeScale(RefractionRelativeScale);
	}
}

void ASlashEffectActor::ActivateSlash(
	const FVector& Location,
	const FVector& Rotation,
	const FVector& Direction)
{
	SetActorLocation(Location);
	SetActorRotation(Rotation + RotationOffset);
	SetActorScale(StartScale);

	if (CoreMeshComponent)
	{
		CoreMeshComponent->SetRelativeScale(CoreRelativeScale);
	}

	if (GlowMeshComponent)
	{
		GlowMeshComponent->SetRelativeScale(GlowRelativeScale);
	}

	MoveDirection = Direction.Normalized();
	Age = 0.0f;
	bActive = true;

	SetSlashMaterialParams(1.0f, GlowAlphaMultiplier, 0.0f, 0.0f);
	SetVisible(true);
}

void ASlashEffectActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActive)
	{
		return;
	}

	Age += DeltaTime;

	const float T = Lifetime > 0.0f
		? FMath::Clamp(Age / Lifetime, 0.0f, 1.0f)
		: 1.0f;

	FVector CurrentScale;
	if (T < 0.25f)
	{
		const float A = T / 0.25f;
		CurrentScale = StartScale + (PeakScale - StartScale) * A;
	}
	else
	{
		const float A = (T - 0.25f) / 0.75f;
		CurrentScale = PeakScale + (EndScale - PeakScale) * A;
	}

	float BaseAlpha = 1.0f;
	if (T < 0.12f)
	{
		BaseAlpha = T / 0.12f;
	}
	else
	{
		BaseAlpha = 1.0f - ((T - 0.12f) / 0.88f);
	}
	BaseAlpha = FMath::Clamp(BaseAlpha, 0.0f, 1.0f);

	const float CoreAlpha = FMath::Clamp(BaseAlpha * CoreFadeOutSpeed, 0.0f, 1.0f);

	float GlowAlpha = BaseAlpha * GlowFadeOutSpeed + 0.18f;
	if (T > 0.88f)
	{
		GlowAlpha *= 1.0f - ((T - 0.88f) / 0.12f);
	}
	GlowAlpha = FMath::Clamp(GlowAlpha, 0.0f, 1.0f) * GlowAlphaMultiplier;

	float Dissolve = 0.0f;
	if (T > DissolveStartTime)
	{
		const float D = (T - DissolveStartTime) / (1.0f - DissolveStartTime);
		Dissolve = FMath::Clamp(D, 0.0f, 1.0f) * DissolveEndValue;
	}

	const float NoiseScroll = Age * NoiseScrollSpeed;

	SetActorScale(CurrentScale);
	SetSlashMaterialParams(CoreAlpha, GlowAlpha, Dissolve, NoiseScroll);

	if (MoveSpeed > 0.0f)
	{
		AddActorWorldOffset(MoveDirection * MoveSpeed * DeltaTime);
	}

	if (Age >= Lifetime)
	{
		FinishSlash();
	}
}

void ASlashEffectActor::FinishSlash()
{
	bActive = false;
	Age = 0.0f;

	SetSlashAlpha(0.0f);
	SetVisible(false);

	if (bDestroyOnFinish)
	{
		if (UWorld* World = GetWorld())
		{
			World->DestroyActor(this);
		}
	}
}

void ASlashEffectActor::PostDuplicate()
{
	Super::PostDuplicate();

	ResolveComponents();
	LoadSlashAssets();
}

void ASlashEffectActor::PostLoad()
{
	Super::PostLoad();

	ResolveComponents();
	LoadSlashAssets();
}

void ASlashEffectActor::SetSlashAlpha(float Alpha)
{
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	if (CoreMaterial)
	{
		CoreMaterial->SetScalarParameter("SlashAlpha", Alpha);
	}

	if (GlowMaterial)
	{
		GlowMaterial->SetScalarParameter("SlashAlpha", Alpha * GlowAlphaMultiplier);
	}
}

void ASlashEffectActor::SetSlashMaterialParams(
	float CoreAlpha,
	float GlowAlpha,
	float Dissolve,
	float NoiseScroll)
{
	CoreAlpha = FMath::Clamp(CoreAlpha, 0.0f, 1.0f);
	GlowAlpha = FMath::Clamp(GlowAlpha, 0.0f, 1.0f);
	Dissolve = FMath::Clamp(Dissolve, 0.0f, 1.0f);

	if (CoreMaterial)
	{
		CoreMaterial->SetScalarParameter("SlashAlpha", CoreAlpha);
		CoreMaterial->SetScalarParameter("SlashDissolve", Dissolve);
		CoreMaterial->SetScalarParameter("SlashNoiseScroll", NoiseScroll);
	}

	if (GlowMaterial)
	{
		GlowMaterial->SetScalarParameter("SlashAlpha", GlowAlpha);
		GlowMaterial->SetScalarParameter("SlashDissolve", Dissolve * 0.75f);
		GlowMaterial->SetScalarParameter("SlashNoiseScroll", NoiseScroll * 0.65f);
	}

	if (RefractionMaterial)
	{
		RefractionMaterial->SetScalarParameter("SlashAlpha", CoreAlpha);
		RefractionMaterial->SetScalarParameter("RefractionStrength", RefractionStrength * CoreAlpha);
		RefractionMaterial->SetScalarParameter("RefractionNoiseScroll", NoiseScroll);
	}
}

void ASlashEffectActor::ResolveComponents()
{
	SlashRootComponent = GetRootComponent();

	CoreMeshComponent = nullptr;
	GlowMeshComponent = nullptr;

	for (UActorComponent* Component : GetComponents())
	{
		UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component);
		if (!MeshComp)
		{
			continue;
		}

		if (!CoreMeshComponent)
		{
			CoreMeshComponent = MeshComp;
		}
		else if (!GlowMeshComponent)
		{
			GlowMeshComponent = MeshComp;
			break;
		}
	}
}
