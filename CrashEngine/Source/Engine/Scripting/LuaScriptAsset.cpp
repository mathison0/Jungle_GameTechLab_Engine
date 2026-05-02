#include "LuaScriptAsset.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "Core/Logging/LogMacros.h"
#include "Platform/Paths.h"

namespace
{
FString ToLowerAscii(FString Value)
{
	std::ranges::transform(Value, Value.begin(),
                           [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
	return Value;
}

bool IsReservedLuaPropertyName(const FString& Name)
{
	return Name.empty() ||
		Name == "properties" ||
		Name == "BeginPlay" ||
		Name == "Tick" ||
		Name == "EndPlay" ||
		Name == "start_coroutine" ||
		Name == "stop_coroutine";
}

bool ParseLuaPropertyTypeName(const FString& TypeName, ELuaScriptPropertyType& OutType)
{
	const FString LowerType = ToLowerAscii(TypeName);
	if (LowerType == "bool" || LowerType == "boolean")
	{
		OutType = ELuaScriptPropertyType::Bool;
		return true;
	}
	if (LowerType == "int" || LowerType == "integer")
	{
		OutType = ELuaScriptPropertyType::Int;
		return true;
	}
	if (LowerType == "float" || LowerType == "number")
	{
		OutType = ELuaScriptPropertyType::Float;
		return true;
	}
	if (LowerType == "string")
	{
		OutType = ELuaScriptPropertyType::String;
		return true;
	}
	if (LowerType == "vec3" || LowerType == "vector3")
	{
		OutType = ELuaScriptPropertyType::Vec3;
		return true;
	}
	return false;
}

bool InferLuaPropertyType(const sol::object& ValueObject, ELuaScriptPropertyType& OutType)
{
	switch (ValueObject.get_type())
	{
	case sol::type::boolean:
		OutType = ELuaScriptPropertyType::Bool;
		return true;
	case sol::type::number:
		OutType = ELuaScriptPropertyType::Float;
		return true;
	case sol::type::string:
		OutType = ELuaScriptPropertyType::String;
		return true;
	default:
		return false;
	}
}

FLuaScriptValue MakeDefaultLuaScriptValue(ELuaScriptPropertyType Type)
{
	FLuaScriptValue Value;
	Value.Type = Type;
	return Value;
}

bool ReadLuaScriptValue(const sol::object& ValueObject, ELuaScriptPropertyType Type, FLuaScriptValue& OutValue)
{
	OutValue = MakeDefaultLuaScriptValue(Type);

	if (!ValueObject.valid() || ValueObject == sol::nil)
	{
		return true;
	}

	switch (Type)
	{
	case ELuaScriptPropertyType::Bool:
		if (ValueObject.get_type() != sol::type::boolean)
		{
			return false;
		}
		OutValue.BoolValue = ValueObject.as<bool>();
		return true;
	case ELuaScriptPropertyType::Int:
		if (ValueObject.get_type() != sol::type::number)
		{
			return false;
		}
		OutValue.IntValue = ValueObject.as<int32>();
		return true;
	case ELuaScriptPropertyType::Float:
		if (ValueObject.get_type() != sol::type::number)
		{
			return false;
		}
		OutValue.FloatValue = ValueObject.as<float>();
		return true;
	case ELuaScriptPropertyType::String:
		if (ValueObject.get_type() != sol::type::string)
		{
			return false;
		}
		OutValue.StringValue = ValueObject.as<FString>();
		return true;
	case ELuaScriptPropertyType::Vec3:
	{
		if (!ValueObject.is<sol::table>())
		{
			return false;
		}

		sol::table Table = ValueObject.as<sol::table>();

		sol::object XObject = Table[1];
		sol::object YObject = Table[2];
		sol::object ZObject = Table[3];

		if (XObject.get_type() != sol::type::number ||
			YObject.get_type() != sol::type::number ||
			ZObject.get_type() != sol::type::number)
		{
			return false;
		}

		OutValue.Vec3Value.X = XObject.as<float>();
		OutValue.Vec3Value.Y = YObject.as<float>();
		OutValue.Vec3Value.Z = ZObject.as<float>();
		return true;
	}
	default:
		return false;
	}
}

void SetLuaTableValue(sol::state& Lua, sol::table Table, const FString& Name, const FLuaScriptValue& Value)
{
	switch (Value.Type)
	{
	case ELuaScriptPropertyType::Bool:
		Table[Name] = Value.BoolValue;
		break;
	case ELuaScriptPropertyType::Int:
		Table[Name] = Value.IntValue;
		break;
	case ELuaScriptPropertyType::Float:
		Table[Name] = Value.FloatValue;
		break;
	case ELuaScriptPropertyType::String:
		Table[Name] = Value.StringValue;
		break;
	case ELuaScriptPropertyType::Vec3:
	{
		sol::table Vec = Lua.create_table();
		Vec[1] = Value.Vec3Value.X;
		Vec[2] = Value.Vec3Value.Y;
		Vec[3] = Value.Vec3Value.Z;
		Vec["x"] = Value.Vec3Value.X;
		Vec["y"] = Value.Vec3Value.Y;
		Vec["z"] = Value.Vec3Value.Z;
		Table[Name] = Vec;
		break;
	}
	default:
		break;
	}
}
} // namespace

FLuaScriptAsset::FLuaScriptAsset(FString InRelativePath, FString InFullPath)
	: RelativePath(std::move(InRelativePath))
	, FullPath(std::move(InFullPath))
{
}

bool FLuaScriptAsset::Load(sol::state& Lua)
{
	const bool bResult = LoadInternal(Lua);
	if (!bResult)
	{
		UE_LOG([Lua], Error, "Failed to load Lua script '%s': %s",
			FullPath.c_str(), LastError.c_str());
	}

	return bResult;
}

bool FLuaScriptAsset::ReloadIfChange(sol::state& Lua)
{
	if (!HasFileChanged())
	{
		return State == ELuaScriptAssetState::Loaded;
	}

	const bool bResult = LoadInternal(Lua);
	if (!bResult)
	{
		UE_LOG([Lua], Error, "Failed to reload Lua script '%s' : %s",
			FullPath.c_str(), LastError.c_str());
	}

	return bResult;
}

bool FLuaScriptAsset::HasFileChanged() const
{
	std::error_code Ec;
	const auto CurrentWriteTime = std::filesystem::last_write_time(FPaths::ToPath(FullPath), Ec);
	const bool bCurrentFileExists = !Ec;

	if (!bHasObservedFileState)
	{
		return true;
	}

	if (bCurrentFileExists != bLastFileExists)
	{
		return true;
	}

	if (!bCurrentFileExists)
	{
		return false;
	}

	return CurrentWriteTime != LastWriteTime;
}

sol::table FLuaScriptAsset::CreateInstance(sol::state& Lua, const TArray<FLuaScriptPropertyOverride>* PropertyOverrides) const
{
	sol::table Instance = Lua.create_table();

	if (!Prototype.valid())
	{
		return Instance;
	}

	for (const auto& Pair : Prototype)
	{
		Instance[Pair.first] = Pair.second;
	}

	for (const FLuaScriptPropertyDesc& Desc : PropertyDescriptors)
	{
		SetLuaTableValue(Lua, Instance, Desc.Name, Desc.DefaultValue);
	}

	if (PropertyOverrides)
	{
		for (const FLuaScriptPropertyOverride& Override : *PropertyOverrides)
		{
			SetLuaTableValue(Lua, Instance, Override.Name, Override.Value);
		}
	}

	Instance["__assetPath"] = RelativePath;
	Instance["__assetFullPath"] = FullPath;
	Instance["__assetVersion"] = Version;

	return Instance;
}

bool FLuaScriptAsset::LoadInternal(sol::state& Lua)
{
	LastError.clear();

	const std::filesystem::path ScriptFilePath = FPaths::ToPath(FullPath);
	std::ifstream File(ScriptFilePath, std::ios::binary);
	if (!File.is_open())
	{
		LastError = "Failed to open Lua script";
		std::error_code ExistsEc;
		const bool bExists = std::filesystem::exists(ScriptFilePath, ExistsEc);
		State = (!ExistsEc && !bExists) ? ELuaScriptAssetState::Missing : ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	FString NewSourceCode = Buffer.str();

	const FString ChunkName = "@" + RelativePath;

	sol::environment Env(Lua, sol::create, Lua.globals());
	sol::load_result Loaded = Lua.load(NewSourceCode, ChunkName);

	if (!Loaded.valid())
	{
		sol::error Err = Loaded;
		LastError = Err.what();
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::protected_function Function = Loaded;
	
	if (!sol::set_environment(Env, Function))
	{
		LastError = "Failed to set Lua environment";
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::protected_function_result Result = Function();

	if (!Result.valid())
	{
		sol::error Err = Result;
		LastError = Err.what();
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::object Returned = Result.get<sol::object>();
	if (!Returned.is<sol::table>())
	{
		LastError = "Lua script must return a table";
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	sol::table NewPrototype = Returned.as<sol::table>();
	TArray<FLuaScriptPropertyDesc> NewPropertyDescriptors;
	if (!ParsePropertyDescriptors(NewPrototype, NewPropertyDescriptors))
	{
		State = ELuaScriptAssetState::Error;
		UpdateObservedFileState();
		return false;
	}

	SourceCode = std::move(NewSourceCode);
	Prototype = NewPrototype;
	PropertyDescriptors = std::move(NewPropertyDescriptors);
	UpdateObservedFileState();
	++Version;
	State = ELuaScriptAssetState::Loaded;

	UE_LOG([Lua], Info, "Loaded Lua script: %s", RelativePath.c_str());
	return true;
}

bool FLuaScriptAsset::UpdateObservedFileState()
{
	std::error_code Ec;
	LastWriteTime = std::filesystem::last_write_time(FPaths::ToPath(FullPath), Ec);
	bHasObservedFileState = true;
	bLastFileExists = !Ec;
	if (!bLastFileExists)
	{
		LastWriteTime = {};
	}
	return bLastFileExists;
}

bool FLuaScriptAsset::ParsePropertyDescriptors(sol::table ScriptTable, TArray<FLuaScriptPropertyDesc>& OutDescriptors)
{
	OutDescriptors.clear();

	sol::object PropertiesObject = ScriptTable["properties"];
	if (!PropertiesObject.valid() || PropertiesObject == sol::nil)
	{
		return true;
	}

	if (!PropertiesObject.is<sol::table>())
	{
		LastError = "Lua script 'properties' must be a table";
		return false;
	}

	sol::table PropertiesTable = PropertiesObject.as<sol::table>();
	for (const auto& Pair : PropertiesTable)
	{
		sol::object KeyObject = Pair.first;
		if (!KeyObject.is<FString>())
		{
			LastError = "Lua script property names must be strings";
			return false;
		}

		FLuaScriptPropertyDesc Desc;
		Desc.Name = KeyObject.as<FString>();
		if (IsReservedLuaPropertyName(Desc.Name))
		{
			LastError = "Lua script property name is reserved: " + Desc.Name;
			return false;
		}

		sol::object DescriptorObject = Pair.second;
		if (DescriptorObject.is<sol::table>())
		{
			sol::table DescriptorTable = DescriptorObject.as<sol::table>();

			sol::object TypeObject = DescriptorTable["type"];
			sol::object DefaultObject = DescriptorTable["default"];
			if (TypeObject.valid() && TypeObject != sol::nil)
			{
				if (!TypeObject.is<FString>() || !ParseLuaPropertyTypeName(TypeObject.as<FString>(), Desc.Type))
				{
					LastError = "Lua script property has unsupported type: " + Desc.Name;
					return false;
				}
			}
			else if (!InferLuaPropertyType(DefaultObject, Desc.Type))
			{
				LastError = "Lua script property needs a type or default value: " + Desc.Name;
				return false;
			}

			if (!ReadLuaScriptValue(DefaultObject, Desc.Type, Desc.DefaultValue))
			{
				LastError = "Lua script property default value does not match type: " + Desc.Name;
				return false;
			}

			sol::object MinObject = DescriptorTable["min"];
			sol::object MaxObject = DescriptorTable["max"];
			sol::object SpeedObject = DescriptorTable["speed"];
			if (MinObject.valid() && MinObject.get_type() == sol::type::number)
			{
				Desc.Min = MinObject.as<float>();
			}
			if (MaxObject.valid() && MaxObject.get_type() == sol::type::number)
			{
				Desc.Max = MaxObject.as<float>();
			}
			if (SpeedObject.valid() && SpeedObject.get_type() == sol::type::number)
			{
				Desc.Speed = SpeedObject.as<float>();
			}
		}
		else
		{
			if (!InferLuaPropertyType(DescriptorObject, Desc.Type))
			{
				LastError = "Lua script property descriptor must be a supported value or table: " + Desc.Name;
				return false;
			}
			if (!ReadLuaScriptValue(DescriptorObject, Desc.Type, Desc.DefaultValue))
			{
				LastError = "Lua script property value is invalid: " + Desc.Name;
				return false;
			}
		}

		OutDescriptors.push_back(std::move(Desc));
	}

	return true;
}
