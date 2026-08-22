#include "BillboardComponent.h"

#include "Core/Engine.h"
#include "Object/Class.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Texture.h"
#include "Serializer/Archive.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_RTTI(UBillboardComponent, UPrimitiveComponent)

UBillboardComponent::UBillboardComponent(const UBillboardComponent& Other)
	: UPrimitiveComponent(Other)
	, Sprite(Other.Sprite)
	, Size(Other.Size)
	, TintColor(Other.TintColor)
	, bScreenSizeScaled(Other.bScreenSizeScaled)
	, ScreenSize(Other.ScreenSize)
	, BillboardMesh(nullptr)  // GPU 버퍼를 공유하지 않도록 nullptr로 초기화 — DuplicateSubObjects에서 재생성
	, BillboardMaterial(nullptr)
	, LoadedSprite(nullptr)
{
}

UBillboardComponent::~UBillboardComponent() = default;

void UBillboardComponent::PostConstruct()
{
	bDrawDebugBounds = false;

	BillboardMesh = std::make_shared<FDynamicMesh>();
	BillboardMesh->Topology = EMeshTopology::EMT_TriangleList;
	RebuildMesh();
}

void UBillboardComponent::OnRegister()
{
	UPrimitiveComponent::OnRegister();
	EnsureRenderResources();
	UpdateBounds();
}

void UBillboardComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	FString SpriteAssetPath = Sprite ? Sprite->GetAssetPathFileName() : "";
	Ar.Serialize("Sprite", SpriteAssetPath);
	FVector SerializedSize(Size.X, Size.Y, 0.0f);
	Ar.Serialize("Size", SerializedSize);
	Ar.Serialize("TintColor", TintColor);
	Ar.Serialize("ScreenSizeScaled", bScreenSizeScaled);
	Ar.Serialize("ScreenSize", ScreenSize);

	if (Ar.IsLoading())
	{
		if (SpriteAssetPath.empty() && Ar.Contains("SpritePath"))
		{
			Ar.Serialize("SpritePath", SpriteAssetPath);
		}

		Sprite = UTexture::FindOrLoad(SpriteAssetPath, this);
		Size = FVector2((std::max)(SerializedSize.X, 0.001f), (std::max)(SerializedSize.Y, 0.001f));
		LoadedSprite = nullptr;
		RebuildMesh();
		EnsureRenderResources();
		UpdateBounds();
	}
}

FRenderMesh* UBillboardComponent::GetRenderMesh() const
{
	return BillboardMesh.get();
}

FBoxSphereBounds UBillboardComponent::GetWorldBounds() const
{
	const FVector Center = GetWorldLocation();
	const FVector WorldScale = GetWorldTransform().GetScaleVector();

	const float HalfWidth = Size.X * 0.5f * std::max(std::abs(WorldScale.Y), 1.e-3f);
	const float HalfHeight = Size.Y * 0.5f * std::max(std::abs(WorldScale.Z), 1.e-3f);
	const float HalfDepth = (std::max)(HalfWidth, HalfHeight);

	const FVector BoxExtent(HalfDepth, HalfWidth, HalfHeight);
	return { Center, BoxExtent.Size(), BoxExtent };
}

void UBillboardComponent::SetSprite(UTexture* InSprite)
{
	if (Sprite.Get() == InSprite)
	{
		return;
	}

	Sprite = InSprite;
	LoadedSprite = nullptr;
	EnsureRenderResources();
}

void UBillboardComponent::SetSize(const FVector2& InSize)
{
	Size.X = (std::max)(InSize.X, 0.001f);
	Size.Y = (std::max)(InSize.Y, 0.001f);
	RebuildMesh();
	UpdateBounds();
}

void UBillboardComponent::SetTintColor(const FVector4& InTintColor)
{
	TintColor = InTintColor;
	EnsureMaterial();
	if (BillboardMaterial)
	{
		BillboardMaterial->SetParameterData("BaseColor", &TintColor, sizeof(TintColor));
	}
}

FVector UBillboardComponent::GetBillboardRenderScale(const FVector& CameraPosition) const
{
	if (bScreenSizeScaled)
	{
		const float Distance = (CameraPosition - GetWorldLocation()).Size();
		const float ScaleFactor = (std::max)(Distance * ScreenSize, 0.001f);
		return FVector(1.0f, ScaleFactor, ScaleFactor);
	}

	const FVector WorldScale = GetWorldTransform().GetScaleVector();
	return FVector(
		(std::max)(std::abs(WorldScale.X), 1.0f),
		(std::max)(std::abs(WorldScale.Y), 0.001f),
		(std::max)(std::abs(WorldScale.Z), 0.001f)
	);
}

