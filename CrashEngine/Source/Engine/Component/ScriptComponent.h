#pragma once
#include "ActorComponent.h"
#include "sol.hpp"
#include "Engine/LUA/LuaCoroutine.h"
#include "Engine/Scripting/LuaScriptTypes.h"

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
	void SetScriptPath(const FString& InScriptPath);
	const FString& GetScriptPath() const { return ScriptPath; }

private:
	bool LoadScript();
	void SyncScriptPropertiesWithAsset();
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
	TArray<FLuaScriptPropertyOverride> ScriptProperties;
	FString SyncedScriptPath;
	uint64 SyncedScriptVersion = 0;

	FCoroutineExecutorSet CoroutineExecutorSet;
};
