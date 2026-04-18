#include "ShaderManager.h"
#include "VertexLayouts.h"
#include "Render/Resource/Shader.h"

FShader* FShaderManager::GetShader(const FShaderKey& Key)
{
    auto It = ShaderMap.find(Key);
    if (It != ShaderMap.end())
        return It->second.get();

    return nullptr;

}

void FShaderManager::PreloadShaders(ID3D11Device* Device)
{

    for (int view = 0; view < 3; ++view)
    {
        FShaderKey Key;
        Key.SetViewMode(view);

        if (!ShaderMap.contains(Key))
        {
            CreateShader(Device, Key);
        }
    }
}

FShader* FShaderManager::CreateShader(ID3D11Device* Device, const FShaderKey& Key)
{
    std::unique_ptr<FShader> Shader = std::make_unique<FShader>();

    FString view = std::to_string(Key.Bits & 0b11);

    D3D_SHADER_MACRO Defines[] = 
	{
		{"VIEW_MODE", view.c_str()}, 
		{"USE_NORMALMAP", (Key.Bits & NORMALMAP_BIT) ? "1" : "0"},
		{nullptr, nullptr}};

    Shader->Create(Device, L"UberShader.hlsl", "VS", "PS", VertexLayouts::NormalVertexInputLayout, ARRAYSIZE(VertexLayouts::NormalVertexInputLayout));

    FShader* Result = Shader.get();
    ShaderMap.emplace(Key, std::move(Shader));

    return Result;
}
