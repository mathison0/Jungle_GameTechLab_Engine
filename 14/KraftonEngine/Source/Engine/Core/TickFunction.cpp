#include "TickFunction.h"
#include "Component/ActorComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

namespace
{
	bool ShouldDispatchActorTick(const AActor* Actor, ELevelTick TickType)
	{
		if (!Actor)
		{
			return false;
		}

		switch (TickType)
		{
		case LEVELTICK_ViewportsOnly:
			return Actor->bTickInEditor;

		case LEVELTICK_All:
		case LEVELTICK_TimeOnly:
		case LEVELTICK_PauseTick:
			return Actor->bNeedsTick && Actor->HasActorBegunPlay();

		default:
			return false;
		}
	}
}

void FTickFunction::RegisterTickFunction()
{
	bRegistered = true;
	TickAccumulator = 0.0f;
}

void FTickFunction::UnRegisterTickFunction()
{
	bRegistered = false;
	TickAccumulator = 0.0f;
}

void FTickManager::Tick(UWorld* World, float DeltaTime, ELevelTick TickType)
{
	GatherTickFunctions(World, TickType);

	for (int GroupIndex = 0; GroupIndex < TG_MAX; ++GroupIndex)
	{
		const ETickingGroup CurrentGroup = static_cast<ETickingGroup>(GroupIndex);
		for (FTickFunction* TickFunction : TickFunctions)
		{
			if (!TickFunction || TickFunction->GetTickGroup() != CurrentGroup)
			{
				continue;
			}

			if (!TickFunction->CanTick(TickType))
			{
				continue;
			}

			if (!TickFunction->ConsumeInterval(DeltaTime))
			{
				continue;
			}

			TickFunction->ExecuteTick(DeltaTime, TickType);
		}
	}
}

void FTickManager::Reset()
{
	TickFunctions.clear();
}

void FTickManager::GatherTickFunctions(UWorld* World, ELevelTick TickType)
{
	TickFunctions.clear();

	if (!World)
	{
		return;
	}

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		const bool bActorTicks = ShouldDispatchActorTick(Actor, TickType);
		if (bActorTicks)
		{
			QueueTickFunction(Actor->PrimaryActorTick);
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component)
			{
				continue;
			}

			// 액터가 안 틱해도 Editor 틱에선 컴포넌트 단위 opt-in(bTickInEditor) 허용
			// — 본 추적 부착물처럼 에디터 편집 중 갱신이 필요한 컴포넌트용.
			const bool bComponentTicks = bActorTicks ||
				(TickType == LEVELTICK_ViewportsOnly && Component->bTickInEditor);
			if (bComponentTicks)
			{
				QueueTickFunction(Component->PrimaryComponentTick);
			}
		}
	}
}

void FTickManager::QueueTickFunction(FTickFunction& TickFunction)
{
	if (!TickFunction.bRegistered)
	{
		TickFunction.RegisterTickFunction();
	}

	TickFunctions.push_back(&TickFunction);
}

void FActorTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickActor(DeltaTime, TickType, *this);
	}
}

const char* FActorTickFunction::GetDebugName() const
{
	return Target ? Target->GetClass()->GetName() : "FActorTickFunction";
}

void FActorComponentTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType)
{
	if (Target)
	{
		Target->TickComponent(DeltaTime, TickType, *this);
	}
}

const char* FActorComponentTickFunction::GetDebugName() const
{
	return Target ? Target->GetClass()->GetName() : "FActorComponentTickFunction";
}
