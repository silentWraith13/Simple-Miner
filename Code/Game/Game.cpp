#include "Game/Game.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Time.hpp"
#include "Game/App.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "Game/BlockDef.hpp"
#include "Game/BlockTemplate.hpp"
#include "Game/World.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
SpriteSheet* g_terrainSpriteSheet = nullptr;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Game::Game()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Game::~Game()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::Startup()
{
	InitializeTerrainSpriteSheet();
	SetScreenCamera();
	BlockDef::InitializeBlockDefs();
	BlockTemplate::InitializeBlockTemplateDefinitions();
	InitializeWorld();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::ShutDown()
{
	if (m_world != nullptr)
	{
		delete m_world;
		m_world = nullptr;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::Update(float deltaSeconds)
{
	if (m_isInAttractMode)
	{
		UpdateAttractMode();
	}
	if (!m_isInAttractMode)
	{
		UpdateWorld(deltaSeconds);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::Render() const
{
	g_theRenderer->BeginCamera(m_screenCamera);
	if (m_isInAttractMode)
	{
		RenderAttractMode();
		return;
	}
	g_theRenderer->EndCamera(m_screenCamera);

	RenderWorld();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::RenderAttractMode() const
{
	float timeNow = static_cast<float>(GetCurrentTimeSeconds());
	float radius = 60.f * sinf(timeNow * 10.3f);
	float thickness = 10.f * sinf(timeNow * 13.91f);

	Vec2 worldCenter(1600.f * 0.5f, 800.f * 0.5f);

	DebugDrawRing(worldCenter, radius, thickness, Rgba8(120, 100, 155, 255));

	PrintControlsInAttractMode();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::UpdateAttractMode()
{
	if (m_isInAttractMode == true && g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		g_theApp->HandleQuitRequested();
		return;
	}

	if (m_isInAttractMode == true && g_theInput->WasKeyJustPressed(KEYCODE_SPACE))
	{
		m_isInAttractMode = false;
		ShutDown();
		Startup();
		return;
	}

	if (m_isInAttractMode == false && g_theInput->WasKeyJustPressed(KEYCODE_ESC))
	{
		m_isInAttractMode = true;
		return;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::SetScreenCamera()
{
	m_screenCamera.m_mode = Camera::eMode_Orthographic;
	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::PrintControlsInAttractMode() const
{
	std::vector<Vertex_PCU> inputLineVerts;
	BitmapFont* textFont = nullptr;
	std::string fontName = "SquirrelFixedFont";
	textFont = g_theRenderer->CreateOrGetBitmapFontFromFile(("Data/Images/" + fontName).c_str());
	AABB2 bounds(Vec2(150.f, 150.f), Vec2(400.f, 400.f));
	std::string text = "Press Space to enter";
	textFont->AddVertsForTextInBox2D(inputLineVerts, bounds, 20.f, text, Rgba8(255, 0, 0, 255), 0.7f, Vec2(0.f, 1.f), TextDrawMode::OVERRUN, 9999);
	g_theRenderer->BindTexture(&textFont->GetTexture());
	g_theRenderer->DrawVertexArray((int)inputLineVerts.size(), inputLineVerts.data());
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::InitializeTerrainSpriteSheet()
{
	Texture* spriteSheetTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/BasicSprites_64x64.png");
	g_terrainSpriteSheet = new SpriteSheet(*spriteSheetTexture, TERRAIN_SPRITE_LAYOUT);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::InitializeWorld()
{
	m_world = new World();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::UpdateWorld(float deltaSeconds)
{
	if (m_world)
	{
		m_world->Update(deltaSeconds);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Game::RenderWorld() const
{
	if (m_world)
	{
		m_world->Render();
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------