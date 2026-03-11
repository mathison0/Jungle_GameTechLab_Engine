#pragma once
enum EGameState
{
	Title,
	Running,
	Ending,
	Clear
};

class IGameStateListener
{
public:
	virtual void OnGameStateChanged(EGameState) = 0;
	virtual ~IGameStateListener() {}

};