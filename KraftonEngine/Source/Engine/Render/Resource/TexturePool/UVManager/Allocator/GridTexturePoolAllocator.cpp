#include "Render/Resource/TexturePool/UVManager/Allocator/GridTexturePoolAllocator.h"
#include <algorithm>
#include <cmath>
#include <functional>

void FGridTexturePoolAllocator::Initialize(uint32 InAtlasSize, uint32 InLayerCount, uint32 InMinBlockSize)
{
	FTexturePoolAllocatorBase::Initialize(InAtlasSize, InLayerCount, InMinBlockSize);
	GridCount = AtlasSize / MinBlockSize;
	ResetSliceOccupancy(InLayerCount);
	Entries.clear();
	NextHandle = 1;
}

bool FGridTexturePoolAllocator::AllocateHandle(float TextureSize, FTexturePoolHandle& OutHandle)
{
	const uint32 RequestSize = static_cast<uint32>(std::ceil(TextureSize));
	const uint32 BlockCount = CeilDiv(RequestSize, MinBlockSize);

	if (BlockCount == 0 || BlockCount > GridCount)
	{
		return false;
	}

	for (uint32 SliceIndex = 0; SliceIndex < GetLayerCount(); ++SliceIndex)
	{
		uint32 OutX = 0;
		uint32 OutY = 0;

		if (!FindFreeRect(SliceIndex, BlockCount, BlockCount, OutX, OutY))
		{
			continue;
		}

		MarkRect(SliceIndex, OutX, OutY, BlockCount, BlockCount, true);

		const uint32 HandleId = NextHandle++;
		FEntry Entry;
		Entry.X = OutX;
		Entry.Y = OutY;
		Entry.W = BlockCount;
		Entry.H = BlockCount;
		Entry.ArrayIndex = SliceIndex;

		Entries.emplace(HandleId, Entry);

		OutHandle.InternalIndex = HandleId;
		OutHandle.ArrayIndex = SliceIndex;
		return true;
	}

	return false;
}

bool FGridTexturePoolAllocator::CanAllocateHandle(float TextureSize) const
{
	const uint32 RequestSize = static_cast<uint32>(std::ceil(TextureSize));
	const uint32 BlockCount = CeilDiv(RequestSize, MinBlockSize);

	if (BlockCount == 0 || BlockCount > GridCount)
	{
		return false;
	}

	for (uint32 SliceIndex = 0; SliceIndex < GetLayerCount(); ++SliceIndex)
	{
		uint32 OutX = 0;
		uint32 OutY = 0;
		if (FindFreeRect(SliceIndex, BlockCount, BlockCount, OutX, OutY))
		{
			return true;
		}
	}

	return false;
}

bool FGridTexturePoolAllocator::CanAllocateHandleSet(const FTexturePoolHandleRequest& Request) const
{
	TArray<uint32> SortedSizes = Request.Sizes;
	std::sort(SortedSizes.begin(), SortedSizes.end(), std::greater<uint32>());

	std::vector<std::vector<bool>> TestOccupancy = OccupiedBySlice;
	for (uint32 Size : SortedSizes)
	{
		const uint32 BlockCount = CeilDiv(Size, MinBlockSize);
		if (BlockCount == 0 || BlockCount > GridCount)
		{
			return false;
		}

		bool bPlaced = false;
		for (uint32 SliceIndex = 0; SliceIndex < GetLayerCount(); ++SliceIndex)
		{
			uint32 OutX = 0;
			uint32 OutY = 0;
			if (!FindFreeRectInOccupancy(TestOccupancy, SliceIndex, BlockCount, BlockCount, OutX, OutY))
			{
				continue;
			}

			MarkRectInOccupancy(TestOccupancy, SliceIndex, OutX, OutY, BlockCount, BlockCount, true);
			bPlaced = true;
			break;
		}

		if (!bPlaced)
		{
			return false;
		}
	}

	return true;
}

FAtlasUV FGridTexturePoolAllocator::GetAtlasUV(const FTexturePoolHandle& InHandle)
{
	auto It = Entries.find(InHandle.InternalIndex);
	if (It == Entries.end())
	{
		return {};
	}

	const FEntry& Entry = It->second;
	const float PixelX1 = static_cast<float>(Entry.X * MinBlockSize);
	const float PixelY1 = static_cast<float>(Entry.Y * MinBlockSize);
	const float PixelX2 = static_cast<float>((Entry.X + Entry.W) * MinBlockSize);
	const float PixelY2 = static_cast<float>((Entry.Y + Entry.H) * MinBlockSize);

	FAtlasUV UV;
	UV.ArrayIndex = Entry.ArrayIndex;
	UV.u1 = PixelX1 / static_cast<float>(AtlasSize);
	UV.v1 = PixelY1 / static_cast<float>(AtlasSize);
	UV.u2 = PixelX2 / static_cast<float>(AtlasSize);
	UV.v2 = PixelY2 / static_cast<float>(AtlasSize);
	return UV;
}

