#define WIN32_LEAN_AND_MEAN		// Always #define this before #including <windows.h>
#include <windows.h>			// #include this (massive, platform-specific) header in very few places
#include <math.h>
#include <cassert>
#include <crtdbg.h>
#include "GameCommon.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Game/App.hpp"



//-----------------------------------------------------------------------------------------------
// #SD1ToDo: Move this useful macro to a more central place, e.g. Engine/Core/EngineCommon.hpp
//
//#define UNUSED(x) (void)(x);


//-----------------------------------------------------------------------------------------------
// #SD1ToDo: Move each of these items to its proper place, once that place is established
// 
bool g_isQuitting = false;							// ...becomes App::m_isQuitting
//HWND g_hWnd = nullptr;								// ...becomes WindowContext::m_windowHandle
//HDC g_displayDeviceContext = nullptr;				// ...becomes WindowContext::m_displayContext
//HGLRC g_openGLRenderingContext = nullptr;			// ...becomes Renderer::m_apiRenderingContext
//const char* APP_NAME = "SD1-A2: Starship Prototype";	// ...becomes ??? (Change this per project!)



//-----------------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
	UNUSED(commandLineString);
	UNUSED(applicationInstanceHandle);
	
	g_theApp = new App();
	g_theApp->Startup();
	g_theApp->Run();    ///May take time to return. will block in an infinite loop internally
	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}



