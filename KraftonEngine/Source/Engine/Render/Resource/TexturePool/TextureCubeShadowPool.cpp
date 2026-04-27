#include "TextureCubeShadowPool.h"
namespace
{
	uint32 MakeTierResolution(uint32 BaseResolution, uint32 TierIndex)
	{
		const uint32 SafeBase = BaseResolution > 0 ? BaseResolution : 1024;
		switch (TierIndex)
		{
		case 0: return SafeBase / 4 > 0 ? SafeBase / 4 : 1u;
		case 1: return SafeBase / 2 > 0 ? SafeBase / 2 : 1u;
		case 2: return SafeBase;
		default: return SafeBase * 2;
		}
	}
}

void FTextureCubeShadowPool::Initialize(ID3D11Device* InDevice, uint32 InBaseResolution, uint32 InitialCubeCapacity)
{
	Release();

	Device = InDevice;
	BaseResolution = InBaseResolution > 0 ? InBaseResolution : 1024;

	const uint32 SafeInitialCapacity = InitialCubeCapacity > 0 ? InitialCubeCapacity : 1;
	for (uint32 TierIndex = 0; TierIndex < TierCount; ++TierIndex)
	{
		Tiers[TierIndex].Resolution = MakeTierResolution(BaseResolution, TierIndex);
		RebuildResources(TierIndex, SafeInitialCapacity);
	}
}

void FTextureCubeShadowPool::Release()
{
	for (FTierPool& Tier : Tiers)
	{
		Tier.FaceDSVs.clear();
		Tier.SRV.Reset();
		Tier.Texture.Reset();
		Tier.AllocationFlags.clear();
		Tier.FreeCubeIndices.clear();
		Tier.Resolution = 0;
		Tier.CubeCapacity = 0;
		Tier.AllocatedCount = 0;
	}

	Device = nullptr;
	BaseResolution = 1024;
}

FTextureCubeShadowPool::FCubeShadowHandle FTextureCubeShadowPool::Allocate(float ResolutionScale)
{
	const uint32 TierIndex = GetTierIndexForScale(ResolutionScale);
	FTierPool* Tier = GetTier(TierIndex);
	if (!Device || !Tier)
	{
		return {};
	}

	if (Tier->FreeCubeIndices.empty())
	{
		const uint32 NewCapacity = Tier->CubeCapacity > 0 ? Tier->CubeCapacity * 2 : 1;
		Resize(TierIndex, NewCapacity);
	}

	if (Tier->FreeCubeIndices.empty())
	{
		return {};
	}

	const uint32 CubeIndex = Tier->FreeCubeIndices.back();
	Tier->FreeCubeIndices.pop_back();

	if (CubeIndex >= Tier->AllocationFlags.size())
	{
		return {};
	}

	Tier->AllocationFlags[CubeIndex] = 1;
	++Tier->AllocatedCount;

	FCubeShadowHandle Handle;
	Handle.CubeIndex = CubeIndex;
	Handle.TierIndex = TierIndex;
	return Handle;
}

void FTextureCubeShadowPool::ReleaseHandle(FCubeShadowHandle Handle)
{
	FTierPool* Tier = GetTier(Handle.TierIndex);
	if (!Handle.IsValid() || !Tier || Handle.CubeIndex >= Tier->AllocationFlags.size())
	{
		return;
	}

	if (Tier->AllocationFlags[Handle.CubeIndex] == 0)
	{
		return;
	}

	Tier->AllocationFlags[Handle.CubeIndex] = 0;
	Tier->FreeCubeIndices.push_back(Handle.CubeIndex);

	if (Tier->AllocatedCount > 0)
	{
		--Tier->AllocatedCount;
	}
}

ID3D11ShaderResourceView* FTextureCubeShadowPool::GetSRV(uint32 TierIndex) const
{
	const FTierPool* Tier = GetTier(TierIndex);
	return Tier ? Tier->SRV.Get() : nullptr;
}

ID3D11DepthStencilView* FTextureCubeShadowPool::GetFaceDSV(FCubeShadowHandle Handle, uint32 FaceIndex) const
{
	const FTierPool* Tier = GetTier(Handle.TierIndex);
	if (!Handle.IsValid() || !Tier || FaceIndex >= CubeFaceCount)
	{
		return nullptr;
	}

	const uint32 SliceIndex = GetSliceIndex(Handle, FaceIndex);
	if (SliceIndex >= Tier->FaceDSVs.size())
	{
		return nullptr;
	}

	return Tier->FaceDSVs[SliceIndex].Get();
}

uint32 FTextureCubeShadowPool::GetResolution(FCubeShadowHandle Handle) const
{
	return GetResolutionForTier(Handle.TierIndex);
}

