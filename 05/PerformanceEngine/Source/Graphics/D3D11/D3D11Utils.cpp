#include "Graphics/D3D11/D3D11Utils.h"

namespace
{
	void OutputShaderCompilerMessages(ID3DBlob* InErrorBlob)
	{
		if (InErrorBlob == nullptr || InErrorBlob->GetBufferPointer() == nullptr)
		{
			return;
		}

		OutputDebugStringA(static_cast<const char*>(InErrorBlob->GetBufferPointer()));
		OutputDebugStringA("\n");
	}

	void OutputCompileFailureHeader(const char* InDebugName)
	{
		OutputDebugStringA("[D3D11Utils] Failed to compile shader");
		if (InDebugName != nullptr && InDebugName[0] != '\0')
		{
			OutputDebugStringA(": ");
			OutputDebugStringA(InDebugName);
		}

		OutputDebugStringA("\n");
	}
}

namespace D3D11Utils
{
	bool CreateImmutableBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		UINT InBindFlags,
		const void* InInitialData,
		TComPtr<ID3D11Buffer>& OutBuffer)
	{
		if (InDevice == nullptr || InInitialData == nullptr)
		{
			return false;
		}

		const D3D11_BUFFER_DESC BufferDesc =
		{
			InByteWidth,
			D3D11_USAGE_IMMUTABLE,
			InBindFlags,
			0,
			0,
			0
		};

		const D3D11_SUBRESOURCE_DATA InitialData =
		{
			InInitialData,
			0,
			0
		};

		return SUCCEEDED(InDevice->CreateBuffer(&BufferDesc, &InitialData, OutBuffer.GetAddressOf()));
	}

	bool CreateDynamicConstantBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		TComPtr<ID3D11Buffer>& OutBuffer)
	{
		if (InDevice == nullptr)
		{
			return false;
		}

		const D3D11_BUFFER_DESC BufferDesc =
		{
			InByteWidth,
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_CONSTANT_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0,
			0
		};

		return SUCCEEDED(InDevice->CreateBuffer(&BufferDesc, nullptr, OutBuffer.GetAddressOf()));
	}

	bool CreateStructuredBuffer(
		ID3D11Device* InDevice,
		UINT InElementStride,
		UINT InElementCount,
		UINT InBindFlags,
		D3D11_USAGE InUsage,
		UINT InCpuAccessFlags,
		const void* InInitialData,
		TComPtr<ID3D11Buffer>& OutBuffer)
	{
		if (InDevice == nullptr || InElementStride == 0 || InElementCount == 0)
		{
			return false;
		}

		D3D11_BUFFER_DESC BufferDesc = {};
		BufferDesc.ByteWidth = InElementStride * InElementCount;
		BufferDesc.Usage = InUsage;
		BufferDesc.BindFlags = InBindFlags;
		BufferDesc.CPUAccessFlags = InCpuAccessFlags;
		BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		BufferDesc.StructureByteStride = InElementStride;

		D3D11_SUBRESOURCE_DATA InitialData = {};
		D3D11_SUBRESOURCE_DATA* InitialDataPtr = nullptr;
		if (InInitialData != nullptr)
		{
			InitialData.pSysMem = InInitialData;
			InitialDataPtr = &InitialData;
		}

		return SUCCEEDED(InDevice->CreateBuffer(&BufferDesc, InitialDataPtr, OutBuffer.GetAddressOf()));
	}

	bool CreateStructuredBufferSRV(
		ID3D11Device* InDevice,
		ID3D11Buffer* InBuffer,
		UINT InElementCount,
		TComPtr<ID3D11ShaderResourceView>& OutShaderResourceView)
	{
		if (InDevice == nullptr || InBuffer == nullptr || InElementCount == 0)
		{
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC Desc = {};
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		Desc.Buffer.FirstElement = 0;
		Desc.Buffer.NumElements = InElementCount;
		return SUCCEEDED(InDevice->CreateShaderResourceView(InBuffer, &Desc, OutShaderResourceView.GetAddressOf()));
	}

	bool CreateStructuredBufferUAV(
		ID3D11Device* InDevice,
		ID3D11Buffer* InBuffer,
		UINT InElementCount,
		TComPtr<ID3D11UnorderedAccessView>& OutUnorderedAccessView)
	{
		if (InDevice == nullptr || InBuffer == nullptr || InElementCount == 0)
		{
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC Desc = {};
		Desc.Format = DXGI_FORMAT_UNKNOWN;
		Desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		Desc.Buffer.FirstElement = 0;
		Desc.Buffer.NumElements = InElementCount;
		return SUCCEEDED(InDevice->CreateUnorderedAccessView(InBuffer, &Desc, OutUnorderedAccessView.GetAddressOf()));
	}

	bool CreateStagingBuffer(
		ID3D11Device* InDevice,
		UINT InByteWidth,
		TComPtr<ID3D11Buffer>& OutBuffer)
	{
		if (InDevice == nullptr || InByteWidth == 0)
		{
			return false;
		}

		D3D11_BUFFER_DESC BufferDesc = {};
		BufferDesc.ByteWidth = InByteWidth;
		BufferDesc.Usage = D3D11_USAGE_STAGING;
		BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		return SUCCEEDED(InDevice->CreateBuffer(&BufferDesc, nullptr, OutBuffer.GetAddressOf()));
	}

	bool ReadbackBuffer(
		ID3D11DeviceContext* InDeviceContext,
		ID3D11Buffer* InSourceBuffer,
		ID3D11Buffer* InStagingBuffer,
		void* OutData,
		size_t InByteCount)
	{
		if (InDeviceContext == nullptr || InSourceBuffer == nullptr || InStagingBuffer == nullptr || OutData == nullptr || InByteCount == 0)
		{
			return false;
		}

		InDeviceContext->CopyResource(InStagingBuffer, InSourceBuffer);

		D3D11_MAPPED_SUBRESOURCE Mapped = {};
		if (FAILED(InDeviceContext->Map(InStagingBuffer, 0, D3D11_MAP_READ, 0, &Mapped)))
		{
			return false;
		}

		std::memcpy(OutData, Mapped.pData, InByteCount);
		InDeviceContext->Unmap(InStagingBuffer, 0);
		return true;
	}

	bool CompileShaderFromFile(
		const std::filesystem::path& InShaderPath,
		const char* InEntryPoint,
		const char* InTarget,
		TComPtr<ID3DBlob>& OutBlob,
		const char* InDebugName)
	{
		if (InShaderPath.empty())
		{
			return false;
		}

		UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		TComPtr<ID3DBlob> ErrorBlob;
		const HRESULT Result = D3DCompileFromFile(
			InShaderPath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			InEntryPoint,
			InTarget,
			CompileFlags,
			0,
			OutBlob.GetAddressOf(),
			ErrorBlob.GetAddressOf());

		if (FAILED(Result))
		{
			OutputCompileFailureHeader(InDebugName);
			OutputDebugStringW(InShaderPath.c_str());
			OutputDebugStringA("\n");
			OutputShaderCompilerMessages(ErrorBlob.Get());
			return false;
		}

		return true;
	}

	bool CompileShaderFromSource(
		const char* InSource,
		const char* InEntryPoint,
		const char* InTarget,
		TComPtr<ID3DBlob>& OutBlob,
		const char* InDebugName)
	{
		if (InSource == nullptr)
		{
			return false;
		}

		UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		TComPtr<ID3DBlob> ErrorBlob;
		const HRESULT Result = D3DCompile(
			InSource,
			std::strlen(InSource),
			nullptr,
			nullptr,
			nullptr,
			InEntryPoint,
			InTarget,
			CompileFlags,
			0,
			OutBlob.GetAddressOf(),
			ErrorBlob.GetAddressOf());

		if (FAILED(Result))
		{
			OutputCompileFailureHeader(InDebugName);
			OutputShaderCompilerMessages(ErrorBlob.Get());
			return false;
		}

		return true;
	}

	bool CreateComputeShaderFromFile(
		ID3D11Device* InDevice,
		const std::filesystem::path& InShaderPath,
		const char* InEntryPoint,
		TComPtr<ID3D11ComputeShader>& OutShader,
		const char* InDebugName)
	{
		if (InDevice == nullptr)
		{
			return false;
		}

		TComPtr<ID3DBlob> ShaderBlob;
		if (!CompileShaderFromFile(InShaderPath, InEntryPoint, "cs_5_0", ShaderBlob, InDebugName))
		{
			return false;
		}

		return SUCCEEDED(InDevice->CreateComputeShader(
			ShaderBlob->GetBufferPointer(),
			ShaderBlob->GetBufferSize(),
			nullptr,
			OutShader.GetAddressOf()));
	}
}
