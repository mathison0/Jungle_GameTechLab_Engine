#pragma once

#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Render/Pipeline/WorldRenderProxy.h"
#include <memory>

// === Forward Declaration
class UCameraComponent;
// === Forward Declaration

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld();
	~UWorld() override;

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	void DestroyActor(AActor* Actor);

	const TArray<AActor*>& GetActors() const { return Actors; }
	void AddActor(AActor* Actor);

	void InitWorld();      // Set up the world before gameplay begins
	void BeginPlay();      // Triggers BeginPlay on all actors
	void Tick(float DeltaTime);  // Drives the game loop every frame
	void EndPlay();        // Cleanup before world is destroyed

	bool HasBegunPlay() const { return bHasBegunPlay; }

	// Active Camera — EditorViewportClient 또는 PlayerController가 세팅
	void SetActiveCamera(UCameraComponent* InCamera) { ActiveCamera = InCamera; }
	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }

	FWorldRenderProxy& GetRenderProxy() { return *RenderProxy; }

private:
	TArray<AActor*> Actors;
	UCameraComponent* ActiveCamera = nullptr;
	bool bHasBegunPlay = false;

	std::unique_ptr<FWorldRenderProxy> RenderProxy;
};

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>();
	Actor->SetWorld(this);
	Actor->RegisterAllComponents();
	if (bHasBegunPlay)
	{
		Actor->BeginPlay();
	}
	Actors.push_back(Actor);
	return Actor;
}
