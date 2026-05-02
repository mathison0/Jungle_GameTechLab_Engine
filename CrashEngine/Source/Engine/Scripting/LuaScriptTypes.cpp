#include "Engine/Scripting/LuaScriptTypes.h"

#include "Serialization/Archive.h"

FArchive& operator<<(FArchive& Ar, FLuaScriptValue& Value)
{
    Ar << Value.Type;
    Ar << Value.BoolValue;
    Ar << Value.IntValue;
    Ar << Value.FloatValue;
    Ar << Value.StringValue;
	Ar << Value.Vec3Value;
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FLuaScriptPropertyOverride& Override)
{
    Ar << Override.Name;
    Ar << Override.Value;
    return Ar;
}
