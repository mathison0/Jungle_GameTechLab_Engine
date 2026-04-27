#pragma once

#include "Core/CoreTypes.h"
#include <d3d11.h>
#include <wrl/client.h>

class FTextureCubeShadowPool final
{
	template<typename T>
	using TComPtr = Microsoft::WRL::ComPtr<T>;

public:
	static constexpr uint32 InvalidCubeIndex = static_cast<uint32>(-1);
	static constexpr uint32 InvalidTierIndex = static_cast<uint32>(-1);
	static constexpr uint32 CubeFaceCount = 6;
	static constexpr uint32 TierCount = 4;

	struct FCubeShadowHandle
	{
		uint32 CubeIndex = InvalidCubeIndex;
		uint32 TierIndex = InvalidTierIndex;

		bool IsValid() const { return CubeIndex != InvalidCubeIndex && TierIndex != InvalidTierIndex; }
	};

	static FTextureCubeShadowPool& Get()
	{
		static FTextureCubeShadowPool Instance;
		return Instance;
	}

	FTextureCubeShadowPool(const FTextureCubeShadowPool&) = delete;
	FTextureCubeShadowPool& operator=(const FTextureCubeShadowPool&) = delete;
	FTextureCubeShadowPool(FTextureCubeShadowPool&&) = delete;
	FTextureCubeShadowPool& operator=(FTextureCubeShadowPool&&) = delete;

	void Initialize(ID3D11Device* InDevice, uint32 InBaseResolution, uint32 InitialCubeCapacity = 1);
	void Release();

	FCubeShadowHandle Allocate(float ResolutionScale = 1.0f);
	void ReleaseHandle(FCubeShadowHandle Handle);

	ID3D11ShaderResourceView* GetSRV(uint32 TierIndex) const;
	ID3D11DepthStencilView* GetFaceDSV(FCubeShadowHandle Handle, uint32 FaceIndex) const;

	uint32 GetResolution(FCubeShadowHandle Handle) const;
	uint32 GetResolutionForTier(uint32 TierIndex) const;
	uint32 GetTierIndexForScale(float ResolutionScale) const;
	uint32 GetCapacity(uint32 TierIndex) const;
	uint32 GetAllocatedCount(uint32 TierIndex) const;
	bool IsInitialized() const { return Device != nullptr; }

private:
	FTextureCubeShadowPool() = default;
	~FTextureCubeShadowPool() = default;

	struct FTierPool
	{
		uint32 Resolution = 0;
		uint32 CubeCapacity = 0;
		uint32 AllocatedCount = 0;

		TComPtr<ID3D11Texture2D> Texture;
		TComPtr<ID3D11ShaderResourceView> SRV;
		TArray<TComPtr<ID3D11DepthStencilView>> FaceDSVs;
		TArray<uint8> AllocationFlags;
		TArray<uint32> FreeCubeIndices;
	};

	void Resize(uint32 TierIndex, uint32 NewCubeCapacity);
	bool RebuildResources(uint32 TierIndex, uint32 NewCubeCapacity);
	uint32 GetSliceIndex(FCubeShadowHandle Handle, uint32 FaceIndex) const;
	FTierPool* GetTier(uint32 TierIndex);
	const FTierPool* GetTier(uint32 TierIndex) const;

private:
	ID3D11Device* Device = nullptr;
	uint32 BaseResolution = 1024;
	FTierPool Tiers[TierCount];
};
