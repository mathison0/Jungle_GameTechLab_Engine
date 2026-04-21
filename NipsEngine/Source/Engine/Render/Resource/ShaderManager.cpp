#include "ShaderManager.h"
#include "VertexLayouts.h"
#include "Render/Resource/Shader.h"
#include "Render/Common/ViewTypes.h"

FShader* FShaderManager::GetShader(const FShaderKey& Key)
{
    auto It = ShaderMap.find(Key);
    if (It != ShaderMap.end())
    {
        return It->second.get();
    }

    if (CachedDevice != nullptr)
    {
        return CreateShader(CachedDevice, Key);
    }

    return nullptr;
}

void FShaderManager::PreloadShaders(ID3D11Device* Device)
{
    CachedDevice = Device;

    for (int view = 0; view < (int)EViewMode::Count; ++view)
    {
        for (int bNormal = 0; bNormal <= 1; ++bNormal)
        {
            FShaderKey Key;
            Key.SetViewMode(view);
            Key.SetNormalMap(bNormal != 0);

            if (!ShaderMap.contains(Key))
            {
                CreateShader(Device, Key);
            }
        }
    }
}

FShader* FShaderManager::CreateShader(ID3D11Device* Device, const FShaderKey& Key)
{
    std::unique_ptr<FShader> Shader = std::make_unique<FShader>();

	static const char* ViewModeTable[] = {"0", "1", "2",  "3",  "4",  "5",  "6",  "7",
                                          "8", "9", "10", "11", "12", "13", "14", "15"};

	uint32 ViewModeIndex = Key.Bits & VIEWMODE_MASK;

    D3D_SHADER_MACRO Defines[] = 
		{{"VIEW_MODE", ViewModeTable[ViewModeIndex]}, 
		{"USE_NORMALMAP", (Key.Bits & NORMALMAP_BIT) ? "1" : "0"},
		{nullptr, nullptr}};

    Shader->Create(Device, L"Shaders/UberLit.hlsl", "VS", "PS", VertexLayouts::NormalVertexInputLayout, ARRAYSIZE(VertexLayouts::NormalVertexInputLayout),Defines);

    FShader* Result = Shader.get();
    ShaderMap.emplace(Key, std::move(Shader));

    return Result;
}