uint32 FTextureCubeShadowPool::GetResolutionForTier(uint32 TierIndex) const
{
	const FTierPool* Tier = GetTier(TierIndex);
	return Tier ? Tier->Resolution : 0;
}

uint32 FTextureCubeShadowPool::GetTierIndexForScale(float ResolutionScale) const
{
	const float SafeScale = ResolutionScale > 0.0f ? ResolutionScale : 0.0f;
	if (SafeScale <= 0.25f)
	{
		return 0;
	}
	if (SafeScale <= 0.5f)
	{
		return 1;
	}
	if (SafeScale <= 1.0f)
	{
		return 2;
	}
	return 3;
}

uint32 FTextureCubeShadowPool::GetCapacity(uint32 TierIndex) const
{
	const FTierPool* Tier = GetTier(TierIndex);
	return Tier ? Tier->CubeCapacity : 0;
}

uint32 FTextureCubeShadowPool::GetAllocatedCount(uint32 TierIndex) const
{
	const FTierPool* Tier = GetTier(TierIndex);
	return Tier ? Tier->AllocatedCount : 0;
}

void FTextureCubeShadowPool::Resize(uint32 TierIndex, uint32 NewCubeCapacity)
{
	FTierPool* Tier = GetTier(TierIndex);
	if (!Tier || NewCubeCapacity <= Tier->CubeCapacity)
	{
		return;
	}

	RebuildResources(TierIndex, NewCubeCapacity);
}

bool FTextureCubeShadowPool::RebuildResources(uint32 TierIndex, uint32 NewCubeCapacity)
{
	FTierPool* Tier = GetTier(TierIndex);
	if (!Device || !Tier || Tier->Resolution == 0 || NewCubeCapacity == 0)
	{
		return false;
	}

	TComPtr<ID3D11Texture2D> NewTexture;
	TComPtr<ID3D11ShaderResourceView> NewSRV;
	TArray<TComPtr<ID3D11DepthStencilView>> NewFaceDSVs;

	const uint32 TotalSlices = NewCubeCapacity * CubeFaceCount;

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Tier->Resolution;
	TextureDesc.Height = Tier->Resolution;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = TotalSlices;
	TextureDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT hr = Device->CreateTexture2D(&TextureDesc, nullptr, NewTexture.GetAddressOf());
	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
	SRVDesc.TextureCubeArray.MostDetailedMip = 0;
	SRVDesc.TextureCubeArray.MipLevels = 1;
	SRVDesc.TextureCubeArray.First2DArrayFace = 0;
	SRVDesc.TextureCubeArray.NumCubes = NewCubeCapacity;

	hr = Device->CreateShaderResourceView(NewTexture.Get(), &SRVDesc, NewSRV.GetAddressOf());
	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	NewFaceDSVs.resize(TotalSlices);
	for (uint32 SliceIndex = 0; SliceIndex < TotalSlices; ++SliceIndex)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
		DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
		DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		DSVDesc.Texture2DArray.MipSlice = 0;
		DSVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
		DSVDesc.Texture2DArray.ArraySize = 1;

		hr = Device->CreateDepthStencilView(NewTexture.Get(), &DSVDesc, NewFaceDSVs[SliceIndex].GetAddressOf());
		if (FAILED(hr))
		{
			assert(false);
			return false;
		}
	}

	const uint32 OldCapacity = Tier->CubeCapacity;

	Tier->Texture = std::move(NewTexture);
	Tier->SRV = std::move(NewSRV);
	Tier->FaceDSVs = std::move(NewFaceDSVs);
	Tier->CubeCapacity = NewCubeCapacity;

	Tier->AllocationFlags.resize(Tier->CubeCapacity, 0);
	for (uint32 CubeIndex = Tier->CubeCapacity; CubeIndex > OldCapacity; --CubeIndex)
	{
		Tier->FreeCubeIndices.push_back(CubeIndex - 1);
	}

	return true;
}

uint32 FTextureCubeShadowPool::GetSliceIndex(FCubeShadowHandle Handle, uint32 FaceIndex) const
{
	return Handle.CubeIndex * CubeFaceCount + FaceIndex;
}

FTextureCubeShadowPool::FTierPool* FTextureCubeShadowPool::GetTier(uint32 TierIndex)
{
	return TierIndex < TierCount ? &Tiers[TierIndex] : nullptr;
}

const FTextureCubeShadowPool::FTierPool* FTextureCubeShadowPool::GetTier(uint32 TierIndex) const
{
	return TierIndex < TierCount ? &Tiers[TierIndex] : nullptr;
}
