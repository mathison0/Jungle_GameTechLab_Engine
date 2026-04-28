#pragma once

#include "../TexturePoolTypes.h"
#include "Core/CoreTypes.h"
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>

struct FAtlasUV
{
	uint32 ArrayIndex = 0;

	float u1 = 0.0f;
	float v1 = 0.0f;
	float u2 = 0.0f;
	float v2 = 0.0f;
};

class FTexturePoolAllocatorBase
{
public:
	virtual ~FTexturePoolAllocatorBase() = default;

	virtual void Initialize(uint32 InSize, uint32 InLayerCount)
	{
		Size = InSize;
		LayerCount = InLayerCount;
	}

	virtual bool AllocateHandle(float TextureSize, FTexturePoolHandle& OutHandle) = 0;
	virtual FAtlasUV GetAtlasUV(const FTexturePoolHandle& InHandle) = 0;
	virtual void ReleaseHandle(const FTexturePoolHandle& InHandle) = 0;
	virtual void BroadcastEntries() = 0;

	virtual void SetSize(uint32 InNewTextureSize)
	{
		Size = InNewTextureSize;
		BroadcastEntries();
	}

	virtual void SetLayerCount(uint32 InNewLayerCount)
	{
		LayerCount = InNewLayerCount;
		BroadcastEntries();
	}

	uint32 ReserveHandleSetId()
	{
		return NextHandleSetId++;
	}

	FTexturePoolHandleSet* RegisterHandleSet(std::unique_ptr<FTexturePoolHandleSet> InHandleSet)
	{
		if (!InHandleSet)
		{
			return nullptr;
		}

		FTexturePoolHandleSet* HandleSet = InHandleSet.get();
		RegisteredHandleSets[HandleSet->InternalIndex] = std::move(InHandleSet);
		return HandleSet;
	}

	void UnregisterHandleSet(uint32 InHandleSetId)
	{
		RegisteredHandleSets.erase(InHandleSetId);
	}

	void InvalidateAllHandleSets()
	{
		for (auto& Pair : RegisteredHandleSets)
		{
			if (FTexturePoolHandleSet* HandleSet = Pair.second.get())
			{
				HandleSet->bIsValid = false;
				++HandleSet->DebugVersion;
			}
		}
	}

protected:
	uint32 GetSize() const { return Size; }
	uint32 GetLayerCount() const { return LayerCount; }

private:
	uint32 Size = 0;
	uint32 LayerCount = 0;
	uint32 NextHandleSetId = 0;
	TMap<uint32, std::unique_ptr<FTexturePoolHandleSet>> RegisteredHandleSets;
};

class FGridTexturePoolAllocator : public FTexturePoolAllocatorBase
{
public:
	explicit FGridTexturePoolAllocator(uint32 InMinBlockSize = 1024)
		: MinBlockSize(InMinBlockSize)
	{
	}

	virtual void Initialize(uint32 InAtlasSize, uint32 InLayerCount) override
	{
		FTexturePoolAllocatorBase::Initialize(InAtlasSize, InLayerCount);

		AtlasSize = InAtlasSize;
		GridCount = AtlasSize / MinBlockSize;
		ResetSliceOccupancy(InLayerCount);
		Entries.clear();
		NextHandle = 1;
	}

	virtual bool AllocateHandle(float TextureSize, FTexturePoolHandle& OutHandle) override
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

	virtual FAtlasUV GetAtlasUV(const FTexturePoolHandle& InHandle) override
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

	virtual void ReleaseHandle(const FTexturePoolHandle& InHandle) override
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

	virtual void BroadcastEntries() override
	{
		// UV는 GetAtlasUV 호출 시 현재 atlas 크기 기준으로 계산한다.
	}

	virtual void SetSize(uint32 InNewTextureSize) override
	{
		FTexturePoolAllocatorBase::SetSize(InNewTextureSize);
		AtlasSize = InNewTextureSize;
		GridCount = AtlasSize / MinBlockSize;
		RebuildOccupancyFromEntries();
	}

	virtual void SetLayerCount(uint32 InNewLayerCount) override
	{
		FTexturePoolAllocatorBase::SetLayerCount(InNewLayerCount);
		RebuildOccupancyFromEntries();
	}

private:
	struct FEntry
	{
		uint32 X = 0;
		uint32 Y = 0;
		uint32 W = 0;
		uint32 H = 0;
		uint32 ArrayIndex = 0;
	};

private:
	static uint32 CeilDiv(uint32 A, uint32 B)
	{
		return (A + B - 1) / B;
	}

	uint32 Index(uint32 X, uint32 Y) const
	{
		return Y * GridCount + X;
	}

	void ResetSliceOccupancy(uint32 InLayerCount)
	{
		const size_t CellCount = static_cast<size_t>(GridCount) * static_cast<size_t>(GridCount);
		OccupiedBySlice.assign(InLayerCount, std::vector<bool>(CellCount, false));
	}

	void RebuildOccupancyFromEntries()
	{
		ResetSliceOccupancy(GetLayerCount());
		for (const auto& Pair : Entries)
		{
			const FEntry& Entry = Pair.second;
			MarkRect(Entry.ArrayIndex, Entry.X, Entry.Y, Entry.W, Entry.H, true);
		}
	}

	bool IsFreeRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H) const
	{
		if (SliceIndex >= OccupiedBySlice.size() || X + W > GridCount || Y + H > GridCount)
		{
			return false;
		}

		const std::vector<bool>& Occupied = OccupiedBySlice[SliceIndex];
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

	bool FindFreeRect(uint32 SliceIndex, uint32 W, uint32 H, uint32& OutX, uint32& OutY) const
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

	void MarkRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H, bool bOccupied)
	{
		if (SliceIndex >= OccupiedBySlice.size())
		{
			return;
		}

		std::vector<bool>& Occupied = OccupiedBySlice[SliceIndex];
		for (uint32 yy = Y; yy < Y + H; ++yy)
		{
			for (uint32 xx = X; xx < X + W; ++xx)
			{
				Occupied[Index(xx, yy)] = bOccupied;
			}
		}
	}

private:
	uint32 AtlasSize = 4096;
	uint32 MinBlockSize = 1024;
	uint32 GridCount = 4;
	uint32 NextHandle = 1;

	std::vector<std::vector<bool>> OccupiedBySlice;
	std::unordered_map<uint32, FEntry> Entries;
};
