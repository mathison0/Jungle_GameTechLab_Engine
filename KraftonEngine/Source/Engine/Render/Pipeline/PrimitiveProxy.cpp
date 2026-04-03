#include "PrimitiveProxy.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Resource/ShaderManager.h"

FPrimitiveProxy::FPrimitiveProxy(UPrimitiveComponent* InOwner)
	: Owner(InOwner)
	, bIsDirty(true)
{
}

void FPrimitiveProxy::CollectRender(FRenderBus& Bus, bool bSelected)
{
	if (IsDirty())
	{
		UpdateProxy();
		bIsDirty = false;
	}

	Bus.AddCommand(ERenderPass::Opaque, CachedCommand);

	if (bSelected)
	{
		if (Owner->SupportsOutline())
		{
			// Selection Mask
			Bus.AddCommand(ERenderPass::SelectionMask, CachedCommand);
		}

		if (Bus.GetShowFlags().bBoundingVolume)
		{
			FAABBEntry Entry = {};
			FBoundingBox Box = Owner->GetWorldBoundingBox();
			Entry.AABB.Min = Box.Min;
			Entry.AABB.Max = Box.Max;
			Entry.AABB.Color = FColor::White();
			Bus.AddAABBEntry(std::move(Entry));
		}
	}
}

FDefaultPrimitiveProxy::FDefaultPrimitiveProxy(UPrimitiveComponent* InOwner)
	: FPrimitiveProxy(InOwner)
{
}

void FDefaultPrimitiveProxy::UpdateProxy()
{
	FMeshBuffer* Buffer = Owner->GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid())
	{
		CachedCommand = {};
		return;
	}

	CachedCommand = {};
	CachedCommand.PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Owner->GetWorldMatrix());
	CachedCommand.Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	CachedCommand.MeshBuffer = Buffer;
}
