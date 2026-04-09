#include "PlayerStart.h"

#include "Component/ArrowComponent.h"
#include "Component/BillboardComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Renderer/Texture.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(APlayerStart, AActor)

namespace
{
	const FVector4 PlayerStartArrowColor(0.45f, 1.0f, 0.35f, 1.0f);
}

void APlayerStart::EnsureVisualizerComponents()
{
	ArrowComponent = GetExactComponentByClass<UArrowComponent>();
	if (ArrowComponent == nullptr)
	{
		ArrowComponent = FObjectFactory::ConstructObject<UArrowComponent>(this, "ArrowComponent");
		if (ArrowComponent)
		{
			AddOwnedComponent(ArrowComponent);
			ArrowComponent->SetArrowColor(PlayerStartArrowColor);
		}
	}

	USceneComponent* ExistingRoot = GetRootComponent();
	if (ArrowComponent && ExistingRoot != ArrowComponent)
	{
		const FTransform RootTransform = ExistingRoot ? ExistingRoot->GetRelativeTransform() : ArrowComponent->GetRelativeTransform();
		ArrowComponent->DetachFromParent();
		ArrowComponent->SetRelativeTransform(RootTransform);
		SetRootComponent(ArrowComponent);
	}

	BillboardComponent = GetExactComponentByClass<UBillboardComponent>();
	const bool bCreatedBillboard = (BillboardComponent == nullptr);
	if (BillboardComponent == nullptr)
	{
		BillboardComponent = FObjectFactory::ConstructObject<UBillboardComponent>(this, "BillboardComponent");
		if (BillboardComponent)
		{
			AddOwnedComponent(BillboardComponent);
		}
	}

	if (BillboardComponent)
	{
		if (BillboardComponent->GetSprite() == nullptr)
		{
			if (UTexture* PawnSprite = UTexture::FindOrLoad("Editor/Icons/Pawn_64x.png", this))
			{
				BillboardComponent->SetSprite(PawnSprite);
			}
		}

		if (bCreatedBillboard)
		{
			BillboardComponent->SetSize(FVector2(0.5f, 0.5f));
		}

		const bool bNeedsBillboardReset =
			bCreatedBillboard
			|| BillboardComponent == ExistingRoot
			|| BillboardComponent->GetAttachParent() != ArrowComponent;

		if (ArrowComponent && BillboardComponent->GetAttachParent() != ArrowComponent)
		{
			BillboardComponent->AttachTo(ArrowComponent);
		}

		if (bNeedsBillboardReset)
		{
			BillboardComponent->SetRelativeTransform(FTransform());
		}
	}
}

void APlayerStart::PostSpawnInitialize()
{
	EnsureVisualizerComponents();

	AActor::PostSpawnInitialize();
}

void APlayerStart::Serialize(FArchive& Ar)
{
	AActor::Serialize(Ar);

	if (Ar.IsLoading())
	{
		EnsureVisualizerComponents();
	}
}

void APlayerStart::BeginPlay()
{
	AActor::BeginPlay();
	SetVisible(false);
}

void APlayerStart::Tick(float fTimeDelta)
{
	AActor::Tick(fTimeDelta);
}
