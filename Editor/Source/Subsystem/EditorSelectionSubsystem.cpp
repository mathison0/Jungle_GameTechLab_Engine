#include "Subsystem/EditorSelectionSubsystem.h"

#include "Actor/Actor.h"
#include "Component/ActorComponent.h"

void FEditorSelectionSubsystem::Shutdown()
{
	SelectedActor = nullptr;
	SelectedComponent = nullptr;
}

void FEditorSelectionSubsystem::SetSelectedActor(AActor* InActor)
{
	SelectedActor = InActor;
	SelectedComponent = nullptr;
}

AActor* FEditorSelectionSubsystem::GetSelectedActor() const
{
	if (SelectedActor && SelectedActor->IsPendingDestroy())
	{
		return nullptr;
	}

	return SelectedActor.Get();
}

void FEditorSelectionSubsystem::SetSelectedComponent(UActorComponent* InComponent)
{
	SelectedComponent = InComponent;
	SelectedActor = InComponent ? InComponent->GetOwner() : SelectedActor.Get();
}

UActorComponent* FEditorSelectionSubsystem::GetSelectedComponent() const
{
	if (SelectedComponent && SelectedComponent->IsPendingKill())
	{
		return nullptr;
	}

	AActor* OwnerActor = SelectedComponent ? SelectedComponent->GetOwner() : nullptr;
	if (OwnerActor && OwnerActor->IsPendingDestroy())
	{
		return nullptr;
	}

	return SelectedComponent.Get();
}
