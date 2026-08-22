#include "Render/Proxy/WeaponTrailSceneProxy.h"

#include "Materials/Material.h"
#include "Render/Command/DrawCommand.h"

#include <cmath>

FWeaponTrailSceneProxy::FWeaponTrailSceneProxy(UWeaponTrailComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	Samples = InComponent->GetTrailSamples();
	TrailLifetime = InComponent->GetTrailLifetime();
	TrailMaterial = InComponent->GetTrailMaterial();
	HeadColor = InComponent->GetHeadColor();
	TailColor = InComponent->GetTailColor();
	AlphaMultiplier = InComponent->GetAlphaMultiplier();
	AlphaPower = InComponent->GetAlphaPower();
	HeadWidthScale = InComponent->GetHeadWidthScale();
	TailWidthScale = InComponent->GetTailWidthScale();
	WidthFadePower = InComponent->GetWidthFadePower();
	DistanceUVScale = InComponent->GetDistanceUVScale();

	ProxyFlags |= EPrimitiveProxyFlags::NeverCull;
}

FWeaponTrailSceneProxy::~FWeaponTrailSceneProxy()
{
	Geometry.Release();
}

void FWeaponTrailSceneProxy::UpdateMesh()
{
	Geometry.Clear();

	UWeaponTrailComponent* Comp = static_cast<UWeaponTrailComponent*>(GetOwner());
	Samples = Comp->GetTrailSamples();
	TrailLifetime = Comp->GetTrailLifetime();
	TrailMaterial = Comp->GetTrailMaterial();
	HeadColor = Comp->GetHeadColor();
	TailColor = Comp->GetTailColor();
	AlphaMultiplier = Comp->GetAlphaMultiplier();
	AlphaPower = Comp->GetAlphaPower();
	HeadWidthScale = Comp->GetHeadWidthScale();
	TailWidthScale = Comp->GetTailWidthScale();
	WidthFadePower = Comp->GetWidthFadePower();
	DistanceUVScale = Comp->GetDistanceUVScale();

	if (Samples.size() < 2)
	{
		SectionDraws.clear();
		return;
	}

	TArray<uint32> StartIndices;
	TArray<uint32> EndIndices;

	for (const FWeaponTrailSample& Sample : Samples)
	{
		float NormalizedAge = TrailLifetime > 0.0f ? Sample.Age / TrailLifetime : 1.0f;
		NormalizedAge = FMath::Clamp(NormalizedAge, 0.0f, 1.0f);

		float Fade = 1.0f - NormalizedAge;
		float Alpha = std::pow(Fade, std::max(AlphaPower, 0.01f)) * AlphaMultiplier;
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

		FVector4 Color = TailColor * NormalizedAge + HeadColor * (1.0f - NormalizedAge);
		Color.W *= Alpha;

		const float WidthFade = std::pow(Fade, std::max(WidthFadePower, 0.01f));
		const float WidthScale = TailWidthScale + (HeadWidthScale - TailWidthScale) * WidthFade;
		const FVector Center = (Sample.Start + Sample.End) * 0.5f;
		const FVector HalfWidth = (Sample.End - Sample.Start) * 0.5f * WidthScale;
		const FVector Start = Center - HalfWidth;
		const FVector End = Center + HalfWidth;

		const float U = Sample.Distance * DistanceUVScale;

		StartIndices.push_back(Geometry.AddVertex({ Start, Color, FVector2(U, 0.0f) }));
		EndIndices.push_back(Geometry.AddVertex({ End, Color, FVector2(U, 1.0f) }));
	}

	for (int32 Index = 0; Index + 1 < static_cast<int32>(Samples.size()); ++Index)
	{
		const uint32 S0 = StartIndices[Index];
		const uint32 E0 = EndIndices[Index];
		const uint32 S1 = StartIndices[Index + 1];
		const uint32 E1 = EndIndices[Index + 1];

		Geometry.AddTriangle(S0, E0, S1);
		Geometry.AddTriangle(E0, E1, S1);
	}

	SectionDraws.clear();

	UMaterialInterface* Material = TrailMaterial;
	if (Material)
	{
		SectionDraws.push_back({ Material, 0, Geometry.GetIndexCount() });
	}
}

bool FWeaponTrailSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	if (!Device || !Context || Geometry.GetIndexCount() == 0) return false;

	if (!bGeometryCreated)
	{
		Geometry.Create(Device);
		bGeometryCreated = true;
	}

	if (!Geometry.Upload(Device, Context)) return false;

	OutBuffer = {};
	OutBuffer.VB = Geometry.GetVertexBuffer();
	OutBuffer.VBStride = Geometry.GetVertexStride();
	OutBuffer.IB = Geometry.GetIndexBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}
