#pragma once
#include "Game/GameCommon.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
class Camera;
class Entity;
class World;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class Game
{
public:
	Game();
	~Game();
	void                Startup();
	void                ShutDown();
	void                Update(float deltaSeconds);
	void                Render() const;
	
	void				RenderAttractMode() const;
	void				UpdateAttractMode();
	void				PrintControlsInAttractMode() const;
	
	void				SetScreenCamera();
	
	void				InitializeTerrainSpriteSheet();

	void				InitializeWorld();
	void				UpdateWorld(float deltaSeconds);
	void				RenderWorld() const;

	//Member variables
	Camera				m_screenCamera;
	bool                m_isInAttractMode = true;
	World*				m_world = nullptr;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------