void FGridTexturePoolAllocator::ReleaseHandle(const FTexturePoolHandle& InHandle)
{
	auto It = Entries.find(InHandle.InternalIndex);
	if (It == Entries.end())
	{
		return;
	}

	const FEntry& Entry = It->second;
	MarkRect(Entry.ArrayIndex, Entry.X, Entry.Y, Entry.W, Entry.H, false);
	Entries.erase(It);
}

void FGridTexturePoolAllocator::BroadcastEntries()
{
	// UVs are computed on demand from the current atlas size.
}

void FGridTexturePoolAllocator::SetSize(uint32 InNewTextureSize)
{
	FTexturePoolAllocatorBase::SetSize(InNewTextureSize);
	AtlasSize = InNewTextureSize;
	GridCount = AtlasSize / MinBlockSize;
	RebuildOccupancyFromEntries();
}

void FGridTexturePoolAllocator::SetLayerCount(uint32 InNewLayerCount)
{
	FTexturePoolAllocatorBase::SetLayerCount(InNewLayerCount);
	RebuildOccupancyFromEntries();
}

uint32 FGridTexturePoolAllocator::CeilDiv(uint32 A, uint32 B)
{
	return (A + B - 1) / B;
}

uint32 FGridTexturePoolAllocator::Index(uint32 X, uint32 Y) const
{
	return Y * GridCount + X;
}

void FGridTexturePoolAllocator::ResetSliceOccupancy(uint32 InLayerCount)
{
	const size_t CellCount = static_cast<size_t>(GridCount) * static_cast<size_t>(GridCount);
	OccupiedBySlice.assign(InLayerCount, std::vector<bool>(CellCount, false));
}

void FGridTexturePoolAllocator::RebuildOccupancyFromEntries()
{
	ResetSliceOccupancy(GetLayerCount());
	for (const auto& Pair : Entries)
	{
		const FEntry& Entry = Pair.second;
		MarkRect(Entry.ArrayIndex, Entry.X, Entry.Y, Entry.W, Entry.H, true);
	}
}

bool FGridTexturePoolAllocator::IsFreeRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H) const
{
	return IsFreeRectInOccupancy(OccupiedBySlice, SliceIndex, X, Y, W, H);
}

bool FGridTexturePoolAllocator::FindFreeRect(uint32 SliceIndex, uint32 W, uint32 H, uint32& OutX, uint32& OutY) const
{
	for (uint32 y = 0; y + H <= GridCount; ++y)
	{
		for (uint32 x = 0; x + W <= GridCount; ++x)
		{
			if (IsFreeRect(SliceIndex, x, y, W, H))
			{
				OutX = x;
				OutY = y;
				return true;
			}
		}
	}

	return false;
}

void FGridTexturePoolAllocator::MarkRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H, bool bOccupied)
{
	MarkRectInOccupancy(OccupiedBySlice, SliceIndex, X, Y, W, H, bOccupied);
}

bool FGridTexturePoolAllocator::IsFreeRectInOccupancy(
	const std::vector<std::vector<bool>>& Occupancy,
	uint32 SliceIndex,
	uint32 X,
	uint32 Y,
	uint32 W,
	uint32 H) const
{
	if (SliceIndex >= Occupancy.size() || X + W > GridCount || Y + H > GridCount)
	{
		return false;
	}

	const std::vector<bool>& Occupied = Occupancy[SliceIndex];
	for (uint32 yy = Y; yy < Y + H; ++yy)
	{
		for (uint32 xx = X; xx < X + W; ++xx)
		{
			if (Occupied[Index(xx, yy)])
			{
				return false;
			}
		}
	}

	return true;
}

bool FGridTexturePoolAllocator::FindFreeRectInOccupancy(
	const std::vector<std::vector<bool>>& Occupancy,
	uint32 SliceIndex,
	uint32 W,
	uint32 H,
	uint32& OutX,
	uint32& OutY) const
{
	for (uint32 y = 0; y + H <= GridCount; ++y)
	{
		for (uint32 x = 0; x + W <= GridCount; ++x)
		{
			if (IsFreeRectInOccupancy(Occupancy, SliceIndex, x, y, W, H))
			{
				OutX = x;
				OutY = y;
				return true;
			}
		}
	}

	return false;
}

void FGridTexturePoolAllocator::MarkRectInOccupancy(
	std::vector<std::vector<bool>>& Occupancy,
	uint32 SliceIndex,
	uint32 X,
	uint32 Y,
	uint32 W,
	uint32 H,
	bool bOccupied) const
{
	if (SliceIndex >= Occupancy.size())
	{
		return;
	}

	std::vector<bool>& Occupied = Occupancy[SliceIndex];
	for (uint32 yy = Y; yy < Y + H; ++yy)
	{
		for (uint32 xx = X; xx < X + W; ++xx)
		{
			Occupied[Index(xx, yy)] = bOccupied;
		}
	}
}
