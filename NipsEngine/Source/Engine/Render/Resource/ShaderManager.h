#pragma once
#include "Core/CoreMinimal.h"
#include <d3d11.h>

// 현재 발제 내용에서 하나의 Uber Shader로 여러 라이팅 모델을 렌더링해야합니다.
// 따라서 ShaderKey로 ViewMode를 사용해서 Opaque 패스가 같은 셰이더를 바인딩 할 계획입니다.
// 이후 이 클래스를 마주친 기구한 인원이 있다면, 머테리얼이 ShaderKey값을 통해
// 필요한 셰이더를 찾을 수 있도록 개선해주세요.

class FShader;

enum EShaderKeyBits : uint32
{
    VIEWMODE_MASK = 0b00000000000000000000000000000011, // 2 bits
    NORMALMAP_BIT = 1 << 2,
    SKINNING_BIT = 1 << 3,
    // 필요하면 계속 추가
};

struct FShaderKey
{
    uint32 Bits = 0;

    bool operator==(const FShaderKey& other) const { return Bits == other.Bits; }

    inline void SetViewMode(uint32 view)
    {
        Bits &= ~VIEWMODE_MASK;
        Bits |= (view & 0b11);
    }

	inline void SetNormalMap(bool enable)
    {
        if (enable)
            Bits |= NORMALMAP_BIT;
        else
            Bits &= ~NORMALMAP_BIT;
    }
};

struct FShaderKeyHash
{
    size_t operator()(const FShaderKey& key) const { return std::hash<uint32>()(key.Bits); }
};

class FShaderManager
{
  public:
    FShader* GetShader(const FShaderKey& Key);
    void     PreloadShaders(ID3D11Device* Device);

  private:
    FShader* CreateShader(ID3D11Device* Device, const FShaderKey& Key);

  private:
    TMap<FShaderKey, std::unique_ptr<FShader>, FShaderKeyHash> ShaderMap;
};
