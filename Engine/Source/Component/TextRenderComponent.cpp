#include "TextRenderComponent.h"
#include "Object/Class.h"
#include <algorithm>

#include "Serializer/Archive.h"


IMPLEMENT_RTTI(UTextRenderComponent, UPrimitiveComponent)

namespace
{
	struct FTextRenderComponentAliases
	{
		FTextRenderComponentAliases()
		{
			UClass::RegisterAlias("UTextComponent", UTextRenderComponent::StaticClass());
		}
	} GTextRenderComponentAliases;
}

void UTextRenderComponent::PostConstruct()
{
	bDrawDebugBounds = false;
	TextMesh = std::make_shared<FDynamicMesh>();
	TextMesh->Topology = EMeshTopology::EMT_TriangleList;

	bTextMeshDirty = true;
	if (TextMesh) TextMesh->bIsDirty = true;
}

void UTextRenderComponent::SetText(const FString& InText)
{
	if (Text != InText)
	{
		Text = InText;
		MarkTextMeshDirty();
	}
}

FRenderMesh* UTextRenderComponent::GetRenderMesh() const
{
	return TextMesh.get();
}

void UTextRenderComponent::DuplicateSubObjects()
{
	UPrimitiveComponent::DuplicateSubObjects();

	// copy constructor가 shared_ptr을 얕게 복사해 원본과 같은 FDynamicMesh를 공유한다.
	// 새 인스턴스를 만들어 소유권을 분리하고 dirty 플래그를 세워 다음 프레임에 재빌드되게 한다.
	TextMesh = std::make_shared<FDynamicMesh>();
	TextMesh->Topology = EMeshTopology::EMT_TriangleList;
	bTextMeshDirty = true;
	TextMesh->bIsDirty = true;
}

void UTextRenderComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	if (Ar.IsSaving())
	{
		Ar.Serialize("Text", Text);
		Ar.Serialize("TextColor", TextColor);
		Ar.Serialize("AlwaysFaceCamera", bAlwaysFaceCamera);
		Ar.Serialize("WorldSize", WorldSize);
	}
	else
	{
		Ar.Serialize("Text", Text);
		Ar.Serialize("TextColor", TextColor);

		// 구 버전 호환용
		if (Ar.Contains("AlwaysFaceCamera"))
		{
			Ar.Serialize("AlwaysFaceCamera", bAlwaysFaceCamera);
		}
		else if (Ar.Contains("Billboard"))
		{
			Ar.Serialize("Billboard", bAlwaysFaceCamera);
		}

		if (Ar.Contains("WorldSize"))
		{
			Ar.Serialize("WorldSize", WorldSize);
		}
		else if (Ar.Contains("TextScale"))
		{
			Ar.Serialize("TextScale", WorldSize);
		}

		SetText(Text);
		SetTextColor(TextColor);
		SetAlwaysFaceCamera(bAlwaysFaceCamera);
		SetWorldSize(WorldSize);
	}
}

FBoxSphereBounds UTextRenderComponent::GetWorldBounds() const
{
	const FVector Center = GetRenderWorldPosition();
	const FString DisplayText = GetDisplayText();
	const size_t TextLength = std::max<size_t>(DisplayText.size(), 1);

	const FVector RenderScale = GetRenderWorldScale();
	const float BaseScale = std::max(
		std::max(RenderScale.X, RenderScale.Y),
		std::max(RenderScale.Z, 0.3f)
	);

	const float HalfWidth = static_cast<float>(TextLength) * BaseScale * 0.35f;
	const float HalfHeight = BaseScale * 0.5f;
	const float HalfDepth = BaseScale * 0.15f;

	const FVector BoxExtent(HalfDepth, HalfWidth, HalfHeight);
	return { Center, BoxExtent.Size(), BoxExtent };
}
