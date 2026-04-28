#pragma once

#include "Render/Resource/TexturePool/UVManager/Allocator/TexturePoolAllocatorBase.h"
#include <unordered_map>
#include <vector>

class FGridTexturePoolAllocator : public FTexturePoolAllocatorBase
{
public:
	virtual void Initialize(uint32 InAtlasSize, uint32 InLayerCount, uint32 InMinBlockSize = 32) override;
	virtual bool AllocateHandle(float TextureSize, FTexturePoolHandle& OutHandle) override;
	virtual bool CanAllocateHandle(float TextureSize) const override;
	virtual bool CanAllocateHandleSet(const FTexturePoolHandleRequest& Request) const override;
	virtual FAtlasUV GetAtlasUV(const FTexturePoolHandle& InHandle) override;
	virtual void ReleaseHandle(const FTexturePoolHandle& InHandle) override;
	virtual void BroadcastEntries() override;
	virtual uint32 GetFreeRectCount() const override;
	virtual uint64 GetTotalFreeArea() const override;
	virtual uint64 GetLargestFreeRectArea() const override;
	virtual float GetFragmentationRatio() const override;
	virtual void SetSize(uint32 InNewTextureSize) override;
	virtual void SetLayerCount(uint32 InNewLayerCount) override;

private:
	struct FAtlasRect
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
	void ResetFreeRects(uint32 InLayerCount);
	void ResetAllocationState(uint32 InLayerCount);
	void MarkRect(uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H, bool bOccupied);
	void MarkRectInOccupancy(std::vector<std::vector<bool>>& Occupancy, uint32 SliceIndex, uint32 X, uint32 Y, uint32 W, uint32 H, bool bOccupied) const;
	uint32 GetBlockCount(float TextureSize) const;
	int32 FindBestFreeRect(uint32 W, uint32 H, const TArray<FAtlasRect>& InFreeRects) const;
	bool TryPlaceRectIntoFreeRects(TArray<FAtlasRect>& InOutFreeRects, uint32 W, uint32 H, FAtlasRect& OutRect) const;
	void SplitFreeRect(TArray<FAtlasRect>& InOutFreeRects, uint32 FreeRectIndex, const FAtlasRect& Used) const;
	void PruneContainedFreeRects(TArray<FAtlasRect>& InOutFreeRects) const;
	void MergeAdjacentFreeRects(TArray<FAtlasRect>& InOutFreeRects) const;
	bool IsContained(const FAtlasRect& Inner, const FAtlasRect& Outer) const;

private:
	uint32 GridCount = 4;
	uint32 NextHandle = 1;

	std::vector<std::vector<bool>> OccupiedBySlice;
	TArray<FAtlasRect> FreeRects;
	std::unordered_map<uint32, FAtlasRect> AllocatedRects;
};
