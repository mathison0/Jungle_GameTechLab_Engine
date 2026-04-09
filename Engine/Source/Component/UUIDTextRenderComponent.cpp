#include "UUIDTextRenderComponent.h"
#include "Actor/Actor.h"
#include "Object/Class.h"
#include <limits>

IMPLEMENT_RTTI(UUUIDTextRenderComponent, UTextRenderComponent)

namespace
{
	struct FUUIDTextRenderComponentAliases
	{
		FUUIDTextRenderComponentAliases()
		{
			UClass::RegisterAlias("UUUIDBillboardComponent", UUUIDTextRenderComponent::StaticClass());
		}
	} GUUIDTextRenderComponentAliases;
}

FRenderMesh* UUUIDTextRenderComponent::GetRenderMesh() const
{
	// UUID 빌보드는 에디터 전용이므로 PIE/Game 월드에서는 렌더링하지 않는다.
	AActor* OwnerActor = GetOwner();
	if (OwnerActor)
	{
		if (UWorld* World = OwnerActor->GetWorld())
		{
			if (World->GetWorldType() != EWorldType::Editor)
				return nullptr;
		}
	}
	return UTextRenderComponent::GetRenderMesh();
}

void UUUIDTextRenderComponent::PostConstruct()
{
	UTextRenderComponent::PostConstruct();
	SetAlwaysFaceCamera(true);
	bDrawDebugBounds = false;
	SetWorldSize(0.3f);
}

FString UUUIDTextRenderComponent::GetDisplayText() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return "";
	}

	return FString("UUID: ") + OwnerActor->GetUUIDString();
}

FVector UUUIDTextRenderComponent::GetRenderWorldPosition() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return WorldOffset;

	USceneComponent* Root = OwnerActor->GetRootComponent();
	if (!Root) return WorldOffset;

	const FVector RootLocation = Root->GetWorldLocation();

	bool bFoundPrimitiveBounds = false;
	float MaxTopZ = -std::numeric_limits<float>::infinity();

	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		if (!Component || Component == this) continue;
		if (!Component->IsA(UPrimitiveComponent::StaticClass())) continue;

		UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
		FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();

		const float TopZ = Bounds.Center.Z + Bounds.BoxExtent.Z;

		if (!bFoundPrimitiveBounds || TopZ > MaxTopZ)
		{
			MaxTopZ = TopZ;
			bFoundPrimitiveBounds = true;
		}
	}

	if (bFoundPrimitiveBounds)
	{
		return FVector(
			RootLocation.X + WorldOffset.X,
			RootLocation.Y + WorldOffset.Y,
			MaxTopZ + WorldOffset.Z
		);
	}

	return RootLocation + WorldOffset;
}

FVector UUUIDTextRenderComponent::GetRenderWorldScale() const
{
	return FVector(WorldSize, WorldSize, WorldSize);
}

FBoxSphereBounds UUUIDTextRenderComponent::GetWorldBounds() const
{
	const FVector Center = GetRenderWorldPosition();
	const FVector Extent(WorldSize * 3.0f * 0.707f, WorldSize * 3.0f * 0.707f, WorldSize * 3.0f * 0.707f);

	return { Center, Extent.Size(), Extent };
}
