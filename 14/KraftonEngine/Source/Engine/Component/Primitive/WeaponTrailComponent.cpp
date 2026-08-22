#include "Component/Primitive/WeaponTrailComponent.h"

#include "Render/Scene/FScene.h"
#include "Debug/DebugDrawQueue.h"
#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Render/Proxy/WeaponTrailSceneProxy.h"

#include <algorithm>
#include <cstring>

void UWeaponTrailComponent::SetTrailEnabled(bool bEnabled)
{
	bTrailEnabled = bEnabled;
}

void UWeaponTrailComponent::ClearTrail()
{
	Samples.clear();
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UWeaponTrailComponent::SetTrailMaterial(UMaterialInterface* InMaterial)
{
	TrailMaterial = InMaterial;
	TrailMaterialPath = TrailMaterial ? TrailMaterial->GetAssetPathFileName() : "None";

	MarkProxyDirty(EDirtyFlag::Material);
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UWeaponTrailComponent::LoadTrailMaterialFromPath()
{
	if (TrailMaterialPath.IsNull() || TrailMaterialPath == "None")
	{
		TrailMaterial = nullptr;
		return;
	}

	TrailMaterial = FMaterialManager::Get().GetOrCreateMaterialInterface(TrailMaterialPath.ToString());
}

void UWeaponTrailComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!bDrawDebugPoints)
	{
		return;
	}

	FVector Start;
	FVector End;
	if (!GetTrailPoints(Start, End))
	{
		return;
	}

	FDebugDrawQueue& Queue = Scene.GetDebugDrawQueue();

	const float Radius = DebugSphereRadius;
	const int32 Segments = 12;

	Queue.AddSphere(Start, Radius, Segments, FColor::Green(), 0.0f);
	Queue.AddSphere(End, Radius, Segments, FColor::Red(), 0.0f);

	Queue.AddLine(Start, End, FColor::Yellow(), 0.0f);
}

void UWeaponTrailComponent::PostLoad()
{
	UPrimitiveComponent::PostLoad();
	LoadTrailMaterialFromPath();
}

void UWeaponTrailComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	LoadTrailMaterialFromPath();
}

void UWeaponTrailComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "TrailMaterialPath") == 0 || strcmp(PropertyName, "Material") == 0)
	{
		LoadTrailMaterialFromPath();
		MarkProxyDirty(EDirtyFlag::Material);
		MarkProxyDirty(EDirtyFlag::Mesh);
	}

	if (strcmp(PropertyName, "TrailLifetime") == 0 ||
		strcmp(PropertyName, "HeadColor") == 0 ||
		strcmp(PropertyName, "TailColor") == 0 ||
		strcmp(PropertyName, "AlphaMultiplier") == 0 ||
		strcmp(PropertyName, "AlphaPower") == 0 ||
		strcmp(PropertyName, "HeadWidthScale") == 0 ||
		strcmp(PropertyName, "TailWidthScale") == 0 ||
		strcmp(PropertyName, "WidthFadePower") == 0 ||
		strcmp(PropertyName, "DistanceUVScale") == 0 ||
		strcmp(PropertyName, "StartLocalOffset") == 0 ||
		strcmp(PropertyName, "EndLocalOffset") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

void UWeaponTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSamples(DeltaTime);

	if (bTrailEnabled)
	{
		AddSampleIfNeeded(DeltaTime);
	}

	if (!Samples.empty())
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

FPrimitiveSceneProxy* UWeaponTrailComponent::CreateSceneProxy()
{
	return new FWeaponTrailSceneProxy(this);
}

void UWeaponTrailComponent::AddSampleIfNeeded(float DeltaTime)
{
	FVector NewStart;
	FVector NewEnd;
	if (!GetTrailPoints(NewStart, NewEnd)) return;

	if (!Samples.empty())
	{
		const FWeaponTrailSample& Last = Samples.back();

		const float MoveDistance = FVector::Distance(NewStart, Last.Start)
			+ FVector::Distance(NewEnd, Last.End);

		if (MoveDistance < MinSampleDistance) return;
	}

	FWeaponTrailSample NewSample;
	NewSample.Start = NewStart;
	NewSample.End = NewEnd;
	NewSample.Age = 0.0f;

	if (!Samples.empty())
	{
		const FWeaponTrailSample& Last = Samples.back();
		const FVector LastCenter = (Last.Start + Last.End) * 0.5f;
		const FVector NewCenter = (NewStart + NewEnd) * 0.5f;
		NewSample.Distance = Last.Distance + FVector::Distance(NewCenter, LastCenter);
	}

	Samples.push_back(NewSample);

	while (static_cast<int32>(Samples.size()) > MaxSamples)
	{
		Samples.erase(Samples.begin());
	}
}

void UWeaponTrailComponent::UpdateSamples(float DeltaTime)
{
	for (FWeaponTrailSample& Sample : Samples)
	{
		Sample.Age += DeltaTime;
	}

	Samples.erase(std::remove_if(Samples.begin(), Samples.end(),
		[this](const FWeaponTrailSample& Sample)
		{
			return Sample.Age >= TrailLifetime;
	}), Samples.end());
}

bool UWeaponTrailComponent::GetTrailPoints(FVector& OutStart, FVector& OutEnd) const
{
	const FMatrix& WorldMatrix = GetWorldMatrix();

	OutStart = WorldMatrix.TransformPositionWithW(StartLocalOffset);
	OutEnd = WorldMatrix.TransformPositionWithW(EndLocalOffset);

	return true;
}
