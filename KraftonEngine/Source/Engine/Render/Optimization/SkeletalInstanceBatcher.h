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
};

class FSkeletalInstanceBatcher
{
public:
	void Create(ID3D11Device* InDevice, ID3D11DeviceContext* InContext);
	void Release();

	void BuildInstancedCommands(const TArray<FDrawCommand>& InCommands, TArray<FDrawCommand>& OutCommands);

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;

	FDynamicVertexBuffer InstanceBuffer;
	TArray<FSkeletalInstanceData> InstanceData;
};
