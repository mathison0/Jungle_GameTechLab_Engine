#pragma once
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/VertexTypes.h"

struct FVertex;
struct FNormalVertex;

namespace VertexLayouts
{
    //	Primitive and Gizmo Input Layout
    extern const D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2];


	// StaticMesh (FNormalVertex) Input Layout
    extern const D3D11_INPUT_ELEMENT_DESC NormalVertexInputLayout[3];
}
