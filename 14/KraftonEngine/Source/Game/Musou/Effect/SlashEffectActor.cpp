#include "Game/Musou/Effect/SlashEffectActor.h"

#include "Component/Particle/ParticleSystemComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshManager.h"
#include "Particles/ParticleSystemManager.h"
#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Render/Pipeline/Renderer.h"

#include <cmath>

namespace
{
	constexpr float FixedRefractionStrength = 0.20f;
	constexpr float FixedRefractionScale = 1.70f;
	constexpr float FixedRefractionScreenThickness = 10.0f;
	constexpr float FixedRefractionScreenThicknessStrength = 1.0f;

	float SlashSparkRandom01(int32 Index, int32 Salt)
	{
		const float Value = std::sin(static_cast<float>(Index * 127 + Salt * 311) * 12.9898f) * 43758.5453f;
		return Value - std::floor(Value);
	}

	float SlashSparkSignedRandom(int32 Index, int32 Salt)
	{
		return SlashSparkRandom01(Index, Salt) * 2.0f - 1.0f;
	}

	void BuildArcSparkLocalFrame(
		ESlashArcSparkPlaneMode PlaneMode,
		float SinAngle,
		float CosAngle,
		FVector& OutDirection,
		FVector& OutTangent)
	{
		switch (PlaneMode)
		{
		case ESlashArcSparkPlaneMode::XZ:
			OutDirection = FVector(SinAngle, 0.0f, CosAngle);
			OutTangent = FVector(CosAngle, 0.0f, -SinAngle);
			break;
		case ESlashArcSparkPlaneMode::XY:
			OutDirection = FVector(SinAngle, CosAngle, 0.0f);
			OutTangent = FVector(CosAngle, -SinAngle, 0.0f);
			break;
		case ESlashArcSparkPlaneMode::YZ:
		default:
			OutDirection = FVector(0.0f, SinAngle, CosAngle);
			OutTangent = FVector(0.0f, CosAngle, -SinAngle);
			break;
		}
	}
}

void ASlashEffectActor::InitDefaultComponents()
{
	bTickInEditor = true;

	SlashRootComponent = AddComponent<USceneComponent>();
	SetRootComponent(SlashRootComponent);

	CoreMeshComponent = AddComponent<UStaticMeshComponent>();
	CoreMeshComponent->AttachToComponent(SlashRootComponent);

	GlowMeshComponent = AddComponent<UStaticMeshComponent>();
	GlowMeshComponent->AttachToComponent(SlashRootComponent);

	RefractionMeshComponent = AddComponent<UStaticMeshComponent>();
	RefractionMeshComponent->AttachToComponent(SlashRootComponent);

	ArcSparkComponent = AddComponent<UParticleSystemComponent>();
	ArcSparkComponent->AttachToComponent(SlashRootComponent);
	ArcSparkComponent->Deactivate();

	LoadSlashAssets();

	SetVisible(false);
	bActive = false;
}

void ASlashEffectActor::BeginPlay()
{
	Super::BeginPlay();

	if (!CoreMeshComponent || !GlowMeshComponent || !RefractionMeshComponent || !ArcSparkComponent)
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
	UStaticMesh* Mesh = FMeshManager::LoadStaticMesh(MeshPath.ToString(), Device);

	CoreMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(CoreMaterialPath.ToString());
	GlowMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(GlowMaterialPath.ToString());

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

	RefractionMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(RefractionMaterialPath.ToString());

	if (RefractionMeshComponent)
	{
		RefractionMeshComponent->SetStaticMesh(Mesh);
		RefractionMeshComponent->SetMaterial(0, RefractionMaterial);
		RefractionMeshComponent->SetRelativeScale(FVector(FixedRefractionScale, FixedRefractionScale, FixedRefractionScale));
	}

	ArcSparkParticle = ArcSparkParticlePath.IsNull()
		? nullptr
		: FParticleSystemManager::Get().Load(ArcSparkParticlePath.ToString());

	if (ArcSparkComponent)
	{
		ArcSparkComponent->SetTemplate(ArcSparkParticle);
		ArcSparkComponent->Deactivate();
	}
}

