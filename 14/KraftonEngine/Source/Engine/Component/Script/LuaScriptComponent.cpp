#include "LuaScriptComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "Lua/LuaScriptManager.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

namespace
{
	FString ToLowerCopy(FString Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});
		return Value;
	}

	bool IsValidLuaObject(const sol::object& Object)
	{
		return Object.valid() && Object.get_type() != sol::type::nil;
	}

	bool TryReadLuaString(const sol::object& Object, FString& OutValue)
	{
		if (!IsValidLuaObject(Object) || Object.get_type() != sol::type::string)
		{
			return false;
		}

		OutValue = Object.as<FString>();
		return true;
	}

	FString ReadLuaStringOr(const sol::object& Object, const FString& DefaultValue)
	{
		FString Result;
		return TryReadLuaString(Object, Result) ? Result : DefaultValue;
	}

	bool TryReadLuaBool(const sol::object& Object, bool& OutValue)
	{
		if (!IsValidLuaObject(Object) || Object.get_type() != sol::type::boolean)
		{
			return false;
		}

		OutValue = Object.as<bool>();
		return true;
	}

	bool TryReadLuaFloat(const sol::object& Object, float& OutValue)
	{
		if (!IsValidLuaObject(Object) || Object.get_type() != sol::type::number)
		{
			return false;
		}

		OutValue = static_cast<float>(Object.as<double>());
		return true;
	}

	bool TryReadLuaInt(const sol::object& Object, int32& OutValue)
	{
		float FloatValue = 0.0f;
		if (!TryReadLuaFloat(Object, FloatValue))
		{
			return false;
		}

		OutValue = static_cast<int32>(FloatValue);
		return true;
	}

	bool TryReadVectorField(sol::table Table, int Index, const char* LowerName, const char* UpperName, float& OutValue)
	{
		if (TryReadLuaFloat(Table[Index], OutValue))
		{
			return true;
		}
		if (TryReadLuaFloat(Table[LowerName], OutValue))
		{
			return true;
		}
		return TryReadLuaFloat(Table[UpperName], OutValue);
	}

	bool TryReadLuaVector(const sol::object& Object, FVector& OutValue)
	{
		if (!IsValidLuaObject(Object) || Object.get_type() != sol::type::table)
		{
			return false;
		}

		sol::table Table = Object.as<sol::table>();
		FVector Result = FVector::ZeroVector;
		TryReadVectorField(Table, 1, "x", "X", Result.X);
		TryReadVectorField(Table, 2, "y", "Y", Result.Y);
		TryReadVectorField(Table, 3, "z", "Z", Result.Z);
		OutValue = Result;
		return true;
	}

	ELuaEditorPropertyType LuaEditorPropertyTypeFromString(const FString& TypeName)
	{
		const FString LowerType = ToLowerCopy(TypeName);
		if (LowerType == "bool" || LowerType == "boolean")
		{
			return ELuaEditorPropertyType::Bool;
		}
		if (LowerType == "int" || LowerType == "integer")
		{
			return ELuaEditorPropertyType::Int;
		}
		if (LowerType == "float" || LowerType == "number")
		{
			return ELuaEditorPropertyType::Float;
		}
		if (LowerType == "string")
		{
			return ELuaEditorPropertyType::String;
		}
		if (LowerType == "vector" || LowerType == "vec3")
		{
			return ELuaEditorPropertyType::Vector;
		}
		if (LowerType == "enum")
		{
			return ELuaEditorPropertyType::Enum;
		}
		return ELuaEditorPropertyType::Unknown;
	}

	const char* LuaEditorPropertyTypeToString(ELuaEditorPropertyType Type)
	{
		switch (Type)
		{
		case ELuaEditorPropertyType::Bool: return "bool";
		case ELuaEditorPropertyType::Int: return "int";
		case ELuaEditorPropertyType::Float: return "float";
		case ELuaEditorPropertyType::String: return "string";
		case ELuaEditorPropertyType::Vector: return "vector";
		case ELuaEditorPropertyType::Enum: return "enum";
		default: return "unknown";
		}
	}

	FLuaScriptPropertyOverride MakeDefaultLuaPropertyValue(const FLuaEditorPropertyDescriptor& Descriptor)
	{
		FLuaScriptPropertyOverride Value;
		Value.Name = Descriptor.Name;
		Value.Type = LuaEditorPropertyTypeToString(Descriptor.Type);
		Value.BoolValue = Descriptor.BoolDefault;
		Value.IntValue = Descriptor.IntDefault;
		Value.FloatValue = Descriptor.FloatDefault;
		Value.StringValue = Descriptor.StringDefault;
		Value.VectorValue = Descriptor.VectorDefault;
		Value.EnumType = Descriptor.EnumType;
		Value.EnumValue = Descriptor.EnumDefault;
		return Value;
	}

	bool IsOverrideCompatible(const FLuaScriptPropertyOverride& Override, const FLuaEditorPropertyDescriptor& Descriptor)
	{
		if (Override.Name != Descriptor.Name)
		{
			return false;
		}

		if (Override.Type != LuaEditorPropertyTypeToString(Descriptor.Type))
		{
			return false;
		}

		if (Descriptor.Type == ELuaEditorPropertyType::Enum && !Descriptor.EnumType.empty() && !Override.EnumType.empty())
		{
			return Override.EnumType == Descriptor.EnumType;
		}

		return true;
	}
}

