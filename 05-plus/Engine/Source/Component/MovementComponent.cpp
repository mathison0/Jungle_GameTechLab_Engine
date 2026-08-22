#include "MovementComponent.h"

#include "Actor/Actor.h"
#include "Component/SceneComponent.h"
#include "Math/MathUtility.h"
#include "Object/Class.h"
#include "Serializer/Archive.h"
#include "World/World.h"

#include <cmath>

IMPLEMENT_RTTI(UMovementComponent, UActorComponent)

namespace
{
	bool ShouldSimulateMovement(const AActor* Owner)
	{
		if (Owner == nullptr)
		{
			return false;
		}

		const UWorld* World = Owner->GetWorld();
		if (World == nullptr)
		{
			return false;
		}

		const EWorldType WorldType = World->GetWorldType();
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}
}

void UMovementComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	ResetRuntimeState();

	if (AActor* Owner = GetOwner())
	{
		if (USceneComponent* RootComponent = Owner->GetRootComponent())
		{
			InitialRelativeZ = RootComponent->GetRelativeLocation().Z;
		}
	}
}

void UMovementComponent::Tick(float DeltaTime)
{
	if (!bEnabled || !ShouldSimulateMovement(GetOwner()))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	USceneComponent* RootComponent = Owner->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	FVector RelativeLocation = RootComponent->GetRelativeLocation();
	RelativeLocation.Z = InitialRelativeZ + std::sin(ElapsedTime * Speed) * Amplitude;
	RootComponent->SetRelativeLocation(RelativeLocation);
}

void UMovementComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar.Serialize("Enabled", bEnabled);
	Ar.Serialize("Amplitude", Amplitude);
	Ar.Serialize("Speed", Speed);

	if (Ar.IsLoading())
	{
		SetAmplitude(Amplitude);
		SetSpeed(Speed);
		ResetRuntimeState();
	}
}

void UMovementComponent::DuplicateSubObjects()
{
	UActorComponent::DuplicateSubObjects();
	ResetRuntimeState();
}

void UMovementComponent::SetAmplitude(float InAmplitude)
{
	Amplitude = FMath::Max(InAmplitude, 0.0f);
}

bool UMovementComponent::IsEnabled() const
{
	return bEnabled;
}

void UMovementComponent::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
}

float UMovementComponent::GetAmplitude() const
{
	return Amplitude;
}

void UMovementComponent::SetSpeed(float InSpeed)
{
	Speed = FMath::Max(InSpeed, 0.0f);
}

float UMovementComponent::GetSpeed() const
{
	return Speed;
}

void UMovementComponent::ResetRuntimeState()
{
	InitialRelativeZ = 0.0f;
	ElapsedTime = 0.0f;
}