void ASlashEffectActor::ConfigureSlashEffect(
	const FSoftObjectPtr& InMeshPath,
	const FSoftObjectPtr& InCoreMaterialPath,
	const FSoftObjectPtr& InGlowMaterialPath,
	const FSoftObjectPtr& InRefractionMaterialPath,
	const FVector& InCoreRelativeScale,
	const FVector& InGlowRelativeScale,
	const FVector& InRefractionRelativeScale,
	float InGlowAlphaMultiplier,
	float InLifetime,
	const FVector& InStartScale,
	const FVector& InPeakScale,
	const FVector& InEndScale,
	float InMoveSpeed,
	const FVector& InRotationOffset,
	float InCoreFadeOutSpeed,
	float InGlowFadeOutSpeed,
	float InDissolveStartTime,
	float InDissolveEndValue,
	float InNoiseScrollSpeed,
	float InRefractionStrength,
	float InRevealDurationRatio,
	float InRevealSoftness,
	float InEdgeSoftness,
	float InTailFadeStart,
	float InTrailLength,
	float InCoreScreenThickness,
	float InCoreScreenThicknessStrength,
	float InGlowScreenThickness,
	float InGlowScreenThicknessStrength,
	float InRefractionScreenThickness,
	float InRefractionScreenThicknessStrength,
	const FVector4& InDissolveEdgeColor,
	float InDissolveEdgeWidth,
	float InCoreDissolveEdgeIntensity,
	float InGlowDissolveEdgeIntensity,
	bool bInSpawnArcSparks,
	const FSoftObjectPtr& InArcSparkParticlePath,
	int32 InArcSparkCount,
	float InArcSparkRadius,
	float InArcSparkAngleMin,
	float InArcSparkAngleMax,
	ESlashArcSparkPlaneMode InArcSparkPlaneMode,
	float InArcSparkOutwardSpeed,
	float InArcSparkTangentSpeed,
	float InArcSparkUpSpeed,
	float InArcSparkPositionJitter,
	float InArcSparkSpeedJitter)
{
	MeshPath = InMeshPath;
	CoreMaterialPath = InCoreMaterialPath;
	GlowMaterialPath = InGlowMaterialPath;
	RefractionMaterialPath = InRefractionMaterialPath;

	CoreRelativeScale = InCoreRelativeScale;
	GlowRelativeScale = InGlowRelativeScale;
	RefractionRelativeScale = InRefractionRelativeScale;

	GlowAlphaMultiplier = InGlowAlphaMultiplier;
	Lifetime = InLifetime;
	StartScale = InStartScale;
	PeakScale = InPeakScale;
	EndScale = InEndScale;
	MoveSpeed = InMoveSpeed;
	RotationOffset = InRotationOffset;
	CoreFadeOutSpeed = InCoreFadeOutSpeed;
	GlowFadeOutSpeed = InGlowFadeOutSpeed;
	DissolveStartTime = InDissolveStartTime;
	DissolveEndValue = InDissolveEndValue;
	NoiseScrollSpeed = InNoiseScrollSpeed;
	RefractionStrength = InRefractionStrength;
	RevealDurationRatio = InRevealDurationRatio;
	RevealSoftness = InRevealSoftness;
	EdgeSoftness = InEdgeSoftness;
	TailFadeStart = InTailFadeStart;
	TrailLength = InTrailLength;
	CoreScreenThickness = InCoreScreenThickness;
	CoreScreenThicknessStrength = InCoreScreenThicknessStrength;
	GlowScreenThickness = InGlowScreenThickness;
	GlowScreenThicknessStrength = InGlowScreenThicknessStrength;
	RefractionScreenThickness = InRefractionScreenThickness;
	RefractionScreenThicknessStrength = InRefractionScreenThicknessStrength;
	DissolveEdgeColor = InDissolveEdgeColor;
	DissolveEdgeWidth = InDissolveEdgeWidth;
	CoreDissolveEdgeIntensity = InCoreDissolveEdgeIntensity;
	GlowDissolveEdgeIntensity = InGlowDissolveEdgeIntensity;
	bSpawnArcSparks = bInSpawnArcSparks;
	ArcSparkParticlePath = InArcSparkParticlePath;
	ArcSparkCount = InArcSparkCount;
	ArcSparkRadius = InArcSparkRadius;
	ArcSparkAngleMin = InArcSparkAngleMin;
	ArcSparkAngleMax = InArcSparkAngleMax;
	ArcSparkPlaneMode = InArcSparkPlaneMode;
	ArcSparkOutwardSpeed = InArcSparkOutwardSpeed;
	ArcSparkTangentSpeed = InArcSparkTangentSpeed;
	ArcSparkUpSpeed = InArcSparkUpSpeed;
	ArcSparkPositionJitter = InArcSparkPositionJitter;
	ArcSparkSpeedJitter = InArcSparkSpeedJitter;
}

