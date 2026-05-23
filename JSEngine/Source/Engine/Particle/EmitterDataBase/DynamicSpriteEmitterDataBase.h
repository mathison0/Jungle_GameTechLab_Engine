#pragma once

struct FDynamicEmitterDataBase
{
    virtual ~FDynamicEmitterDataBase() = default;
	
};

struct FDynamicSpriteEitterDataBase : public FDynamicEmitterDataBase
{


};

/*

	데이터를 전달할 때 공용데이터는 어떻게 전달하고 
	instance data는 어떻게 전달할지 결정해야한다.

*/