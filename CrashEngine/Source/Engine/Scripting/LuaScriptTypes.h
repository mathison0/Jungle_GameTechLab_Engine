#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class FArchive;

enum class ELuaScriptPropertyType : uint8
{
    Bool,
    Int,
    Float,
    String,
	Vec3,
};

struct FLuaScriptValue
{
    ELuaScriptPropertyType Type = ELuaScriptPropertyType::Float;
    bool BoolValue = false;
    int32 IntValue = 0;
    float FloatValue = 0.0f;
    FString StringValue;
	FVector	Vec3Value;
};

struct FLuaScriptPropertyDesc
{
    FString Name;
    ELuaScriptPropertyType Type = ELuaScriptPropertyType::Float;
    FLuaScriptValue DefaultValue;
    float Min = 0.0f;
    float Max = 0.0f;
    float Speed = 0.1f;
};

struct FLuaScriptPropertyOverride
{
    FString Name;
    FLuaScriptValue Value;
};

FArchive& operator<<(FArchive& Ar, FLuaScriptValue& Value);
FArchive& operator<<(FArchive& Ar, FLuaScriptPropertyOverride& Override);
