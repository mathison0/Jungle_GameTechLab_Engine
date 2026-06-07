#include "Game/Musou/Combat/AnimNotify_PlaySlashEffect.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Game/Musou/Effect/SlashEffectActor.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"

void UAnimNotify_PlaySlashEffect::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	if (!MeshComp) return;

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor) return;

	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	const bool bIsEditorPreview = World->GetWorldType() == EWorldType::EditorPreview;
	if (bIsEditorPreview && !bPreviewInEditor)
	{
		return;
	}

	if (bOnlyPlayer && !bIsEditorPreview)
	{
		APawn* OwnerPawn = Cast<APawn>(OwnerActor);
		if (!OwnerPawn || !OwnerPawn->IsPossessed()) return;
	}

	const FVector Forward = OwnerActor->GetActorForward().Normalized();
	const FVector Right = OwnerActor->GetActorRight().Normalized();
	const FVector Up = FVector::UpVector;

	const FVector SpawnLocation =
		OwnerActor->GetActorLocation()
		+ Forward * LocationOffset.X
		+ Right * LocationOffset.Y
		+ Up * LocationOffset.Z;

	const FVector SpawnRotation =
		OwnerActor->GetActorRotation().ToVector()
		+ RotationOffset;

	const FVector SlashDirection = Forward;

	ASlashEffectActor* Effect = World->SpawnActor<ASlashEffectActor>();
	if (!Effect)
	{
		return;
	}

	Effect->bTickInEditor = true;
	Effect->ConfigureSlashEffect(
		MeshPath,
		CoreMaterialPath,
		GlowMaterialPath,
		RefractionMaterialPath,
		CoreRelativeScale,
		GlowRelativeScale,
		RefractionRelativeScale,
		GlowAlphaMultiplier,
		Lifetime,
		StartScale,
		PeakScale,
		EndScale,
		MoveSpeed,
		FVector::ZeroVector,
		CoreFadeOutSpeed,
		GlowFadeOutSpeed,
		DissolveStartTime,
		DissolveEndValue,
		NoiseScrollSpeed,
		RefractionStrength,
		RevealDurationRatio,
		RevealSoftness,
		EdgeSoftness,
		TailFadeStart,
		TrailLength,
		CoreScreenThickness,
		CoreScreenThicknessStrength,
		GlowScreenThickness,
		GlowScreenThicknessStrength,
		RefractionScreenThickness,
		RefractionScreenThicknessStrength,
		DissolveEdgeColor,
		DissolveEdgeWidth,
		CoreDissolveEdgeIntensity,
		GlowDissolveEdgeIntensity,
		bSpawnArcSparks,
		ArcSparkParticlePath,
		ArcSparkCount,
		ArcSparkRadius,
		ArcSparkAngleMin,
		ArcSparkAngleMax,
		ArcSparkPlaneMode,
		ArcSparkOutwardSpeed,
		ArcSparkTangentSpeed,
		ArcSparkUpSpeed,
		ArcSparkPositionJitter,
		ArcSparkSpeedJitter);
	Effect->InitDefaultComponents();
	Effect->SetDestroyOnFinish(true);
	Effect->ActivateSlash(SpawnLocation, SpawnRotation, SlashDirection);
}
