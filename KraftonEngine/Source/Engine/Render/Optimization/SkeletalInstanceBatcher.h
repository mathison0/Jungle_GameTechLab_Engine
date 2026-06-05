#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Resource/Buffer.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

struct FSkeletalInstanceData
{
	FVector4 World0;
	FVector4 World1;
	FVector4 World2;
	FVector4 World3;
	FVector4 InstanceColor;

	uint32 SkinMatrixOffset = 0;
	uint32 Padding0 = 0;
	uint32 Padding1 = 0;
	uint32 Padding2 = 0;
};

class FSkeletalInstanceBatcher
{
public:
	void Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	void Release();

	void BuildInstancedCommands(const TArray<FDrawCommand>& InCommands, TArray<FDrawCommand>& OutCommands);

private:
	bool EnsureGlobalSkinMatrixBuffer(uint32 RequiredMatrixCount);

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;

	FDynamicVertexBuffer InstanceBuffer;
	TArray<FSkeletalInstanceData> InstanceData;

	ID3D11Buffer* GlobalSkinMatrixBuffer = nullptr;
	ID3D11ShaderResourceView* GlobalSkinMatrixSRV = nullptr;
	uint32 GlobalSkinMatrixCapacity = 0;

	TArray<FMatrix> GlobalSkinMatrices;
};
