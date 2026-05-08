#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"

class FMeshBuffer
{
public:
    virtual ~FMeshBuffer() = default;

    virtual void          Release()					  = 0;
    virtual bool          IsValid()				const = 0;
    virtual ID3D11Buffer* GetVertexBufferRaw()  const = 0;
    virtual uint32        GetVertexStride()		const = 0;
    virtual uint32        GetVertexCount()		const = 0;
    virtual ID3D11Buffer* GetIndexBufferRaw()	const = 0;
    virtual uint32        GetIndexCount()		const = 0;
};
