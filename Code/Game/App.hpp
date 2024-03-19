#pragma once
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/EventSytem.hpp"
#include "Engine/Core/Clock.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
class Game;
class InputSystem;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class App
{
public:
	App();
	~App();
	void             Startup();
	void             Shutdown();
	void             RunFrame();
	void             Run();
	bool             IsQuitting() const { return m_isQuitting; }
	bool             HandleQuitRequested();
	static bool		 Event_Quit(EventArgs& args);


private:
	void            BeginFrame();
	void            Update();
	void            Render() const;
	void            EndFrame();
	void			SetDevConsoleCamera();
	void			PrintDevConsoleCommands();

public:
	bool            m_isQuitting = false;
	Camera          m_devConsoleCamera;
	Clock			m_clock;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------

