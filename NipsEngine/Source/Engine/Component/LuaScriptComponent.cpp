#include "Component/LuaScriptComponent.h"

#include "Component/PrimitiveComponent.h"
#include "Core/Paths.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include "Scripting/LuaScriptSystem.h"

#include <filesystem>
#include <fstream>
#include <cstring>

DEFINE_CLASS(ULuaScriptComponent, UActorComponent)
REGISTER_FACTORY(ULuaScriptComponent)

namespace
{
	FString SanitizeScriptName(FString Name)
	{
		if (Name.empty())
		{
			return "Actor";
		}

		for (char& Ch : Name)
		{
			const bool bAlphaNum = (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9');
			if (!bAlphaNum && Ch != '_' && Ch != '-')
			{
				Ch = '_';
			}
		}

		return Name;
	}
}

ULuaScriptComponent::ULuaScriptComponent()
{
	bCanEverTick = true;
	ScriptPath = "Asset/Scripts/template.lua";
}

ULuaScriptComponent::~ULuaScriptComponent()
{
	UnbindCollisionEvents();
	FLuaScriptSystem::Get().UnloadScript(this);
}

void ULuaScriptComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	BindCollisionEvents();

	if (ScriptPath.empty())
	{
		ScriptPath = MakeDefaultScriptPath();
	}

	if (bAutoCreateScript)
	{
		EnsureScriptFile();
	}

	if (bAutoLoad)
	{
		ReloadScript();
	}

	if (bLoaded)
	{
		FLuaScriptSystem::Get().CallBeginPlay(this, GetOwner());
	}
}

void ULuaScriptComponent::EndPlay()
{
	if (bLoaded)
	{
		FLuaScriptSystem::Get().CallEndPlay(this, GetOwner());
	}

	FLuaScriptSystem::Get().UnloadScript(this);
	bLoaded = false;
}

void ULuaScriptComponent::OnRegister()
{
	bRegistered = true;
}

void ULuaScriptComponent::OnUnregister()
{
	UnbindCollisionEvents();
	FLuaScriptSystem::Get().UnloadScript(this);
	bLoaded = false;
	bRegistered = false;
}

void ULuaScriptComponent::Serialize(FArchive& Ar)
{
	UActorComponent::Serialize(Ar);
	Ar << "ScriptPath" << ScriptPath;
	Ar << "AutoLoad" << bAutoLoad;
	Ar << "AutoCreateScript" << bAutoCreateScript;
}

void ULuaScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Script Path", EPropertyType::String, &ScriptPath });
	OutProps.push_back({ "Auto Load", EPropertyType::Bool, &bAutoLoad });
	OutProps.push_back({ "Auto Create Script", EPropertyType::Bool, &bAutoCreateScript });
}

void ULuaScriptComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);
	if (PropertyName && strcmp(PropertyName, "Script Path") == 0)
	{
		bLoaded = false;
	}
}

bool ULuaScriptComponent::ReloadScript()
{
	if (ScriptPath.empty())
	{
		ScriptPath = MakeDefaultScriptPath();
	}

	bLoaded = FLuaScriptSystem::Get().ReloadScript(this, FPaths::ToAbsoluteString(FPaths::ToWide(ScriptPath)));
	return bLoaded;
}

bool ULuaScriptComponent::EnsureScriptFile()
{
	if (ScriptPath.empty())
	{
		ScriptPath = MakeDefaultScriptPath();
	}

	std::filesystem::path Target(FPaths::ToAbsolute(FPaths::ToWide(ScriptPath)));
	if (std::filesystem::exists(Target))
	{
		return true;
	}

	std::filesystem::create_directories(Target.parent_path());

	std::filesystem::path Template(FPaths::ToAbsolute(L"Asset/Scripts/template.lua"));
	if (std::filesystem::exists(Template))
	{
		std::filesystem::copy_file(Template, Target, std::filesystem::copy_options::overwrite_existing);
		return true;
	}

	std::ofstream Out(Target);
	if (!Out.is_open())
	{
		return false;
	}

	Out << "function BeginPlay(owner)\nend\n\n";
	Out << "function Tick(owner, deltaTime)\nend\n\n";
	Out << "function EndPlay(owner)\nend\n\n";
	Out << "function OnOverlap(owner, otherActor)\nend\n\n";
	Out << "function OnEndOverlap(owner, otherActor)\nend\n\n";
	Out << "function OnHit(owner, hit)\nend\n";
	return true;
}

void ULuaScriptComponent::HandleBeginOverlap(const FOverlapResult& Overlap)
{
	if (bLoaded)
	{
		FLuaScriptSystem::Get().CallOverlap(this, GetOwner(), Overlap);
	}
}

void ULuaScriptComponent::HandleEndOverlap(const FOverlapResult& Overlap)
{
	if (bLoaded)
	{
		FLuaScriptSystem::Get().CallEndOverlap(this, GetOwner(), Overlap);
	}
}

void ULuaScriptComponent::HandleHit(const FHitResult& Hit)
{
	if (bLoaded)
	{
		FLuaScriptSystem::Get().CallHit(this, GetOwner(), Hit);
	}
}

void ULuaScriptComponent::TickComponent(float DeltaTime)
{
	if (bLoaded)
	{
		FLuaScriptSystem::Get().CallTick(this, GetOwner(), DeltaTime);
	}
}

void ULuaScriptComponent::BindCollisionEvents()
{
	if (bCollisionEventsBound)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	for (UPrimitiveComponent* Primitive : OwnerActor->GetPrimitiveComponents())
	{
		if (!Primitive)
		{
			continue;
		}

		Primitive->OnComponentBeginOverlap.AddDynamic(this, &ULuaScriptComponent::HandleBeginOverlap);
		Primitive->OnComponentEndOverlap.AddDynamic(this, &ULuaScriptComponent::HandleEndOverlap);
		Primitive->OnComponentHit.AddDynamic(this, &ULuaScriptComponent::HandleHit);
	}

	bCollisionEventsBound = true;
}

void ULuaScriptComponent::UnbindCollisionEvents()
{
	if (!bCollisionEventsBound)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		bCollisionEventsBound = false;
		return;
	}

	for (UPrimitiveComponent* Primitive : OwnerActor->GetPrimitiveComponents())
	{
		if (!Primitive)
		{
			continue;
		}

		Primitive->OnComponentBeginOverlap.RemoveDynamic(this);
		Primitive->OnComponentEndOverlap.RemoveDynamic(this);
		Primitive->OnComponentHit.RemoveDynamic(this);
	}

	bCollisionEventsBound = false;
}

FString ULuaScriptComponent::MakeDefaultScriptPath() const
{
	const AActor* OwnerActor = GetOwner();
	const FString ActorName = OwnerActor ? OwnerActor->GetFName().ToString() : "Actor";
	return "Asset/Scripts/" + SanitizeScriptName(ActorName) + ".lua";
}