void ASlashEffectActor::ActivateSlash(
	const FVector& Location,
	const FVector& Rotation,
	const FVector& Direction)
{
	SetActorLocation(Location);
	SetActorRotation(Rotation);
	SetActorScale(StartScale);

	if (CoreMeshComponent)
	{
		CoreMeshComponent->SetRelativeScale(CoreRelativeScale);
		CoreMeshComponent->SetRelativeRotation(FQuat::Identity);
	}

	if (GlowMeshComponent)
	{
		GlowMeshComponent->SetRelativeScale(GlowRelativeScale);
		GlowMeshComponent->SetRelativeRotation(FQuat::Identity);
	}

	if (RefractionMeshComponent)
	{
		RefractionMeshComponent->SetRelativeScale(FVector(FixedRefractionScale, FixedRefractionScale, FixedRefractionScale));
		RefractionMeshComponent->SetRelativeRotation(FQuat::Identity);
	}

	MoveDirection = Direction.Normalized();
	Age = 0.0f;
	bActive = true;

	SetSlashMaterialParams(1.0f, GlowAlphaMultiplier, 0.0f, 0.0f, 0.0f);
	SetVisible(true);
	SpawnArcSparks();
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
	const float Reveal = RevealDurationRatio > 0.0f
		? FMath::Clamp(T / RevealDurationRatio, 0.0f, 1.0f)
		: 1.0f;

	SetActorScale(CurrentScale);
	SetSlashMaterialParams(CoreAlpha, GlowAlpha, Dissolve, NoiseScroll, Reveal);

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
	float NoiseScroll,
	float Reveal)
{
	CoreAlpha = FMath::Clamp(CoreAlpha, 0.0f, 1.0f);
	GlowAlpha = FMath::Clamp(GlowAlpha, 0.0f, 1.0f);
	Dissolve = FMath::Clamp(Dissolve, 0.0f, 1.0f);
	Reveal = FMath::Clamp(Reveal, 0.0f, 1.0f);

	const float GlowReveal = FMath::Clamp(Reveal - 0.10f, 0.0f, 1.0f);
	const float GlowDissolve = FMath::Clamp(Dissolve * 0.65f, 0.0f, 1.0f);
	const float GlowTrailLength = FMath::Clamp(TrailLength + 0.15f, 0.0f, 1.0f);

	const float RefractionReveal = FMath::Clamp(Reveal + 0.15f, 0.0f, 1.0f);
	const float RefractionTrailLength = FMath::Clamp(TrailLength - 0.15f, 0.0f, 1.0f);

	if (CoreMaterial)
	{
		CoreMaterial->SetScalarParameter("SlashAlpha", CoreAlpha);
		CoreMaterial->SetScalarParameter("SlashDissolve", Dissolve);
		CoreMaterial->SetScalarParameter("SlashNoiseScroll", NoiseScroll);
		CoreMaterial->SetScalarParameter("SlashReveal", Reveal);
		CoreMaterial->SetScalarParameter("SlashRevealSoftness", RevealSoftness);
		CoreMaterial->SetScalarParameter("SlashEdgeSoftness", EdgeSoftness);
		CoreMaterial->SetScalarParameter("SlashTailFadeStart", TailFadeStart);
		CoreMaterial->SetScalarParameter("SlashTrailLength", TrailLength);
		CoreMaterial->SetScalarParameter("SlashScreenThickness", CoreScreenThickness);
		CoreMaterial->SetScalarParameter("SlashScreenThicknessStrength", CoreScreenThicknessStrength);
		CoreMaterial->SetVector4Parameter("SlashDissolveEdgeColor", DissolveEdgeColor);
		CoreMaterial->SetScalarParameter("SlashDissolveEdgeWidth", DissolveEdgeWidth);
		CoreMaterial->SetScalarParameter("SlashDissolveEdgeIntensity", CoreDissolveEdgeIntensity);
	}

	if (GlowMaterial)
	{
		GlowMaterial->SetScalarParameter("SlashAlpha", GlowAlpha);
		GlowMaterial->SetScalarParameter("SlashDissolve", GlowDissolve);
		GlowMaterial->SetScalarParameter("SlashNoiseScroll", NoiseScroll * 0.65f);
		GlowMaterial->SetScalarParameter("SlashReveal", GlowReveal);
		GlowMaterial->SetScalarParameter("SlashRevealSoftness", RevealSoftness);
		GlowMaterial->SetScalarParameter("SlashEdgeSoftness", EdgeSoftness);
		GlowMaterial->SetScalarParameter("SlashTailFadeStart", TailFadeStart);
		GlowMaterial->SetScalarParameter("SlashTrailLength", GlowTrailLength);
		GlowMaterial->SetScalarParameter("SlashScreenThickness", GlowScreenThickness);
		GlowMaterial->SetScalarParameter("SlashScreenThicknessStrength", GlowScreenThicknessStrength);
		GlowMaterial->SetVector4Parameter("SlashDissolveEdgeColor", DissolveEdgeColor);
		GlowMaterial->SetScalarParameter("SlashDissolveEdgeWidth", DissolveEdgeWidth);
		GlowMaterial->SetScalarParameter("SlashDissolveEdgeIntensity", GlowDissolveEdgeIntensity);
	}

	if (RefractionMaterial)
	{
		RefractionMaterial->SetScalarParameter("SlashAlpha", CoreAlpha);
		RefractionMaterial->SetScalarParameter("RefractionStrength", FixedRefractionStrength * CoreAlpha);
		RefractionMaterial->SetScalarParameter("RefractionNoiseScroll", NoiseScroll);
		RefractionMaterial->SetScalarParameter("SlashReveal", RefractionReveal);
		RefractionMaterial->SetScalarParameter("SlashRevealSoftness", RevealSoftness);
		RefractionMaterial->SetScalarParameter("SlashEdgeSoftness", EdgeSoftness);
		RefractionMaterial->SetScalarParameter("SlashTailFadeStart", TailFadeStart);
		RefractionMaterial->SetScalarParameter("SlashTrailLength", RefractionTrailLength);
		RefractionMaterial->SetScalarParameter("SlashScreenThickness", FixedRefractionScreenThickness);
		RefractionMaterial->SetScalarParameter("SlashScreenThicknessStrength", FixedRefractionScreenThicknessStrength);
	}
}

