#include "PlayerStart.h"

#include "Component/BillboardComponent.h"
#include "Core/Paths.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Renderer/Texture.h"

IMPLEMENT_RTTI(APlayerStart, AActor)

void APlayerStart::PostSpawnInitialize()
{
	BillboardComponent = FObjectFactory::ConstructObject<UBillboardComponent>(this, "BillboardComponent");

	if (UTexture* PawnSprite = UTexture::FindOrLoad("Editor/Icons/Pawn_64x.png", this))
	{
		BillboardComponent->SetSprite(PawnSprite);
	}

	BillboardComponent->SetSize(FVector2(0.5f, 0.5f));
	AddOwnedComponent(BillboardComponent);

	AActor::PostSpawnInitialize();
}

void APlayerStart::Tick(float fTimeDelta)
{

}
