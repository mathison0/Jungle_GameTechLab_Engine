#include "Component/TestInputComponent.h"
#include "Input/GameInput.h"
#include "Core/Logging/LogMacros.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UTestInputComponent, UActorComponent)

void UTestInputComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
    (void)DeltaTime;

    float H = GameInput::GetAxis("Horizontal");
    float V = GameInput::GetAxis("Vertical");

    if (H != 0.0f || V != 0.0f)
    {
        FVector CurLocation = Owner->GetActorLocation();
        CurLocation.X += V * MoveSpeed * DeltaTime;
        CurLocation.Y += H * MoveSpeed * DeltaTime;
        Owner->SetActorLocation(CurLocation);
    }

    if (GameInput::GetKeyDown(VK_SPACE))
    {
        UE_LOG(TestInput, Info, "Space Key Down!");
    }
}

void UTestInputComponent::Serialize(FArchive& Ar)
{
    UActorComponent::Serialize(Ar);
    Ar << MoveSpeed;
}

void UTestInputComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UActorComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Move Speed", EPropertyType::Float, &MoveSpeed });
}