FMaterial* UBillboardComponent::GetBillboardMaterial() const
{
	return BillboardMaterial.get();
}

bool UBillboardComponent::EnsureRenderResources()
{
	RebuildMesh();
	return EnsureMaterial() && LoadSpriteTexture();
}

void UBillboardComponent::DuplicateSubObjects()
{
	UPrimitiveComponent::DuplicateSubObjects();

	BillboardMesh = std::make_shared<FDynamicMesh>();
	BillboardMesh->Topology = EMeshTopology::EMT_TriangleList;
	RebuildMesh();

	BillboardMaterial = nullptr;
	LoadedSprite = nullptr;
}

void UBillboardComponent::RebuildMesh()
{
	if (!BillboardMesh)
	{
		return;
	}

	BillboardMesh->Vertices.clear();
	BillboardMesh->Indices.clear();
	BillboardMesh->Topology = EMeshTopology::EMT_TriangleList;

	const float HalfWidth = Size.X * 0.5f;
	const float HalfHeight = Size.Y * 0.5f;

	FVertex V0, V1, V2, V3;
	V0.Position = FVector(0.0f, -HalfWidth, HalfHeight);
	V1.Position = FVector(0.0f, HalfWidth, HalfHeight);
	V2.Position = FVector(0.0f, HalfWidth, -HalfHeight);
	V3.Position = FVector(0.0f, -HalfWidth, -HalfHeight);

	V0.UV = FVector2(0.0f, 0.0f);
	V1.UV = FVector2(1.0f, 0.0f);
	V2.UV = FVector2(1.0f, 1.0f);
	V3.UV = FVector2(0.0f, 1.0f);

	V0.Color = V1.Color = V2.Color = V3.Color = TintColor;
	V0.Normal = V1.Normal = V2.Normal = V3.Normal = FVector(1.0f, 0.0f, 0.0f);

	BillboardMesh->Vertices = { V0, V1, V2, V3 };
	BillboardMesh->Indices = { 0, 1, 2, 0, 2, 3 };
	BillboardMesh->bIsDirty = true;
	BillboardMesh->UpdateLocalBound();
}

bool UBillboardComponent::EnsureMaterial()
{
	if (BillboardMaterial)
	{
		return true;
	}

	FRenderer* Renderer = GEngine ? GEngine->GetRenderer() : nullptr;
	if (!Renderer)
	{
		return false;
	}

	FMaterial* DefaultTextureMaterial = Renderer->GetDefaultTextureMaterial();
	if (!DefaultTextureMaterial)
	{
		return false;
	}

	BillboardMaterial = DefaultTextureMaterial->CreateDynamicMaterial();
	if (!BillboardMaterial)
	{
		return false;
	}

	BillboardMaterial->SetOriginName("M_Billboard");
	BillboardMaterial->SetInstanceName(GetName() + "_Billboard");
	BillboardMaterial->SetParameterData("BaseColor", &TintColor, sizeof(TintColor));

	FRasterizerStateOption RasterizerOption = BillboardMaterial->GetRasterizerOption();
	RasterizerOption.CullMode = D3D11_CULL_NONE;
	BillboardMaterial->SetRasterizerOption(RasterizerOption);
	BillboardMaterial->SetRasterizerState(Renderer->GetRenderStateManager()->GetOrCreateRasterizerState(RasterizerOption));

	FDepthStencilStateOption DepthOption = BillboardMaterial->GetDepthStencilOption();
	DepthOption.DepthEnable = true;
	DepthOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	BillboardMaterial->SetDepthStencilOption(DepthOption);
	BillboardMaterial->SetDepthStencilState(Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(DepthOption));

	FBlendStateOption BlendOption = BillboardMaterial->GetBlendOption();
	BlendOption.BlendEnable = true;
	BlendOption.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendOption.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendOption.BlendOp = D3D11_BLEND_OP_ADD;
	BlendOption.SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendOption.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	BlendOption.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BillboardMaterial->SetBlendOption(BlendOption);
	BillboardMaterial->SetBlendState(Renderer->GetRenderStateManager()->GetOrCreateBlendState(BlendOption));

	return true;
}

bool UBillboardComponent::LoadSpriteTexture()
{
	if (!EnsureMaterial())
	{
		return false;
	}

	UTexture* SpriteTexture = Sprite.Get();
	if (!SpriteTexture)
	{
		return false;
	}

	if (LoadedSprite.Get() == SpriteTexture && BillboardMaterial->GetMaterialTexture())
	{
		return true;
	}

	if (!SpriteTexture->EnsureTextureResource())
	{
		return false;
	}

	BillboardMaterial->SetMaterialTexture(SpriteTexture->GetTextureResource());
	LoadedSprite = SpriteTexture;
	return true;
}