ULuaScriptComponent::ULuaScriptComponent()
{
}

ULuaScriptComponent::~ULuaScriptComponent()
{
}

void ULuaScriptComponent::InitializeLua()
{
	LuaEditorPropertyDescriptors.clear();
	bLuaEditorPropertiesDirty = false;

	LuaBeginPlay = sol::nil;
	LuaTick = sol::nil;
	LuaEndPlay = sol::nil;
	LuaOnOverlap = sol::nil;
	LuaOnEndOverlap = sol::nil;
	LuaOnHit = sol::nil;
	LuaOnEndHit = sol::nil;

	sol::state& Lua = FLuaScriptManager::GetState();

	Env = sol::environment(Lua, sol::create, Lua.globals());
	Env["obj"] = GetOwner();
	Env["this"] = this;

	const FString ResolvedPath = FLuaScriptManager::ResolveScriptPath(ScriptFile);
	// 한글 경로 호환 — safe_script_file 은 fopen(UTF-8) 경로라 ANSI 코드페이지에서 깨짐.
	// wide ifstream 으로 읽어 safe_script(string, env, ...) 로 우회.
	FString Content;
	if (!FLuaScriptManager::ReadScriptFileContent(ScriptFile, Content))
	{
		UE_LOG("Failed to read Lua script %s", ResolvedPath.c_str());
		return;
	}
	sol::protected_function_result Result = Lua.safe_script(Content, Env, sol::script_pass_on_error, ResolvedPath);

	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("Failed to load Lua script %s: %s", ScriptFile.c_str(), Err.what());
		return;
	}

	sol::object EditorPropertiesObject = Env["EditorProperties"];
	if (IsValidLuaObject(EditorPropertiesObject) && EditorPropertiesObject.get_type() == sol::type::table)
	{
		ParseLuaEditorProperties(EditorPropertiesObject.as<sol::table>());
		ApplyLuaEditorPropertiesToEnv();
	}

	LuaBeginPlay = Env["BeginPlay"];
	LuaTick = Env["Tick"];
	LuaEndPlay = Env["EndPlay"];
	LuaOnOverlap = Env["OnOverlap"];
	LuaOnEndOverlap = Env["OnEndOverlap"];
	LuaOnHit = Env["OnHit"];
	LuaOnEndHit = Env["OnEndHit"];
}

void ULuaScriptComponent::ReloadScript()
{
	ClearCollisionBindings();
	InitializeLua();

	if (LuaBeginPlay)
	{
		sol::protected_function_result Result = LuaBeginPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua BeginPlay error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}

	BindOwnerCollisionEvents();
}

void ULuaScriptComponent::BeginPlay()
{
	EnsureDefaultScriptFile();
	UActorComponent::BeginPlay();

	InitializeLua();
	FLuaScriptManager::RegisterComponent(this);

	if (LuaBeginPlay)
	{
		sol::protected_function_result Result = LuaBeginPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua BeginPlay error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}

	BindOwnerCollisionEvents();
}

