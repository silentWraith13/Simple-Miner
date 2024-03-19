#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Game/App.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Window/Window.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Engine/Core/JobSystem.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
App*          g_theApp = nullptr;				
Renderer*     g_theRenderer = nullptr;
InputSystem*  g_theInput = nullptr;
AudioSystem*  g_theAudio = nullptr;
Window*		  g_theWindow = nullptr;
Game*		  g_theGame = nullptr;
JobSystem*	  g_theJobSystem = nullptr;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::Startup() 
{
	SetDevConsoleCamera();
	
	//constructs the engine components
	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);
	g_theEventSystem->SubscribeEventCallbackFunction("Quit", App::Event_Quit);

	RendererConfig rendererConfig;
	//rendererConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(rendererConfig);
	
	
	InputSystemConfig inputSystemConfig;
	g_theInput = new InputSystem(inputSystemConfig);

	WindowConfig  windowConfig;
	windowConfig.m_windowTitle = "SimpleMiner";
	windowConfig.m_clientAspect = 2.0f;
	windowConfig.m_inputSystem = g_theInput;
	g_theWindow = new Window(windowConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_renderer = g_theRenderer;
	devConsoleConfig.m_camera = &m_devConsoleCamera;
	g_theDevConsole = new DevConsole(devConsoleConfig);

	AudioSystemConfig audioSystemConfig;
	g_theAudio = new AudioSystem(audioSystemConfig);

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	DebugRenderSystemStartup(debugRenderConfig);

	JobSystemConfig jobSystemConfig;
	jobSystemConfig.m_numWorkerThreads = std::thread::hardware_concurrency();
	g_theJobSystem = new JobSystem(jobSystemConfig);

	//Calling startup of all engine components
	g_theEventSystem->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	g_theAudio->Startup();
	g_theJobSystem->Startup();
	m_clock.Reset();
	
	g_theGame = new Game();
	g_theGame->Startup();

	PrintDevConsoleCommands();
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::Shutdown()
{
	g_theGame->ShutDown();
	delete g_theGame;
	g_theGame = nullptr;

	g_theAudio->Shutdown();
	g_theInput->Shutdown();
	g_theDevConsole->Shutdown();
	g_theRenderer->Shutdown();
	g_theWindow->Shutdown();
	g_theEventSystem->Shutdown();
	g_theJobSystem->ShutDown();
	DebugRenderSystemShutdown();

	delete g_theAudio;
	g_theAudio = nullptr;
	
	delete g_theRenderer;
	g_theRenderer = nullptr;

	delete g_theWindow;
	g_theWindow = nullptr;

	delete g_theDevConsole;
	g_theDevConsole = nullptr;

	delete g_theInput;
	g_theInput = nullptr;
	
	delete g_theEventSystem;
	g_theEventSystem = nullptr;

	delete g_theJobSystem;
	g_theJobSystem = nullptr;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
App::App()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
App::~App()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::RunFrame()
{
	BeginFrame();
	Update();
	Render();
	EndFrame();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::Run()
{
	while (!IsQuitting() )
	{
		g_theApp->RunFrame();
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool App::HandleQuitRequested()
{
	m_isQuitting = true;
	return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool App::Event_Quit(EventArgs& args)
{
	(void)args;
	
	if (!g_theApp)
	{
		return false;
	}

	g_theApp->HandleQuitRequested();
	return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::BeginFrame()
{
	Clock::TickSytemClock();

	g_theDevConsole->BeginFrame();
	g_theInput->BeginFrame();
	g_theAudio->BeginFrame();
	g_theWindow->BeginFrame();
	g_theRenderer->BeginFrame();
	g_theJobSystem->BeginFrame();
	DebugRenderBeginFrame();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::Update()
{
	if (g_theGame != nullptr)
	{
		g_theGame->Update(m_clock.GetDeltaSeconds());
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		EventArgs args;
		FireEvent("Quit", args);
	}

	HWND windowHandle = GetActiveWindow();
	bool hasFocus = (windowHandle == g_theWindow->GetHwnd());

	if (g_theGame->m_isInAttractMode || g_theDevConsole->m_isOpen || !hasFocus)
	{
		g_theInput->SetCursorMode(false, false); 
	}
	else
	{
		g_theInput->SetCursorMode(true, true);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::Render() const
{
	Rgba8 clearColor{0,0,0,255};

	if(g_theGame)
	{
		g_theGame->Render();
	}

	AABB2 bounds(Vec2(0.f,0.f), Vec2(1000.f, 500.f));

	if (g_theDevConsole)
	{
		if (g_theDevConsole->m_isOpen)
		{
			g_theDevConsole->Render(bounds);
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::EndFrame()
{
	g_theRenderer->Endframe();
	g_theWindow->EndFrame();
	g_theAudio->EndFrame();
	g_theDevConsole->EndFrame();
	g_theInput->EndFrame();
	g_theJobSystem->EndFrame();
	DebugRenderEndFrame();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::SetDevConsoleCamera()
{
	m_devConsoleCamera.m_mode = Camera::eMode_Orthographic;
	m_devConsoleCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void App::PrintDevConsoleCommands()
{
	g_theDevConsole->AddLine(Rgba8(0, 255, 0, 255), "Game Keys:");
	g_theDevConsole->AddLine(Rgba8(0, 255, 0, 255), "Move-WASD");
	g_theDevConsole->AddLine(Rgba8(0, 255, 0, 255), "Pitch-UP/DOWN");
	g_theDevConsole->AddLine(Rgba8(0, 255, 0, 255), "Turn-LEFT/RIGHT");
	g_theDevConsole->AddLine(Rgba8(0, 255, 0, 255), "Quit-Escape");
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------

