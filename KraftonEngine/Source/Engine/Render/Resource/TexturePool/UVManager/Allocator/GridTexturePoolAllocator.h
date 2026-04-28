#pragma once

#include "Render/Resource/TexturePool/UVManager/Allocator/TexturePoolAllocatorBase.h"
#include <unordered_map>
#include <vector>

class FGridTexturePoolAllocator : public FTexturePoolAllocatorBase
{
public:
	virtual void Initialize(uint32 InAtlasSize, uint32 InLayerCount, uint32 InMinBlockSize = 32) override;
	virtual bool AllocateHandle(float TextureSize, FTexturePoolHandle& OutHandle) override;
	virtual FAtlasUV GetAtlasUV(const FTexturePoolHandle& InHandle) override;
	virtual void ReleaseHandle(const FTexturePoolHandle& InHandle) override;
	virtual void BroadcastEntries() override;
	virtual void SetSize(uint32 InNewTextureSize) override;
	virtual void SetLayerCount(uint32 InNewLayerCount) override;

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
	static uint32 CeilDiv(uint32 A, uint32 B);

	uint32 Index(uint32 X, uint32 Y) const;
	void ResetSliceOccupancy(uint32 InLayerCount);
	void RebuildOccupancyFromEntries();
	bool IsFreeRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H) const;
	bool FindFreeRect(uint32 SliceIndex, uint32 W, uint32 H, uint32& OutX, uint32& OutY) const;
	void MarkRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H, bool bOccupied);

private:
	uint32 GridCount = 4;
	uint32 NextHandle = 1;

	std::vector<std::vector<bool>> OccupiedBySlice;
	std::unordered_map<uint32, FEntry> Entries;
};
