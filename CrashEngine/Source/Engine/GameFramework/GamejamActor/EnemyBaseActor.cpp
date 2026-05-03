#include "EnemyBaseActor.h"
#include "Component/Collision/CircleCollider2DComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AEnemyBaseActor, AActor)

void AEnemyBaseActor::BeginPlay()
{
    AActor::BeginPlay();
    SetActorTag(FName("Enemy"));
}

void AEnemyBaseActor::Tick(float DeltaTime)
{
    AActor::Tick(DeltaTime);
}

void AEnemyBaseActor::InitDefaultComponents()
{
    ColliderComponent = AddComponent<UCircleCollider2DComponent>();
    ColliderComponent->SetCollisionChannel(ECollisionChannel::Enemy);
    ColliderComponent->SetGenerateOverlapEvents(true);
    SetRootComponent(ColliderComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(FPaths::ContentRelativePath("Models/_Basic/Cube.OBJ"), Device);

	MeshComponent = AddComponent<UStaticMeshComponent>();
    MeshComponent->SetStaticMesh(Asset);
    ColliderComponent->AddChild(MeshComponent);

	EnemyScriptComponent = AddComponent<UScriptComponent>();
    EnemyScriptComponent->SetScriptPath("EnemyScript.lua");

	EnemyScriptComponent = AddComponent<UScriptComponent>();
    EnemyScriptComponent->SetScriptPath("Components/EnemyState.lua");
}
