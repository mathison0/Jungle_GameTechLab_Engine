#include "GameFramework/UIActor.h"

#include "Object/ObjectFactory.h"
#include "UI/UIComponent.h"

IMPLEMENT_CLASS(AUIActor, AActor)

void AUIActor::InitDefaultComponents()
{
    UIComponent = AddComponent<UUIComponent>();
    if (!UIComponent)
    {
        return;
    }

    UIComponent->SetRenderSpace(EUIRenderSpace::ScreenSpace);
    UIComponent->SetGeometryType(EUIGeometryType::Quad);
    UIComponent->SetScreenPosition(FVector2(100.0f, 100.0f));
    UIComponent->SetSize(FVector2(200.0f, 80.0f));
    UIComponent->SetPivot(FVector2(0.0f, 0.0f));
    UIComponent->SetTintColor(FVector4(1.0f, 1.0f, 1.0f, 1.0f));

    SetRootComponent(UIComponent);
}