void ASlashEffectActor::SpawnArcSparks()
{
	if (!bSpawnArcSparks || !ArcSparkComponent || !ArcSparkParticle || ArcSparkCount <= 0)
	{
		return;
	}

	ArcSparkComponent->SetTemplate(ArcSparkParticle);
	ArcSparkComponent->ResetSystem();
	ArcSparkComponent->Activate();

	TArray<FParticleBurstSpawn> SpawnInfos;
	SpawnInfos.reserve(ArcSparkCount);

	const FMatrix SparkWorldMatrix = CoreMeshComponent
		? CoreMeshComponent->GetWorldMatrix()
		: GetRootComponent()
			? GetRootComponent()->GetWorldMatrix()
			: FMatrix::MakeTranslationMatrix(GetActorLocation());
	const float AngleRange = ArcSparkAngleMax - ArcSparkAngleMin;

	for (int32 Index = 0; Index < ArcSparkCount; ++Index)
	{
		const float Fraction = ArcSparkCount > 1
			? static_cast<float>(Index) / static_cast<float>(ArcSparkCount - 1)
			: 0.5f;
		const float AngleDeg = ArcSparkAngleMin + AngleRange * Fraction;
		const float AngleRad = AngleDeg * FMath::DegToRad;

		const float PositionJitter = ArcSparkPositionJitter * SlashSparkSignedRandom(Index, 1);
		float Radius = ArcSparkRadius + PositionJitter;
		if (Radius < 0.0f)
		{
			Radius = 0.0f;
		}
		const float SinAngle = std::sin(AngleRad);
		const float CosAngle = std::cos(AngleRad);

		FVector LocalOutward;
		FVector LocalTangent;
		BuildArcSparkLocalFrame(ArcSparkPlaneMode, SinAngle, CosAngle, LocalOutward, LocalTangent);

		FVector LocalPosition = LocalOutward * Radius;
		if (LocalOutward.LengthSquared() <= 0.0001f)
		{
			LocalOutward = FVector::UpVector;
		}
		LocalOutward.Normalize();

		if (LocalTangent.LengthSquared() <= 0.0001f)
		{
			LocalTangent = FVector::RightVector;
		}
		LocalTangent.Normalize();

		const float SpeedScale = 1.0f + ArcSparkSpeedJitter * SlashSparkSignedRandom(Index, 2);
		const float TangentSign = SlashSparkRandom01(Index, 3) > 0.5f ? 1.0f : -1.0f;
		const FVector WorldPosition = SparkWorldMatrix.TransformPositionWithW(LocalPosition);
		FVector WorldOutward = SparkWorldMatrix.TransformVector(LocalOutward);
		if (WorldOutward.LengthSquared() <= 0.0001f)
		{
			WorldOutward = FVector::UpVector;
		}
		WorldOutward.Normalize();

		FVector WorldTangent = SparkWorldMatrix.TransformVector(LocalTangent);
		if (WorldTangent.LengthSquared() <= 0.0001f)
		{
			WorldTangent = FVector::RightVector;
		}
		WorldTangent.Normalize();

		const FVector WorldVelocity =
			WorldOutward * ArcSparkOutwardSpeed * SpeedScale
			+ WorldTangent * ArcSparkTangentSpeed * TangentSign
			+ FVector::UpVector * ArcSparkUpSpeed;

		FParticleBurstSpawn SpawnInfo;
		SpawnInfo.Location = WorldPosition;
		SpawnInfo.Velocity = WorldVelocity;
		SpawnInfos.push_back(SpawnInfo);
	}

	ArcSparkComponent->EmitBurst(SpawnInfos);
}

void ASlashEffectActor::ResolveComponents()
{
	SlashRootComponent = GetRootComponent();

	CoreMeshComponent = nullptr;
	GlowMeshComponent = nullptr;
	RefractionMeshComponent = nullptr;
	ArcSparkComponent = GetComponentByClass<UParticleSystemComponent>();

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
		}
		else if (!RefractionMeshComponent)
		{
			RefractionMeshComponent = MeshComp;
			break;
		}
	}
}
