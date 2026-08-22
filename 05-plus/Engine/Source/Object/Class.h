#pragma once
#include "CoreMinimal.h"
class UObject;

class ENGINE_API UClass
{
public:
	using CreateFunc = UObject * (*)(UObject* InOuter, const FString& InName);

	UClass(FString InName, UClass* InSuperClass, CreateFunc InCreateFunc);
	
	const FString& GetName() const;
	UClass* GetSuperClass() const;


	bool IsChildOf(const UClass* Other) const;

	UObject* CreateInstance(UObject* InOuter, const FString& InName) const;
	static UClass* FindClass(const FString& InString);
	static const TMap<FString, UClass*>& GetRegisteredClasses();
	static void RegisterClass(UClass* InClass);
	static void RegisterAlias(const FString& AliasName, UClass* InClass);
private:
	FString Name;
	UClass* SuperClass = nullptr;
	CreateFunc Factory = nullptr;
	static TMap<FString, UClass*>& GetClassRegistry();
};

