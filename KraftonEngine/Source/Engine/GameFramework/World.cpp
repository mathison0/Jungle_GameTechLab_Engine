#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Pipeline/WorldRenderProxy.h"
#include <algorithm>
#include <memory>

IMPLEMENT_CLASS(UWorld, UObject)

UWorld::UWorld()
{
	RenderProxy = std::make_unique<FWorldRenderProxy>();
}

UWorld::~UWorld()
{
	if (!Actors.empty())
	{
		EndPlay();
	}
}

void UWorld::DestroyActor(AActor* Actor)
{
	// remove and clean up
	if (!Actor) return;
	Actor->UnregisterAllComponents();
	Actor->EndPlay();
	// Remove from actor list
	auto it = std::find(Actors.begin(), Actors.end(), Actor);
	if (it != Actors.end())
		Actors.erase(it);

	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::AddActor(AActor* Actor)
{
	if (Actor)
	{
		Actors.push_back(Actor);
		Actor->RegisterAllComponents();
	}
}

void UWorld::InitWorld()
{
	if (!RenderProxy)
	{
		RenderProxy = std::make_unique<FWorldRenderProxy>();
	}
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

void UWorld::Tick(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->UnregisterAllComponents();
			Actor->EndPlay();
			UObjectManager::Get().DestroyObject(Actor);
		}
	}

	Actors.clear();
}
