#pragma once

#include <cstddef>
#include <filesystem>
#include <cstring>

#include <d3dcompiler.h>

#include "D3D11Common.h"

namespace D3D11Utils
{
	bool CreateImmutableBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		UINT InBindFlags,
		const void* InInitialData,
		TComPtr<ID3D11Buffer>& OutBuffer);

	bool CreateDynamicConstantBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		TComPtr<ID3D11Buffer>& OutBuffer);

	bool CreateStructuredBuffer(
		ID3D11Device* InDevice,
		UINT InElementStride,
		UINT InElementCount,
		UINT InBindFlags,
		D3D11_USAGE InUsage,
		UINT InCpuAccessFlags,
		const void* InInitialData,
		TComPtr<ID3D11Buffer>& OutBuffer);

	bool CreateStructuredBufferSRV(
		ID3D11Device* InDevice,
		ID3D11Buffer* InBuffer,
		UINT InElementCount,
		TComPtr<ID3D11ShaderResourceView>& OutShaderResourceView);

	bool CreateStructuredBufferUAV(
		ID3D11Device* InDevice,
		ID3D11Buffer* InBuffer,
		UINT InElementCount,
		TComPtr<ID3D11UnorderedAccessView>& OutUnorderedAccessView);

	bool CreateStagingBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		TComPtr<ID3D11Buffer>& OutBuffer);

	bool ReadbackBuffer(
		ID3D11DeviceContext* InDeviceContext,
		ID3D11Buffer* InSourceBuffer,
		ID3D11Buffer* InStagingBuffer,
		void* OutData,
		size_t InByteCount);

	template <typename T>
	bool UpdateDynamicBuffer(ID3D11DeviceContext* InDeviceContext, ID3D11Buffer* InBuffer, const T& InData)
	{
		if (InDeviceContext == nullptr || InBuffer == nullptr)
		{
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (FAILED(InDeviceContext->Map(InBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
		{
			return false;
		}

		std::memcpy(MappedResource.pData, &InData, sizeof(T));
		InDeviceContext->Unmap(InBuffer, 0);
		return true;
	}

	bool CompileShaderFromFile(
		const std::filesystem::path& InShaderPath,
		const char* InEntryPoint,
		const char* InTarget,
		TComPtr<ID3DBlob>& OutBlob,
		const char* InDebugName = nullptr);

	bool CompileShaderFromSource(
		const char* InSource,
		const char* InEntryPoint,
		const char* InTarget,
		TComPtr<ID3DBlob>& OutBlob,
		const char* InDebugName = nullptr);

	bool CreateComputeShaderFromFile(
		ID3D11Device* InDevice,
		const std::filesystem::path& InShaderPath,
		const char* InEntryPoint,
		TComPtr<ID3D11ComputeShader>& OutShader,
		const char* InDebugName = nullptr);
}
