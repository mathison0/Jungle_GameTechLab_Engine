#pragma once

#include <vector>
#include "IGameStateListener.h"

class GameContext
{
private:
	std::vector<IGameStateListener*> listenerObjects;
	EGameState currentState;


public:
	inline void RegisterListenerObject(IGameStateListener* obj)
	{
		listenerObjects.push_back(obj);
	}

	void SetState(EGameState newState)
	{
		currentState = newState;
		for (auto* listener : listenerObjects)
		{
			listener->OnGameStateChanged(newState);
		}
	}

	inline EGameState GetState() { return currentState; }


	static GameContext& GetiNSTANCE()
	{
		static GameContext instance;
		return instance;
	}

	static float MaxHeight;
	static float MinHeight;
};
