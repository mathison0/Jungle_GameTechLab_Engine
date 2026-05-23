#pragma once

#include "Viewport/EditorViewportClient.h"

struct FParticleSystemViewportShowFlags
{
	bool bGrid = true;
	bool bAxis = true;
	bool bBounds = false;
};

class FParticleSystemViewportClient : public FEditorViewportClient
{
public:
	FParticleSystemViewportShowFlags& GetParticleShowFlags() { return ParticleShowFlags; }
	const FParticleSystemViewportShowFlags& GetParticleShowFlags() const { return ParticleShowFlags; }

private:
	FParticleSystemViewportShowFlags ParticleShowFlags;
};
