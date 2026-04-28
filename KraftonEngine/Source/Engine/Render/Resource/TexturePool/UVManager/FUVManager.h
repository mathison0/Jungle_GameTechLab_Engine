#pragma once
#include "Core/CoreTypes.h"
#include "../TexturePool.h"
#include <cmath>
#include <unordered_map>
#include <vector>

struct FAtlasUV
{
	uint32 ArrayIndex;

	float u1;
	float v1;
	float u2;
	float v2;
};

class FUVManagerBase
{
public:
	virtual ~FUVManagerBase() = default;

	virtual void Initialize(uint32 InSize, uint32 InLayerCount)
	{
		Size = InSize;
		LayerCount = InLayerCount;
	}

	virtual bool GetHandle(float TextureSize, FTexturePoolBase::TexturePoolHandle& OutHandle) = 0;
	virtual FAtlasUV GetAtlasUV(const FTexturePoolBase::TexturePoolHandle& InHandle) = 0;
	virtual void ReleaseUV(const FTexturePoolBase::TexturePoolHandle& InHandle) = 0;
	virtual void BroadCastEntries() = 0;

	virtual void SetSize(uint32 InNewTextureSize)
	{
		Size = InNewTextureSize;
		BroadCastEntries();
	}

	virtual void SetLayerCount(uint32 InNewLayerCount)
	{
		LayerCount = InNewLayerCount;
		BroadCastEntries();
	}

protected:
	uint32 GetSize() const { return Size; }
	uint32 GetLayerCount() const { return LayerCount; }

private:
	uint32 Size = 0;
	uint32 LayerCount = 0;
};

class TempManager : public FUVManagerBase
{
public:
	virtual bool GetHandle(float TextureSize, FTexturePoolBase::TexturePoolHandle& OutHandle) { return false; };
	virtual FAtlasUV GetAtlasUV(const FTexturePoolBase::TexturePoolHandle& InHandle) { return FAtlasUV(); };
	virtual void ReleaseUV(const FTexturePoolBase::TexturePoolHandle& InHandle) {};
	virtual void BroadCastEntries() {};
};

class FGridUVManager : public FUVManagerBase
{
public:
	virtual void Initialize(uint32 InAtlasSize, uint32 InLayerCount) override
	{
		Initialize(InAtlasSize, InLayerCount, MinBlockSize);
	}

	void Initialize(uint32 InAtlasSize, uint32 InLayerCount, uint32 InMinBlockSize)
	{
		FUVManagerBase::Initialize(InAtlasSize, InLayerCount);

		AtlasSize = InAtlasSize;
		MinBlockSize = InMinBlockSize;
		GridCount = AtlasSize / MinBlockSize;

		ResetSliceOccupancy(InLayerCount);
		NextHandle = 1;
		Entries.clear();
	}

	virtual bool GetHandle(float TextureSize, FTexturePoolBase::TexturePoolHandle& OutHandle) override
	{
		const uint32 RequestSize = static_cast<uint32>(std::ceil(TextureSize));
		const uint32 BlockCount = CeilDiv(RequestSize, MinBlockSize);

		if (BlockCount == 0 || BlockCount > GridCount)
			return false;

		for (uint32 SliceIndex = 0; SliceIndex < GetLayerCount(); ++SliceIndex)
		{
			uint32 OutX = 0;
			uint32 OutY = 0;

			if (!FindFreeRect(SliceIndex, BlockCount, BlockCount, OutX, OutY))
				continue;

			MarkRect(SliceIndex, OutX, OutY, BlockCount, BlockCount, true);

			const uint32 Handle = NextHandle++;

			FEntry Entry;
			Entry.X = OutX;
			Entry.Y = OutY;
			Entry.W = BlockCount;
			Entry.H = BlockCount;
			Entry.ArrayIndex = SliceIndex;

			Entries.emplace(Handle, Entry);

			OutHandle.InternalIndex = Handle;
			OutHandle.ArrayIndex = SliceIndex;
			return true;
		}

		return false;
	}

	virtual FAtlasUV GetAtlasUV(const FTexturePoolBase::TexturePoolHandle& InHandle) override
	{
		auto It = Entries.find(InHandle.InternalIndex);
		if (It == Entries.end())
			return {};

		const FEntry& Entry = It->second;

		const float PixelX1 = static_cast<float>(Entry.X * MinBlockSize);
		const float PixelY1 = static_cast<float>(Entry.Y * MinBlockSize);
		const float PixelX2 = static_cast<float>((Entry.X + Entry.W) * MinBlockSize);
		const float PixelY2 = static_cast<float>((Entry.Y + Entry.H) * MinBlockSize);

		FAtlasUV UV;
		UV.ArrayIndex = Entry.ArrayIndex;
		UV.u1 = PixelX1 / AtlasSize;
		UV.v1 = PixelY1 / AtlasSize;
		UV.u2 = PixelX2 / AtlasSize;
		UV.v2 = PixelY2 / AtlasSize;

		return UV;
	}

	virtual void ReleaseUV(const FTexturePoolBase::TexturePoolHandle& InHandle) override
	{
		auto It = Entries.find(InHandle.InternalIndex);
		if (It == Entries.end())
			return;

		const FEntry& Entry = It->second;
		MarkRect(Entry.ArrayIndex, Entry.X, Entry.Y, Entry.W, Entry.H, false);
		Entries.erase(It);
	}

	virtual void BroadCastEntries() override
	{
		// Atlas 크기가 바뀌었을 때 기존 UV를 다시 계산해야 하는 구조라면 여기서 알림.
		// 지금 단순 버전에서는 GetAtlasUV()가 매번 현재 AtlasSize 기준으로 계산하므로 비워둬도 됨.
	}

	virtual void SetSize(uint32 InNewTextureSize) override
	{
		FUVManagerBase::SetSize(InNewTextureSize);
		AtlasSize = InNewTextureSize;
		GridCount = AtlasSize / MinBlockSize;
		ResetSliceOccupancy(GetLayerCount());
	}

	virtual void SetLayerCount(uint32 InNewLayerCount) override
	{
		FUVManagerBase::SetLayerCount(InNewLayerCount);
		ResizeSliceOccupancy(InNewLayerCount);
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

	void ResizeSliceOccupancy(uint32 InLayerCount)
	{
		const size_t CellCount = static_cast<size_t>(GridCount) * static_cast<size_t>(GridCount);
		OccupiedBySlice.resize(InLayerCount);

		for (std::vector<bool>& Occupied : OccupiedBySlice)
		{
			Occupied.resize(CellCount, false);
		}
	}

	bool IsFreeRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H) const
	{
		if (SliceIndex >= OccupiedBySlice.size() || X + W > GridCount || Y + H > GridCount)
			return false;

		const std::vector<bool>& Occupied = OccupiedBySlice[SliceIndex];
		for (uint32 yy = Y; yy < Y + H; ++yy)
		{
			for (uint32 xx = X; xx < X + W; ++xx)
			{
				if (Occupied[Index(xx, yy)])
					return false;
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
