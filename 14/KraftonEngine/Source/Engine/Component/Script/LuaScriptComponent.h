#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"
#include "Math/Vector.h"
#include "Source/Engine/Component/Script/LuaScriptComponent.generated.h"
#include <sol/sol.hpp>

class UPrimitiveComponent;
struct FHitResult;

enum class ELuaEditorPropertyType : uint8
{
	Unknown,
	Bool,
	Int,
	Float,
	String,
	Vector,
	Enum,
};

USTRUCT()
struct FLuaScriptPropertyOverride
{
	GENERATED_BODY()

	UPROPERTY(Save, Category="Lua")
	FString Name;

	UPROPERTY(Save, Category="Lua")
	FString Type;

	UPROPERTY(Save, Category="Lua")
	bool BoolValue = false;

	UPROPERTY(Save, Category="Lua")
	int32 IntValue = 0;

	UPROPERTY(Save, Category="Lua")
	float FloatValue = 0.0f;

	UPROPERTY(Save, Category="Lua")
	FString StringValue;

	UPROPERTY(Save, Category="Lua")
	FVector VectorValue = FVector::ZeroVector;

	UPROPERTY(Save, Category="Lua")
	FString EnumType;

	UPROPERTY(Save, Category="Lua")
	FString EnumValue;
};

struct FLuaEditorPropertyDescriptor
{
	FString Name;
	FString DisplayName;
	ELuaEditorPropertyType Type = ELuaEditorPropertyType::Unknown;
	FString EnumType;
	TArray<FString> Options;
	bool BoolDefault = false;
	int32 IntDefault = 0;
	float FloatDefault = 0.0f;
	FString StringDefault;
	FVector VectorDefault = FVector::ZeroVector;
	FString EnumDefault;
	bool bHasMin = false;
	bool bHasMax = false;
	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;
};

UCLASS()
class ULuaScriptComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	ULuaScriptComponent();
	~ULuaScriptComponent();

	void InitializeLua();
	void ReloadScript();

	virtual void BeginPlay() override;
	virtual void EndPlay() override;


	void PreGetEditableProperties() override;
	void PostEditProperty(const char* PropertyName) override;
	const FString& GetScriptFile() const { return ScriptFile; }
	void SetScriptFile(const FString& InScriptFile);
	void DispatchOverlap(class AActor* OtherActor);

	const TArray<FLuaEditorPropertyDescriptor>& GetLuaEditorPropertyDescriptors();
	FLuaScriptPropertyOverride GetLuaEditorPropertyValue(const FLuaEditorPropertyDescriptor& Descriptor) const;
	void SetLuaEditorPropertyValue(const FLuaEditorPropertyDescriptor& Descriptor, const FLuaScriptPropertyOverride& Value);
	void ResetLuaEditorPropertyOverride(const FString& PropertyName);
	bool HasLuaEditorPropertyOverride(const FString& PropertyName) const;

	// Lua script 의 환경(env)에서 인자 없는 전역 함수 하나를 호출. 함수가 없거나
	// nil 이면 조용히 false 반환 — 호출자는 lua 쪽 함수 정의 여부에 신경 쓸 필요 없음.
	bool CallFunction(const FString& FunctionName);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

private:
	void EnsureDefaultScriptFile();
	void BindOwnerCollisionEvents();
	void ClearCollisionBindings();
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& HitResult);
	void HandleEndHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp);

	UPROPERTY(Edit, Save, Category="Script", DisplayName="ScriptFile", AssetType="Script")
	FString ScriptFile;

	UPROPERTY(Save, Category="Script", Type=Array, Struct=FLuaScriptPropertyOverride)
	TArray<FLuaScriptPropertyOverride> LuaPropertyOverrides;
	
	sol::environment Env;
	sol::protected_function LuaBeginPlay;
	sol::protected_function LuaTick;
	sol::protected_function LuaEndPlay;
	sol::protected_function LuaOnOverlap;
	sol::protected_function LuaOnEndOverlap;
	sol::protected_function LuaOnHit;
	sol::protected_function LuaOnEndHit;
	TArray<UPrimitiveComponent*> BoundOverlapComponents;
	TArray<UPrimitiveComponent*> BoundHitComponents;
	TArray<FDelegateHandle> BeginOverlapHandles;
	TArray<FDelegateHandle> EndOverlapHandles;
	TArray<FDelegateHandle> HitHandles;
	TArray<FDelegateHandle> EndHitHandles;

	TArray<FLuaEditorPropertyDescriptor> LuaEditorPropertyDescriptors;
	bool bLuaEditorPropertiesDirty = true;

	void MarkLuaEditorPropertiesDirty();
	void RefreshLuaEditorPropertyDescriptors();
	void ParseLuaEditorProperties(sol::table EditorProperties);
	void ApplyLuaEditorPropertiesToEnv();
	void ApplyLuaEditorPropertyToEnv(const FLuaEditorPropertyDescriptor& Descriptor, const FLuaScriptPropertyOverride& Value);
	const FLuaScriptPropertyOverride* FindLuaEditorPropertyOverride(const FString& PropertyName) const;
	FLuaScriptPropertyOverride* FindLuaEditorPropertyOverride(const FString& PropertyName);
};
