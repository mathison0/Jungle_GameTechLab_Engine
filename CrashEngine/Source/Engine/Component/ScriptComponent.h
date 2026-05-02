#pragma once
#include "ActorComponent.h"
#include "sol.hpp"
#include "Engine/LUA/LuaCoroutine.h"

class UScriptComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UScriptComponent, UActorComponent)
	
	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

private:
	bool LoadScript();
	void CallLuaFunction(const char* Name);
	void CallLuaTick(float DeltaTime);

	void BindFunctions();

    template <typename TFunc>
    void BindFunction(const char* Name, TFunc&& Function)
    {
        if (!ScriptInstance.valid())
        {
            return;
        }

        ScriptInstance.set_function(Name, std::forward<TFunc>(Function));
    }

private:
	sol::table ScriptInstance;
	FString ScriptPath;

	FCoroutineExecutorSet CoroutineExecutorSet;
};