void ULuaScriptComponent::EndPlay()
{
	UActorComponent::EndPlay();
	FLuaScriptManager::UnregisterComponent(this);
	ClearCollisionBindings();
	if (LuaEndPlay)
	{
		sol::protected_function_result Result = LuaEndPlay();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua EndPlay error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::BindOwnerCollisionEvents()
{
	ClearCollisionBindings();

	if (!LuaOnOverlap && !LuaOnEndOverlap && !LuaOnHit && !LuaOnEndHit)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	for (UPrimitiveComponent* PrimitiveComponent : OwnerActor->GetPrimitiveComponents())
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		if ((LuaOnOverlap || LuaOnEndOverlap) && PrimitiveComponent->GetGenerateOverlapEvents())
		{
			BoundOverlapComponents.push_back(PrimitiveComponent);
			BeginOverlapHandles.push_back(PrimitiveComponent->OnComponentBeginOverlap.AddRaw(this, &ULuaScriptComponent::HandleBeginOverlap));
			EndOverlapHandles.push_back(PrimitiveComponent->OnComponentEndOverlap.AddRaw(this, &ULuaScriptComponent::HandleEndOverlap));
		}

		if (LuaOnHit || LuaOnEndHit)
		{
			BoundHitComponents.push_back(PrimitiveComponent);
			HitHandles.push_back(LuaOnHit
				? PrimitiveComponent->OnComponentHit.AddRaw(this, &ULuaScriptComponent::HandleHit)
				: FDelegateHandle());
			EndHitHandles.push_back(LuaOnEndHit
				? PrimitiveComponent->OnComponentEndHit.AddRaw(this, &ULuaScriptComponent::HandleEndHit)
				: FDelegateHandle());
		}
	}
}

void ULuaScriptComponent::ClearCollisionBindings()
{
	for (int32 Index = 0; Index < static_cast<int32>(BoundOverlapComponents.size()); ++Index)
	{
		UPrimitiveComponent* PrimitiveComponent = BoundOverlapComponents[Index];
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (Index < static_cast<int32>(BeginOverlapHandles.size()) && BeginOverlapHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentBeginOverlap.Remove(BeginOverlapHandles[Index]);
		}

		if (Index < static_cast<int32>(EndOverlapHandles.size()) && EndOverlapHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentEndOverlap.Remove(EndOverlapHandles[Index]);
		}
	}

	BoundOverlapComponents.clear();
	BeginOverlapHandles.clear();
	EndOverlapHandles.clear();

	for (int32 Index = 0; Index < static_cast<int32>(BoundHitComponents.size()); ++Index)
	{
		UPrimitiveComponent* PrimitiveComponent = BoundHitComponents[Index];
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (Index < static_cast<int32>(HitHandles.size()) && HitHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentHit.Remove(HitHandles[Index]);
		}

		if (Index < static_cast<int32>(EndHitHandles.size()) && EndHitHandles[Index].IsValid())
		{
			PrimitiveComponent->OnComponentEndHit.Remove(EndHitHandles[Index]);
		}
	}

	BoundHitComponents.clear();
	HitHandles.clear();
	EndHitHandles.clear();
}

void ULuaScriptComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (LuaOnOverlap)
	{
		sol::protected_function_result Result = LuaOnOverlap(OtherActor, OverlappedComponent, OtherComp);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnOverlap error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 /*OtherBodyIndex*/)
{
	if (LuaOnEndOverlap)
	{
		sol::protected_function_result Result = LuaOnEndOverlap(OtherActor, OverlappedComponent, OtherComp);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnEndOverlap error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::HandleHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& HitResult)
{
	if (LuaOnHit)
	{
		sol::protected_function_result Result = LuaOnHit(OtherActor, HitComponent, OtherComp, NormalImpulse, HitResult);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnHit error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::HandleEndHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp)
{
	if (LuaOnEndHit)
	{
		sol::protected_function_result Result = LuaOnEndHit(OtherActor, HitComponent, OtherComp);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnEndHit error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

bool ULuaScriptComponent::CallFunction(const FString& FunctionName)
{
	if (!Env.valid())
	{
		return false;
	}

	sol::object Target = Env[FunctionName.c_str()];
	if (!Target.valid() || Target.get_type() != sol::type::function)
	{
		return false;
	}

	sol::protected_function Fn = Target;
	sol::protected_function_result Result = Fn();
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("Lua %s error in %s: %s", FunctionName.c_str(), ScriptFile.c_str(), Err.what());
		return false;
	}
	return true;
}

void ULuaScriptComponent::DispatchOverlap(AActor* OtherActor)
{
	if (LuaOnOverlap)
	{
		sol::protected_function_result Result = LuaOnOverlap(OtherActor);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua OnOverlap error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (LuaTick)
	{
		sol::protected_function_result Result = LuaTick(DeltaTime);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("Lua Tick error in %s: %s", ScriptFile.c_str(), Err.what());
		}
	}
}

void ULuaScriptComponent::PreGetEditableProperties()
{
	UActorComponent::PreGetEditableProperties();
	EnsureDefaultScriptFile();
}

void ULuaScriptComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	if (!PropertyName)
	{
		return;
	}

	if (std::strcmp(PropertyName, "ScriptFile") == 0)
	{
		MarkLuaEditorPropertiesDirty();
		if (Env.valid())
		{
			ReloadScript();
		}
	}
}

void ULuaScriptComponent::SetScriptFile(const FString& InScriptFile)
{
	if (ScriptFile == InScriptFile)
	{
		return;
	}

	ScriptFile = InScriptFile;
	MarkLuaEditorPropertiesDirty();
}

const TArray<FLuaEditorPropertyDescriptor>& ULuaScriptComponent::GetLuaEditorPropertyDescriptors()
{
	EnsureDefaultScriptFile();
	if (bLuaEditorPropertiesDirty)
	{
		RefreshLuaEditorPropertyDescriptors();
	}
	return LuaEditorPropertyDescriptors;
}

FLuaScriptPropertyOverride ULuaScriptComponent::GetLuaEditorPropertyValue(const FLuaEditorPropertyDescriptor& Descriptor) const
{
	if (const FLuaScriptPropertyOverride* Override = FindLuaEditorPropertyOverride(Descriptor.Name))
	{
		if (IsOverrideCompatible(*Override, Descriptor))
		{
			return *Override;
		}
	}

	return MakeDefaultLuaPropertyValue(Descriptor);
}

void ULuaScriptComponent::SetLuaEditorPropertyValue(const FLuaEditorPropertyDescriptor& Descriptor, const FLuaScriptPropertyOverride& Value)
{
	FLuaScriptPropertyOverride NewValue = Value;
	NewValue.Name = Descriptor.Name;
	NewValue.Type = LuaEditorPropertyTypeToString(Descriptor.Type);
	if (Descriptor.Type == ELuaEditorPropertyType::Enum)
	{
		NewValue.EnumType = Descriptor.EnumType;
	}

	FLuaScriptPropertyOverride* Existing = FindLuaEditorPropertyOverride(Descriptor.Name);
	if (Existing)
	{
		*Existing = NewValue;
	}
	else
	{
		LuaPropertyOverrides.push_back(NewValue);
	}

	ApplyLuaEditorPropertyToEnv(Descriptor, NewValue);
}

void ULuaScriptComponent::ResetLuaEditorPropertyOverride(const FString& PropertyName)
{
	for (auto It = LuaPropertyOverrides.begin(); It != LuaPropertyOverrides.end(); ++It)
	{
		if (It->Name == PropertyName)
		{
			LuaPropertyOverrides.erase(It);
			break;
		}
	}

	for (const FLuaEditorPropertyDescriptor& Descriptor : LuaEditorPropertyDescriptors)
	{
		if (Descriptor.Name == PropertyName)
		{
			ApplyLuaEditorPropertyToEnv(Descriptor, MakeDefaultLuaPropertyValue(Descriptor));
			break;
		}
	}
}

bool ULuaScriptComponent::HasLuaEditorPropertyOverride(const FString& PropertyName) const
{
	return FindLuaEditorPropertyOverride(PropertyName) != nullptr;
}

void ULuaScriptComponent::EnsureDefaultScriptFile()
{
	if (!ScriptFile.empty())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->GetFName().IsValid())
	{
		return;
	}

	ULevel* Level = OwnerActor->GetLevel();
	if (!Level || !Level->GetFName().IsValid())
	{
		return;
	}

	ScriptFile = Level->GetFName().ToString() + "_" + OwnerActor->GetFName().ToString() + ".lua";
}

void ULuaScriptComponent::MarkLuaEditorPropertiesDirty()
{
	bLuaEditorPropertiesDirty = true;
	LuaEditorPropertyDescriptors.clear();
}

void ULuaScriptComponent::RefreshLuaEditorPropertyDescriptors()
{
	bLuaEditorPropertiesDirty = false;
	LuaEditorPropertyDescriptors.clear();

	if (ScriptFile.empty() || ScriptFile == "None")
	{
		return;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	sol::environment PreviewEnv(Lua, sol::create, Lua.globals());
	PreviewEnv["obj"] = GetOwner();
	PreviewEnv["this"] = this;

	FString Content;
	if (!FLuaScriptManager::ReadScriptFileContent(ScriptFile, Content))
	{
		return;
	}

	const FString ResolvedPath = FLuaScriptManager::ResolveScriptPath(ScriptFile);
	sol::protected_function_result Result = Lua.safe_script(Content, PreviewEnv, sol::script_pass_on_error, ResolvedPath);
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("Failed to scan Lua editor properties %s: %s", ScriptFile.c_str(), Err.what());
		return;
	}

	sol::object EditorPropertiesObject = PreviewEnv["EditorProperties"];
	if (IsValidLuaObject(EditorPropertiesObject) && EditorPropertiesObject.get_type() == sol::type::table)
	{
		ParseLuaEditorProperties(EditorPropertiesObject.as<sol::table>());
	}
}

void ULuaScriptComponent::ParseLuaEditorProperties(sol::table EditorProperties)
{
	LuaEditorPropertyDescriptors.clear();

	for (auto&& Pair : EditorProperties)
	{
		FString PropertyName;
		if (!TryReadLuaString(Pair.first, PropertyName) || PropertyName.empty())
		{
			continue;
		}

		if (!IsValidLuaObject(Pair.second) || Pair.second.get_type() != sol::type::table)
		{
			continue;
		}

		sol::table Definition = Pair.second.as<sol::table>();
		const FString TypeName = ReadLuaStringOr(Definition["type"], "");
		const ELuaEditorPropertyType PropertyType = LuaEditorPropertyTypeFromString(TypeName);
		if (PropertyType == ELuaEditorPropertyType::Unknown)
		{
			UE_LOG("[LuaEditorProperty] Unsupported type '%s' for %s in %s", TypeName.c_str(), PropertyName.c_str(), ScriptFile.c_str());
			continue;
		}

		FLuaEditorPropertyDescriptor Descriptor;
		Descriptor.Name = PropertyName;
		Descriptor.DisplayName = ReadLuaStringOr(Definition["display"], PropertyName);
		Descriptor.Type = PropertyType;
		Descriptor.EnumType = ReadLuaStringOr(Definition["enum"], "");

		float NumericValue = 0.0f;
		if (TryReadLuaFloat(Definition["min"], NumericValue))
		{
			Descriptor.Min = NumericValue;
			Descriptor.bHasMin = true;
		}
		if (TryReadLuaFloat(Definition["max"], NumericValue))
		{
			Descriptor.Max = NumericValue;
			Descriptor.bHasMax = true;
		}
		if (TryReadLuaFloat(Definition["speed"], NumericValue))
		{
			Descriptor.Speed = NumericValue;
		}

		sol::object DefaultObject = Definition["default"];
		switch (Descriptor.Type)
		{
		case ELuaEditorPropertyType::Bool:
			TryReadLuaBool(DefaultObject, Descriptor.BoolDefault);
			break;
		case ELuaEditorPropertyType::Int:
			TryReadLuaInt(DefaultObject, Descriptor.IntDefault);
			break;
		case ELuaEditorPropertyType::Float:
			TryReadLuaFloat(DefaultObject, Descriptor.FloatDefault);
			break;
		case ELuaEditorPropertyType::String:
			TryReadLuaString(DefaultObject, Descriptor.StringDefault);
			break;
		case ELuaEditorPropertyType::Vector:
			TryReadLuaVector(DefaultObject, Descriptor.VectorDefault);
			break;
		case ELuaEditorPropertyType::Enum:
		{
			TryReadLuaString(DefaultObject, Descriptor.EnumDefault);

			sol::object OptionsObject = Definition["options"];
			if (IsValidLuaObject(OptionsObject) && OptionsObject.get_type() == sol::type::table)
			{
				sol::table OptionsTable = OptionsObject.as<sol::table>();
				for (auto&& OptionPair : OptionsTable)
				{
					FString Option;
					if (TryReadLuaString(OptionPair.second, Option) && !Option.empty())
					{
						Descriptor.Options.push_back(Option);
					}
				}
			}

			if (Descriptor.EnumDefault.empty() && !Descriptor.Options.empty())
			{
				Descriptor.EnumDefault = Descriptor.Options.front();
			}
			break;
		}
		default:
			break;
		}

		LuaEditorPropertyDescriptors.push_back(std::move(Descriptor));
	}
}

void ULuaScriptComponent::ApplyLuaEditorPropertiesToEnv()
{
	if (!Env.valid())
	{
		return;
	}

	for (const FLuaEditorPropertyDescriptor& Descriptor : LuaEditorPropertyDescriptors)
	{
		ApplyLuaEditorPropertyToEnv(Descriptor, GetLuaEditorPropertyValue(Descriptor));
	}
}

void ULuaScriptComponent::ApplyLuaEditorPropertyToEnv(const FLuaEditorPropertyDescriptor& Descriptor, const FLuaScriptPropertyOverride& Value)
{
	if (!Env.valid() || Descriptor.Name.empty())
	{
		return;
	}

	switch (Descriptor.Type)
	{
	case ELuaEditorPropertyType::Bool:
		Env[Descriptor.Name.c_str()] = Value.BoolValue;
		break;
	case ELuaEditorPropertyType::Int:
		Env[Descriptor.Name.c_str()] = Value.IntValue;
		break;
	case ELuaEditorPropertyType::Float:
		Env[Descriptor.Name.c_str()] = Value.FloatValue;
		break;
	case ELuaEditorPropertyType::String:
		Env[Descriptor.Name.c_str()] = Value.StringValue;
		break;
	case ELuaEditorPropertyType::Vector:
		Env[Descriptor.Name.c_str()] = Value.VectorValue;
		break;
	case ELuaEditorPropertyType::Enum:
	{
		const FString EnumType = !Value.EnumType.empty() ? Value.EnumType : Descriptor.EnumType;
		if (!EnumType.empty() && !Value.EnumValue.empty())
		{
			sol::object EnumTableObject = Env[EnumType.c_str()];
			if (!IsValidLuaObject(EnumTableObject))
			{
				EnumTableObject = FLuaScriptManager::GetState()[EnumType.c_str()];
			}

			if (IsValidLuaObject(EnumTableObject) && EnumTableObject.get_type() == sol::type::table)
			{
				sol::table EnumTable = EnumTableObject.as<sol::table>();
				sol::object EnumValueObject = EnumTable[Value.EnumValue.c_str()];
				if (IsValidLuaObject(EnumValueObject))
				{
					Env[Descriptor.Name.c_str()] = EnumValueObject;
					break;
				}
			}
		}

		Env[Descriptor.Name.c_str()] = Value.EnumValue;
		break;
	}
	default:
		break;
	}
}

const FLuaScriptPropertyOverride* ULuaScriptComponent::FindLuaEditorPropertyOverride(const FString& PropertyName) const
{
	for (const FLuaScriptPropertyOverride& Override : LuaPropertyOverrides)
	{
		if (Override.Name == PropertyName)
		{
			return &Override;
		}
	}
	return nullptr;
}

FLuaScriptPropertyOverride* ULuaScriptComponent::FindLuaEditorPropertyOverride(const FString& PropertyName)
{
	for (FLuaScriptPropertyOverride& Override : LuaPropertyOverrides)
	{
		if (Override.Name == PropertyName)
		{
			return &Override;
		}
	}
	return nullptr;
}
