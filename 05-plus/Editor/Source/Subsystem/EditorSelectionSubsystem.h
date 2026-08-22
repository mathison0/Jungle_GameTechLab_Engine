#pragma once

#include "Types/ObjectPtr.h"

class AActor;
class UActorComponent;

class FEditorSelectionSubsystem
{
public:
	void Shutdown();
	void SetSelectedActor(AActor* InActor);
	AActor* GetSelectedActor() const;
	void SetSelectedComponent(UActorComponent* InComponent);
	UActorComponent* GetSelectedComponent() const;

private:
	TObjectPtr<AActor> SelectedActor;
	TObjectPtr<UActorComponent> SelectedComponent;
};
