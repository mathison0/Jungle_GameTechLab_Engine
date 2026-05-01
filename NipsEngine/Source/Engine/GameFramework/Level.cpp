#include "Level.h"
#include "Component/PrimitiveComponent.h"
#include "UI/EditorConsoleWidget.h"

DEFINE_CLASS(ULevel, UObject)
REGISTER_FACTORY(ULevel)

namespace
{
	bool IsOverlapping(const FAABB& A, const FAABB& B)
	{
		return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X &&
			   A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y &&
			   A.Min.Z <= B.Max.Z && A.Max.Z >= B.Min.Z;
	}

	struct FPrimitiveOverlapCandidate
	{
		AActor* Actor = nullptr;
		UPrimitiveComponent* Component = nullptr;
	};
}

// 소멸될 때 가지고 있던 모든 액터들을 메모리에서 완전히 해제한다.
ULevel::~ULevel()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			UObjectManager::Get().DestroyObject(Actor);
		}
	}

	Actors.clear();
}

/* @brief 액터 배열을 얕은 복사한 뒤 각 액터를 깊은 복사로 교체합니다. */
void ULevel::PostDuplicate(UObject* Original)
{
	const ULevel* OrigLevel = Cast<ULevel>(Original);
	Actors = OrigLevel->Actors; // 얕은 복사
	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		if (Actors[i])
		{
			Actors[i] = Cast<AActor>(Actors[i]->Duplicate()); // 깊은 복사로 교체
		}
	}
}

void ULevel::BeginPlay()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

void ULevel::TickEditor(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor && Actor->IsActive() && Actor->ShouldTickInEditor())
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void ULevel::TickGame(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor && Actor->IsActive())
		{
			Actor->Tick(DeltaTime);
		}
	}

	// brute-force O(n²) 충돌 처리
	TArray<FPrimitiveOverlapCandidate> Candidates;
	for (AActor* Actor : Actors)
	{
		if (Actor == nullptr || !Actor->IsActive())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (Primitive && Primitive->IsGenerateOverlapEvents())
			{
				Candidates.push_back({ Actor, Primitive });
			}
		}
	}

	for (int32 i = 0; i < static_cast<int32>(Candidates.size()); ++i)
	{
		for (int32 j = i + 1; j < static_cast<int32>(Candidates.size()); ++j)
		{
			FPrimitiveOverlapCandidate& A = Candidates[i];
			FPrimitiveOverlapCandidate& B = Candidates[j];

			if (A.Actor == B.Actor)
			{
				continue;
			}

			const bool bIsOverlapping = IsOverlapping(A.Component->GetWorldAABB(), B.Component->GetWorldAABB());

			if (bIsOverlapping)
			{
				UE_LOG("Actors A and B Collided.");
				A.Component->AddOverlapInfo(B.Actor, B.Component);
				B.Component->AddOverlapInfo(A.Actor, A.Component);
			}
			else
			{
				A.Component->RemoveOverlapInfo(B.Actor, B.Component);
				B.Component->RemoveOverlapInfo(A.Actor, A.Component);
			}
		}
	}
}

void ULevel::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->EndPlay(EndPlayReason);
		}
	}
}
