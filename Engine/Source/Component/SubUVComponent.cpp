#include "SubUVComponent.h"
#include "Object/Class.h"
#include "Renderer/MeshData.h"
#include "Serializer/Archive.h"
#include <algorithm>

IMPLEMENT_RTTI(USubUVComponent, UPrimitiveComponent)

void USubUVComponent::PostConstruct()
{
	// SubUV 렌더링용 메시 객체 생성
	bDrawDebugBounds = false;
	SubUVMesh = std::make_shared<FDynamicMesh>();
}

void USubUVComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);

	FVector SerializedSize(Size.X, Size.Y, 0.0f);
	uint32 SerializedColumns = static_cast<uint32>(Columns);
	uint32 SerializedRows = static_cast<uint32>(Rows);
	uint32 SerializedTotalFrames = static_cast<uint32>(TotalFrames);
	uint32 SerializedFirstFrame = static_cast<uint32>(FirstFrame);
	uint32 SerializedLastFrame = static_cast<uint32>(LastFrame);
	float SerializedFPS = FPS;
	bool bSerializedLoop = bLoop;
	bool bSerializedBillboard = bBillboard;

	if (Ar.IsSaving())
	{
		Ar.Serialize("Size", SerializedSize);
		Ar.Serialize("Columns", SerializedColumns);
		Ar.Serialize("Rows", SerializedRows);
		Ar.Serialize("TotalFrames", SerializedTotalFrames);
		Ar.Serialize("FirstFrame", SerializedFirstFrame);
		Ar.Serialize("LastFrame", SerializedLastFrame);
		Ar.Serialize("FPS", SerializedFPS);
		Ar.Serialize("Loop", bSerializedLoop);
		Ar.Serialize("Billboard", bSerializedBillboard);
	}
	else
	{
		if (Ar.Contains("Size"))
		{
			Ar.Serialize("Size", SerializedSize);
		}
		if (Ar.Contains("Columns"))
		{
			Ar.Serialize("Columns", SerializedColumns);
		}
		if (Ar.Contains("Rows"))
		{
			Ar.Serialize("Rows", SerializedRows);
		}
		if (Ar.Contains("TotalFrames"))
		{
			Ar.Serialize("TotalFrames", SerializedTotalFrames);
		}
		if (Ar.Contains("FirstFrame"))
		{
			Ar.Serialize("FirstFrame", SerializedFirstFrame);
		}
		if (Ar.Contains("LastFrame"))
		{
			Ar.Serialize("LastFrame", SerializedLastFrame);
		}
		if (Ar.Contains("FPS"))
		{
			Ar.Serialize("FPS", SerializedFPS);
		}
		if (Ar.Contains("Loop"))
		{
			Ar.Serialize("Loop", bSerializedLoop);
		}
		if (Ar.Contains("Billboard"))
		{
			Ar.Serialize("Billboard", bSerializedBillboard);
		}

		Size = FVector2((std::max)(SerializedSize.X, 0.001f), (std::max)(SerializedSize.Y, 0.001f));
		Columns = static_cast<int32>((std::max)(SerializedColumns, 1u));
		Rows = static_cast<int32>((std::max)(SerializedRows, 1u));
		TotalFrames = static_cast<int32>((std::max)(SerializedTotalFrames, 1u));
		FirstFrame = static_cast<int32>((std::min)(SerializedFirstFrame, static_cast<uint32>(TotalFrames - 1)));
		LastFrame = static_cast<int32>((std::min)((std::max)(SerializedLastFrame, SerializedFirstFrame), static_cast<uint32>(TotalFrames - 1)));
		FPS = (std::max)(SerializedFPS, 0.001f);
		bLoop = bSerializedLoop;
		bBillboard = bSerializedBillboard;
		UpdateBounds();
	}
}

FRenderMesh* USubUVComponent::GetRenderMesh() const { return SubUVMesh.get(); }

FBoxSphereBounds USubUVComponent::GetWorldBounds() const
{
	const FVector Center = GetWorldLocation();
	const FVector WorldScale = GetWorldTransform().GetScaleVector();

	const float HalfW = Size.X * 0.5f * WorldScale.X;
	const float HalfH = Size.Y * 0.5f * WorldScale.Y;
	const float HalfZ = ((HalfW > HalfH) ? HalfW : HalfH);

	const FVector BoxExtent(HalfW, HalfH, HalfZ);
	return { Center, BoxExtent.Size(), BoxExtent };
}
