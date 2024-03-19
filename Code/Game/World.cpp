#include "Game/World.hpp"
#include "Engine/Core/vertexUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Game/App.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/DebugRenderSystem.hpp"
#include "ThirdParty/Squirrel/SmoothNoise.hpp"
#include <cmath>
#include <sstream>  
#include <iomanip>  

//--------------------------------------------------------------------------------------------------------------------------------------------------------
struct MinecraftGameConstants
{
	Vec4 cameraWorldPos;
	float indoorLightColor[4];
	float outdoorLightColor[4];
	float skyColor[4];
	float fogStartDistance;
	float fogEndDistance;
	float fogMaxAlpha;
	float worldTime;
};

static const int k_mincecraftGameConstantsSlot = 5;

//--------------------------------------------------------------------------------------------------------------------------------------------------------
World::World()
{
	LoadGameConfig();
	InitializeShader();
	SetScreenCamera();
	SetWorldCamera();
	SetInitialCameraPosition();
	SetChunkConstantsValues();
	CreateConstantBufferForMinecraftConstants();

	g_theJobSystem->ClearCompletedJobs();
	g_theJobSystem->SetThreadJobType(0, DISK_JOB_TYPE);
	for (int jobThreadId = 1; jobThreadId < g_theJobSystem->GetNumThreads(); jobThreadId++)
	{
		g_theJobSystem->SetThreadJobType(jobThreadId, CHUNK_GENERATION_JOB_TYPE);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
World::~World()
{
	g_theJobSystem->ClearQueuedJobs();
	g_theJobSystem->WaitUntilCurrentJobsCompletion();
	g_theJobSystem->ClearCompletedJobs();

	std::vector<Chunk*> chunksToDeactivate;
	for (std::map<IntVec2, Chunk*>::iterator chunkIt = m_activeChunks.begin(); chunkIt != m_activeChunks.end(); chunkIt++)
	{
		Chunk* chunk = chunkIt->second;
		if (chunk->m_needsSaving)
		{
			QueueForSaving(chunk);
		}
		chunksToDeactivate.push_back(chunk);
	}

	for (Chunk* chunkToDeactivate : chunksToDeactivate)
	{
		m_activeChunks.erase(chunkToDeactivate->GetChunkCoordinates());
		delete chunkToDeactivate;
	}

	g_theJobSystem->WaitUntilQueuedJobsCompletion();
	g_theJobSystem->ClearCompletedJobs();

	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		Chunk* chunk = iter->second;
		if (chunk)
		{
			delete chunk;
			chunk = nullptr;
		}
	}
	m_activeChunks.clear();
	
	for (std::map<IntVec2, Chunk*>::iterator chunkIt = m_initializedChunks.begin(); chunkIt != m_initializedChunks.end(); chunkIt++)
	{
		Chunk*& chunk = chunkIt->second;
		if (chunk) 
		{
			delete chunk;
			chunk = nullptr;
		}
	}

	m_initializedChunks.clear();

	for (int jobThreadId = 0; jobThreadId < g_theJobSystem->GetNumThreads(); jobThreadId++) 
	{
		g_theJobSystem->SetThreadJobType(jobThreadId, DEFAULT_JOB_ID);
	}

	delete m_gameCBO;
	m_gameCBO = nullptr;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::Update(float deltaSeconds)
{
	bool isChunkActivated = ActivateNearestMissingChunk();

	if (!isChunkActivated)
	{
		DeactivateFurthestChunk();
	}
	CheckForCompletedJobs();
	CheckChunksForMeshUpdate();
	PerformRaycast();
	HandleInput();
	UpdateDayCycle(deltaSeconds);
	ProcessDirtyLighting();
	UpdateWorldCamera(deltaSeconds);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::Render()
{
	g_theRenderer->BeginCamera(m_worldCamera);
	
	g_theRenderer->ClearScreen(m_currentSkyColor);
	RenderChunk();
	RenderRaycast();
	AddBasisAtOrigin();
	AddCameraCompass();
	RenderDebugDrawChunk();
	DebugRenderWorld(m_worldCamera);
	g_theRenderer->EndCamera(m_worldCamera);
	
	g_theRenderer->BeginCamera(m_screenTextCamera);
	AddUI();
	g_theRenderer->EndCamera(m_screenTextCamera);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::SetScreenCamera()
{
	m_screenTextCamera.m_mode = Camera::eMode_Orthographic;
	m_screenTextCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(1600.f, 800.f));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::SetWorldCamera()
{
	m_worldCamera.m_mode = Camera::eMode_Perspective;
	m_worldCamera.SetPerspectiveView(g_theWindow->m_config.m_clientAspect, 60.0f, 0.1f, 1000.0f);
	Vec3 Ibasis(0.f, 0.f, 1.f);
	Vec3 JBasis(-1.f, 0.f, 0.f);
	Vec3 Kbasis(0.f, 1.f, 0.f);
	m_worldCamera.SetRenderBasis(Ibasis, JBasis, Kbasis);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::UpdateWorldCamera(float deltaSeconds)
{
	UpdateCameraKeyboardControls(deltaSeconds);
	m_worldCamera.SetTransform(m_camPosition, m_camOrientation);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::UpdateCameraKeyboardControls(float deltaSeconds)
{
	const float MOVE_SPEED = 4.f;
	const float TURN_RATE = 90.f;
	const float MOUSE_SPEED = 0.1f;

	m_velocity = Vec3(0.f, 0.f, 0.f);

	if (g_theInput->IsKeyDown('W'))
	{
		Vec3 forward = Vec3(1.f, 0.f, 0.f);
		Vec3 left = Vec3(0.f, 1.f, 0.f);
		Vec3 up = Vec3(0.f, 0.f, 0.f);
		m_camOrientation.GetAsVectors_XFwd_YLeft_ZUp(forward, left, up);
		forward.z = 0.f;
		forward.Normalize();
		m_velocity += forward * MOVE_SPEED;
	}

	if (g_theInput->IsKeyDown('A'))
	{
		Vec3 forward = Vec3(1.f, 0.f, 0.f);
		Vec3 left = Vec3(0.f, 1.f, 0.f);
		Vec3 up = Vec3(0.f, 0.f, 0.f);
		m_camOrientation.GetAsVectors_XFwd_YLeft_ZUp(forward, left, up);
		left.z = 0.f;
		left.Normalize();
		m_velocity += left * MOVE_SPEED;
	}

	if (g_theInput->IsKeyDown('S'))
	{
		Vec3 forward = Vec3(1.f, 0.f, 0.f);
		Vec3 left = Vec3(0.f, 1.f, 0.f);
		Vec3 up = Vec3(0.f, 0.f, 0.f);
		m_camOrientation.GetAsVectors_XFwd_YLeft_ZUp(forward, left, up);
		forward.z = 0.f;
		forward.Normalize();
		m_velocity -= forward * MOVE_SPEED;
	}

	if (g_theInput->IsKeyDown('D'))
	{
		Vec3 forward = Vec3(1.f, 0.f, 0.f);
		Vec3 left = Vec3(0.f, 1.f, 0.f);
		Vec3 up = Vec3(0.f, 0.f, 0.f);
		m_camOrientation.GetAsVectors_XFwd_YLeft_ZUp(forward, left, up);
		left.z = 0.f;
		left.Normalize();
		m_velocity -= left * MOVE_SPEED;
	}


	if (g_theInput->WasKeyJustPressed('H'))
	{
		m_camPosition = Vec3(0.f, 0.f, 0.f);
		m_camOrientation = EulerAngles(0.f, 0.f, 0.f);
	}

	if (g_theInput->IsKeyDown('Q'))
	{
		Vec3 up = Vec3(0.f, 0.f, 1.f);
		m_velocity += up * MOVE_SPEED;
	}

	if (g_theInput->IsKeyDown('E'))
	{
		Vec3 down = Vec3(0.f, 0.f, -1.f);
		m_velocity += down * MOVE_SPEED;
	}
	
	if (g_theInput->IsKeyDown(KEYCODE_SPACE))
	{
		m_velocity *= 10.0f;
	}

	Vec2 cursorDelta = g_theInput->GetCursorClientDelta();
	m_camOrientation.m_yawDegrees -= cursorDelta.x * MOUSE_SPEED;
	m_camOrientation.m_pitchDegrees += cursorDelta.y * MOUSE_SPEED;

	m_camOrientation.m_pitchDegrees = GetClamped(m_camOrientation.m_pitchDegrees, -85.f, 85.f);
	m_camOrientation.m_rollDegrees = GetClamped(m_camOrientation.m_rollDegrees, -45.f, 45.f);

	m_camPosition += m_velocity * deltaSeconds;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::AddCameraCompass()
{
	std::vector<Vertex_PCU> verts;
	float thickness = 0.0003f;
	float len = 0.01f;
	float compassDist = 0.2f;
	Vec3 worldEast(1.0f, 0.0f, 0.0f);
	Vec3 worldNorth(0.0f, 1.0f, 0.0f);
	Vec3 worldUp(0.0f, 0.0f, 1.0f);
	Vec3 camFwd = m_worldCamera.m_orientation.GetAsMatrix_XFwd_YLeft_ZUp().GetIBasis3D();
	Vec3 compassCenter = m_worldCamera.m_position + (camFwd * compassDist);

	AddVertsForLineSegment3D(verts, compassCenter, compassCenter + (worldEast * len), thickness, Rgba8(255, 0, 0));
 	AddVertsForLineSegment3D(verts, compassCenter, compassCenter + (worldNorth * len), thickness, Rgba8(0, 255, 0));
 	AddVertsForLineSegment3D(verts, compassCenter, compassCenter + (worldUp * len), thickness, Rgba8(0, 0, 255));

	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::AddBasisAtOrigin() const
{
	float arrowLength = 1.0f;
	float arrowRadius = 0.01f;

	Vec3 xEnd = Vec3(arrowLength, 0.0f, 0.0f);
	Vec3 yEnd = Vec3(0.0f, arrowLength, 0.0f);
	Vec3 zEnd = Vec3(0.0f, 0.0f, arrowLength);

	Rgba8 xAxisColor = Rgba8(255, 0, 0);
	Rgba8 yAxisColor = Rgba8(0, 255, 0);
	Rgba8 zAxisColor = Rgba8(0, 0, 255);

	std::vector<Vertex_PCU> verts;
	AddVertsForLineSegment3D(verts, Vec3(0.f, 0.f, 0.f), xEnd, arrowRadius, xAxisColor);
	AddVertsForLineSegment3D(verts, Vec3(0.f, 0.f, 0.f), yEnd, arrowRadius, yAxisColor);
	AddVertsForLineSegment3D(verts, Vec3(0.f, 0.f, 0.f), zEnd, arrowRadius, zAxisColor);

	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::AddUI()
{
	std::vector<Vertex_PCU> textVerts;
	BitmapFont* textFont = nullptr;
	std::string fontName = "SquirrelFixedFont";
	textFont = g_theRenderer->CreateOrGetBitmapFontFromFile(("Data/Images/" + fontName).c_str());
	
	std::string infoLine1 = Stringf("WASD = horizontal, QE = vertical, Space = fast, F1 = debug, F8 = regenerate \n");
	AABB2 bounds(Vec2(15.f, 790.f), Vec2(1500.f, 790.f));
	textFont->AddVertsForTextInBox2D(textVerts, bounds, 20.f, infoLine1, Rgba8(255, 255, 0, 255), 0.8f, Vec2(0.f, 1.f), TextDrawMode::OVERRUN, 9999);
	
	int numChunks = (int)m_activeChunks.size();
	int numChunkVerts = 0;
	int numBlocks = 0;
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		numChunkVerts += iter->second->GetChunkMeshVertices();
		numBlocks += iter->second->GetNumBlocks();
	}

	std::string infoLine2 = Stringf("chunks=%i / %i, Blocks=%i, vertices=%i, xyz=(%i,%i,%i), ypr=(%i,%i,%i), frameMS=%i (%i FPS) \n"
	, numChunks, m_maxChunks, numBlocks, (int)numChunkVerts, (int)m_camPosition.x, (int)m_camPosition.y,
	(int)m_camPosition.z, (int)m_camOrientation.m_yawDegrees, (int)m_camOrientation.m_pitchDegrees, (int)m_camOrientation.m_rollDegrees
	, (int)(g_theApp->m_clock.GetDeltaSeconds() * 1000.f), (int)(1.f / g_theApp->m_clock.GetDeltaSeconds()));
	

	std::string blockName = BlockDef::s_blockDefs[m_blockTypeToAdd].m_name;
	std::string infoLine3 = "BlockType = ";
	infoLine3.append(blockName);
	infoLine3 += ",   Time = " + GetWorldTimeAsHoursAndMinutes();

	AABB2 bounds2(Vec2(15.f, 770.f), Vec2(1500.f, 770.f));
	textFont->AddVertsForTextInBox2D(textVerts, bounds2, 18.f, infoLine2, Rgba8(0, 255, 255, 255), 0.8f, Vec2(0.f, 1.f), TextDrawMode::OVERRUN, 9999);

	AABB2 bounds3(Vec2(15.f, 750.f), Vec2(1500.f, 750.f));
	textFont->AddVertsForTextInBox2D(textVerts, bounds3, 18.f, infoLine3, Rgba8(0, 255, 255, 255), 0.8f, Vec2(0.f, 1.f), TextDrawMode::OVERRUN, 9999);

	g_theRenderer->BindTexture(&textFont->GetTexture());
	g_theRenderer->DrawVertexArray((int)textVerts.size(), textVerts.data());
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::RandomizeChunks()
{
	DeactivateAllChunks();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::ActivateNewChunk(const IntVec2& chunkCoords)
{
	if (m_activeChunks.count(chunkCoords) > 0)
		return;

	m_initiliazedChunksMutex.lock();
	auto chunkIt = m_initializedChunks.find(chunkCoords);
	if (chunkIt == m_initializedChunks.end())
	{
		m_initiliazedChunksMutex.unlock();
		return; // chunk was not found in the initialized chunks list
	}

	Chunk* chunk = chunkIt->second;
	m_initializedChunks.erase(chunkIt);
	m_initiliazedChunksMutex.unlock();

	chunk->m_status = ChunkState::ACTIVE;
	m_activeChunks[chunkCoords] = chunk;

	LinkChunkToNeighbors(chunk);

	chunk->InitializeLighting();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::DeactivateFurthestChunk()
{
	if (m_activeChunks.empty())
		return;

	IntVec2 playerChunkCoords = GetChunkCoordinatesForWorldPosition(m_camPosition);
	float furthestDistanceSquared = 0.f;
	Vec2 playerPos(m_camPosition.x, m_camPosition.y);

	std::map<IntVec2, Chunk*>::iterator furthestChunkIt;

	for (std::map<IntVec2, Chunk*>::iterator it = m_activeChunks.begin(); it != m_activeChunks.end(); ++it)
	{
		Chunk* chunk = it->second;
		IntVec2 currentChunkCoords = it->first;
		
		if (chunk)
		{
			Vec2 chunkCenterXY = Chunk::GetChunkCenterXYForChunkCoords(currentChunkCoords);
			float distanceSquared = GetDistanceSquared2D(Vec2(playerPos.x, playerPos.y), Vec2(chunkCenterXY.x, chunkCenterXY.y));

			if (distanceSquared >= (m_chunkDeactivationRange * m_chunkDeactivationRange) && (distanceSquared > furthestDistanceSquared) )
			{
				furthestDistanceSquared = distanceSquared;
				furthestChunkIt = it;
			}
		}
	}

	if (furthestDistanceSquared != 0.f)
	{
		Chunk* chunkToDeactivate = furthestChunkIt->second;
		chunkToDeactivate->DisconnectFromNeighbors();
		m_activeChunks.erase(furthestChunkIt);
		delete chunkToDeactivate;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::AddDebugDrawVertsForAllChunks(std::vector<Vertex_PCU>& verts)
{
	if (m_activeChunks.empty())
	{
		return; 
	}
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		Chunk* chunk = iter->second;
		
		AABB3 bounds = chunk->GetWorldBounds();
		AddVertsForAABB3D(verts, bounds);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::RenderDebugDrawChunk()
{
	if (m_debugDrawChunk)
	{
		m_debugDrawChunkVerts.clear();
		AddDebugDrawVertsForAllChunks(m_debugDrawChunkVerts);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->BindShader(nullptr);
		g_theRenderer->SetRasterizerModes(RasterizerMode::WIREFRAME_CULL_NONE);
		g_theRenderer->DrawVertexArray((int)m_debugDrawChunkVerts.size(), m_debugDrawChunkVerts.data());
		g_theRenderer->SetRasterizerModes(RasterizerMode::SOLID_CULL_BACK);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::ToggleDebugDrawChunk()
{
	m_debugDrawChunk = !m_debugDrawChunk;
	m_debugDrawChunkVerts.clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::HandleInput()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
	{
		ToggleDebugDrawChunk();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
	{
		OnDiggingOfBlock();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
	{
		OnPlacingOfBlock(static_cast<BlockDefID>(m_blockTypeToAdd));
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F8))
	{
		RandomizeChunks();
	}

	if (g_theInput->WasKeyJustPressed('R'))
	{
		m_freezeRaycast = !m_freezeRaycast;
	}

	if (g_theInput->WasKeyJustPressed('H'))
	{
		m_placeBlockAtCurrentPos = !m_placeBlockAtCurrentPos;
	}

	if (g_theInput->WasKeyJustPressed('1'))
		m_blockTypeToAdd = 1;
	if (g_theInput->WasKeyJustPressed('2'))
		m_blockTypeToAdd = 2;
	if (g_theInput->WasKeyJustPressed('3'))
		m_blockTypeToAdd = 3;
	if (g_theInput->WasKeyJustPressed('4'))
		m_blockTypeToAdd = 4;
	if (g_theInput->WasKeyJustPressed('5'))
		m_blockTypeToAdd = 5;
	if (g_theInput->WasKeyJustPressed('6'))
		m_blockTypeToAdd = 6;
	if (g_theInput->WasKeyJustPressed('7'))
		m_blockTypeToAdd = 7;
	if (g_theInput->WasKeyJustPressed('8'))
		m_blockTypeToAdd = 8;
	if (g_theInput->WasKeyJustPressed('9'))
		m_blockTypeToAdd = 9;

	if (g_theInput->WasKeyJustPressed('U'))
	{
		m_debugDisableWorldShader = !m_debugDisableWorldShader;
	}

	if (g_theInput->WasKeyJustPressed('I'))
	{
		m_debugUseWhiteBlocks = !m_debugUseWhiteBlocks;
	}

	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_debugDisableFog = !m_debugDisableFog;
	}

	if (g_theInput->WasKeyJustPressed('P'))
	{
		m_debugDisableGlowstone = !m_debugDisableGlowstone;
	}

	if (g_theInput->WasKeyJustPressed('K'))
	{
		m_debugDisableSky = !m_debugDisableSky;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::LoadGameConfig()
{
	std::string configPath = "Data/GameConfig.xml";
	XmlDocument configXml;
	configXml.LoadFile(configPath.c_str());
	XmlElement* rootElement = configXml.RootElement();
	m_chunkActivationRange =		ParseXmlAttribute(*rootElement, "chunkActivationDistance",   m_chunkActivationRange);
 	m_autoCreateChunks =			ParseXmlAttribute(*rootElement, "autoCreateChunks",		     m_autoCreateChunks);
 	m_debugDisableHSR =				ParseXmlAttribute(*rootElement, "debugDisableHSR",		     m_debugDisableHSR);
	m_indoorLightColor =			ParseXmlAttribute(*rootElement, "indoorLightColor",		     Rgba8::WHITE);
	m_dayOutdoorLightColor =		ParseXmlAttribute(*rootElement, "dayOutdoorLightColor",      Rgba8::WHITE);
	m_nightOutdoorLightColor =		ParseXmlAttribute(*rootElement, "nightOutdoorLightColor",    Rgba8::WHITE);
	m_daySkyColor =					ParseXmlAttribute(*rootElement, "daySkyColor",			     Rgba8::WHITE);
	m_nightSkyColor =				ParseXmlAttribute(*rootElement, "nightSkyColor",			 Rgba8::WHITE);
	m_worldTimeScale =				ParseXmlAttribute(*rootElement, "worldTimeScale",		     m_worldTimeScale);
	m_fogStartDistance =			ParseXmlAttribute(*rootElement, "fogStart",				     m_fogStartDistance);
	m_fogEndDistance =				ParseXmlAttribute(*rootElement, "fogEnd",				     m_fogEndDistance);
	m_fogMaxAlpha =					ParseXmlAttribute(*rootElement, "fogMaxAlpha",			     m_fogMaxAlpha);
	m_debugUseWhiteBlocks =			ParseXmlAttribute(*rootElement, "debugUseWhiteBlocks",       m_debugUseWhiteBlocks);
	m_debugStepLightPropagation =   ParseXmlAttribute(*rootElement, "debugStepLightPropagation", m_debugStepLightPropagation);
	m_debugDisableWorldShader =     ParseXmlAttribute(*rootElement, "debugStepLightPropagation", m_debugDisableWorldShader);
	m_worldSeed =					ParseXmlAttribute(*rootElement, "worldSeed",				 m_worldSeed);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::InitializeShader()
{
 	std::string shaderName = "Data/Shaders/World";

	// Create Shader
	if (!shaderName.empty())
	{
		m_shader = g_theRenderer->CreateShaderOrGetFromFile((shaderName).c_str());

		if (m_shader == nullptr)
		{
			ERROR_AND_DIE("Failed to create shader");
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int World::GetWorldSeed() const
{
	return m_worldSeed;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::RenderChunk()
{
	CopyDataToCBOAndBindIt();
	if (!m_debugDisableWorldShader)
	{
		g_theRenderer->BindShader(m_shader);
	}

	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		Chunk* chunk = iter->second;
		if (chunk)
		{
			chunk->Render();
		}
	}
	Shader* defaultShader = g_theRenderer->CreateShaderOrGetFromFile("Default");
	g_theRenderer->BindShader(defaultShader);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::CheckChunksForMeshUpdate()
{	
	Chunk* nearestChunks[10] = {};
	float nearestDistances[10] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

	for (std::map<IntVec2, Chunk*>::const_iterator chunkIt = m_activeChunks.begin(); chunkIt != m_activeChunks.end(); chunkIt++) {
		Chunk* chunk = chunkIt->second;
		if (chunk) {

			if (!chunk->m_isChunkDirty || !chunk->HasAllValidNeighbours()) continue;

			Vec3 chunkCenter = chunk->GetChunkCenter();

			float distToCamera = GetDistanceSquared3D(m_camPosition, chunkCenter);

			for (int distIndex = 0; distIndex < 10; distIndex++) {
				float& nearestDistance = nearestDistances[distIndex];
				if (distToCamera < nearestDistance) {
					nearestChunks[distIndex] = chunk;
					nearestDistance = distToCamera;
					break;
				}
			}
		}
	}

	for (int distIndex = 0; distIndex < 10; distIndex++) {
		Chunk* chunk = nearestChunks[distIndex];
		if (!chunk) continue;

		chunk->Update();
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::OnDiggingOfBlock()
{
	if (m_raycastResult.m_didImpact)
	{
		BlockIterator blockToDig = m_raycastResult.m_blockImpacted;

		Chunk* chunk = blockToDig.m_chunk;
		IntVec3 localPos = chunk->GetLocalCoordsFromBlockIndex(blockToDig.m_blockIndex);
		BlockDefID air = BlockDef::GetBlockDefIDByName("air");

		chunk->SetBlockType(localPos.x, localPos.y, localPos.z, air);
		chunk->DigBlock(blockToDig);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::OnPlacingOfBlock(BlockDefID block)
{
	if (m_placeBlockAtCurrentPos)
	{
		IntVec3 highestNonAirBlock = GetHighestNonAirBlock();
		IntVec3 newBlockCoords = highestNonAirBlock + IntVec3(0, 0, 1);
		if (highestNonAirBlock.x != -1)
		{
			Chunk* chunk = GetChunkForWorldPosition(m_camPosition);
			chunk->SetBlockType(newBlockCoords.x, newBlockCoords.y, newBlockCoords.z, block);
			int blockIndex = chunk->GetBlockIndex(newBlockCoords.x, newBlockCoords.y, newBlockCoords.z);
			MarkLightingDirty(BlockIterator(chunk, blockIndex));
		}
	}
	else if (m_raycastResult.m_didImpact)
	{
		BlockIterator blockToAdd;
		if (m_raycastResult.m_impactNormal.x > 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetEastNeighbour();
		else if (m_raycastResult.m_impactNormal.x < 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetWestNeighbour();
		else if (m_raycastResult.m_impactNormal.y > 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetNorthNeighbour();
		else if (m_raycastResult.m_impactNormal.y < 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetSouthNeighbour();
		else if (m_raycastResult.m_impactNormal.z > 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetAboveNeighbour();
		else if (m_raycastResult.m_impactNormal.z < 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetBelowNeighbour();

		blockToAdd.m_chunk->PlaceBlock(blockToAdd);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec3 World::GetHighestNonAirBlock()
{
	IntVec2 chunkCoords = Chunk::GetChunkCoordinatesForWorldPosition(m_camPosition);
	Chunk* chunk = GetChunkForChunkCoordinates(chunkCoords);

	if (!chunk)
	{
		return IntVec3(-1, -1, -1);  
	}

	Vec3 localPos = m_camPosition - chunk->GetWorldBounds().m_mins;
	int localX = (int)localPos.x;
	int localY = (int)localPos.y;
	int localZ = chunk->GetHighestZNonAirBlock(localX, localY);

	if (localZ == -1)  
	{
		return IntVec3(-1, -1, -1);
	}

	return IntVec3(localX, localY, localZ);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Chunk* World::GetChunkForWorldPosition(const Vec3& position)
{
	int x = RoundDownToInt(position.x);
	int y = RoundDownToInt(position.y);
	int chunkCoordsX = x >> CHUNK_BITS_X;
	int chunkCoordsY = y >> CHUNK_BITS_Y;

	auto it = m_activeChunks.find(IntVec2(chunkCoordsX, chunkCoordsY));
	if (it != m_activeChunks.end())
	{
		return it->second;
	}

	return nullptr;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Chunk* World::GetChunkForChunkCoordinates(const IntVec2& chunkCoords)
{
	auto it = m_activeChunks.find(chunkCoords);
	if (it != m_activeChunks.end())
	{
		return it->second;
	}

	return nullptr;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec2 World::GetChunkCoordinatesForWorldPosition(Vec3 const& pos) const
{
	int globalX = RoundDownToInt(pos.x);
	int globalY = RoundDownToInt(pos.y);
	int chunkCoordsX = globalX >> CHUNK_BITS_X;
	int chunkCoordsY = globalY >> CHUNK_BITS_Y;
	return IntVec2(chunkCoordsX, chunkCoordsY);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool World::ActivateNearestMissingChunk()
{
// 	if (m_activeChunks.size() >= m_maxChunks)
// 		return false;

	bool foundActivationCandidate = false;
	IntVec2 playerChunkCoords = GetChunkCoordinatesForWorldPosition(m_camPosition);
	IntVec2 neighborhoodMinChunkCoords = playerChunkCoords - IntVec2(m_maxChunkRadiusX, m_maxChunkRadiusY);
	IntVec2 neighborhoodMaxChunkCoords = playerChunkCoords + IntVec2(m_maxChunkRadiusX, m_maxChunkRadiusY);
	Vec2 playerWorldPos(m_camPosition.x, m_camPosition.y);
	float closestMissingChunkDist = m_chunkActivationRange;
	IntVec2 closestMissingChunkCoords = playerChunkCoords;

	for (int chunkY = neighborhoodMinChunkCoords.y; chunkY <= neighborhoodMaxChunkCoords.y; chunkY++)
	{
		for (int chunkX = neighborhoodMinChunkCoords.x; chunkX <= neighborhoodMaxChunkCoords.x; chunkX++)
		{
			IntVec2 chunkCoords(chunkX, chunkY);
			Vec2 chunkCenterWorldPos = Chunk::GetChunkCenterXYForChunkCoords(chunkCoords);
			float distFromPlayer = GetDistance2D(chunkCenterWorldPos, playerWorldPos);
			if (distFromPlayer < closestMissingChunkDist)
			{
				std::map<IntVec2, Chunk*>::const_iterator chunksQueuedForGenerationIt = m_initializedChunks.find(chunkCoords);
				std::map<IntVec2, Chunk*>::const_iterator chunkActiveIt = m_activeChunks.find(chunkCoords);
				if ((chunksQueuedForGenerationIt == m_initializedChunks.end()) && (chunkActiveIt == m_activeChunks.end()))
				{
					foundActivationCandidate = true;
					closestMissingChunkDist = distFromPlayer;
					closestMissingChunkCoords = chunkCoords;
				}
			}
		}
	}

	if (closestMissingChunkDist < m_chunkActivationRange)
	{
		InitializeChunk(closestMissingChunkCoords);
		//ActivateNewChunk(closestMissingChunkCoords);
		return true;
	}
	
	return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool World::DoesChunkExist(const IntVec2& chunkCoords)
{
	return m_activeChunks.find(chunkCoords) != m_activeChunks.end();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::DeactivateAllChunks()
{
	std::vector<Chunk*> chunksToDeactivate;
	for (auto it = m_activeChunks.begin(); it != m_activeChunks.end(); ++it)
	{
		Chunk* chunkToDeactivate = it->second;
		chunkToDeactivate->DisconnectFromNeighbors();
		chunksToDeactivate.push_back(chunkToDeactivate);
	}

	for (Chunk* chunkToDeactivate : chunksToDeactivate)
	{
		m_activeChunks.erase(chunkToDeactivate->GetChunkCoordinates());
		delete chunkToDeactivate;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::DeactivateChunk(Chunk* chunk)
{
	if (!chunk) return;
	auto chunkIt = m_activeChunks.find(chunk->GetChunkCoordinates());

	if (chunkIt != m_activeChunks.end()) 
	{
		m_activeChunks.erase(chunk->GetChunkCoordinates());
	}

	UnlinkChunkFromNeighbors(chunk);

	delete chunk;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::ProcessDirtyLighting()
{
	while (!m_dirtyLightBlocks.empty())
	{
		BlockIterator front = m_dirtyLightBlocks.front();
		m_dirtyLightBlocks.pop_front();
		Block* block = front.GetBlock();
		
		if (block)
		{
			block->SetIsBlockLightDirty(false);
			uint8_t currentIndoorLightInfluence = block->GetIndoorLightInfluence();
			uint8_t computedIndoorLightInfluence = ComputeIndoorLightInfluence(front);
			uint8_t currentOutdoorLightInfluence = block->GetOutdoorLightInfluence();
			uint8_t computedOutdoorLightInfluence = ComputeOutdoorLightInfluence(front);
			if ((currentOutdoorLightInfluence != computedOutdoorLightInfluence) || (currentIndoorLightInfluence != computedIndoorLightInfluence))
			{
				block->SetOutdoorLightInfluence(computedOutdoorLightInfluence);
				block->SetIndoorLightInfluence(computedIndoorLightInfluence);
				front.m_chunk->SetChunkToDirty();
				MarkNeighbouringChunksAndBlocksAsDirty(front);
			}
		}
		
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::MarkLightingDirty(const BlockIterator& blockIter)
{
	Block& block = *(blockIter.m_chunk->GetBlock(blockIter.m_blockIndex));
	if (block.IsBlockLightDirty())
		return;

	block.SetIsBlockLightDirty(true);
	m_dirtyLightBlocks.push_back(blockIter);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::MarkLightingDirtyIfNotOpaque(const BlockIterator& blockIter)
{
	Block& block = *(blockIter.m_chunk->GetBlock(blockIter.m_blockIndex));
	if (!BlockDef::IsBlockTypeOpaque(block.GetTypeID()))
	{
		MarkLightingDirty(blockIter);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::MarkLightingDirtyIfNotSkyAndNotOpaque(const BlockIterator& blockIter)
{
	if (blockIter.m_chunk)
	{
		Block& block = *(blockIter.m_chunk->GetBlock(blockIter.m_blockIndex));
		if (!block.IsBlockSky())
		{
			MarkLightingDirtyIfNotOpaque(blockIter);
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::MarkNeighbouringChunksAndBlocksAsDirty(const BlockIterator& blockIter)
{
	BlockIterator neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunk != nullptr)
	{
		neighbour.m_chunk->SetChunkToDirty();
		if (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			MarkLightingDirty(neighbour);
		}
	}

	neighbour = blockIter.GetEastNeighbour();
	if (neighbour.m_chunk != nullptr)
	{
		neighbour.m_chunk->SetChunkToDirty();
		if (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			MarkLightingDirty(neighbour);
		}
	}

	neighbour = blockIter.GetSouthNeighbour();
	if (neighbour.m_chunk != nullptr)
	{
		neighbour.m_chunk->SetChunkToDirty();
		if (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			MarkLightingDirty(neighbour);			
		}
	}

	neighbour = blockIter.GetWestNeighbour();
	if (neighbour.m_chunk != nullptr)
	{
		neighbour.m_chunk->SetChunkToDirty();
		if (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			MarkLightingDirty(neighbour);
		}
	}

	neighbour = blockIter.GetAboveNeighbour();
	if (neighbour.m_chunk != nullptr)
	{
		neighbour.m_chunk->SetChunkToDirty();
		if (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			MarkLightingDirty(neighbour);
		}
	}

	neighbour = blockIter.GetBelowNeighbour();
	if (neighbour.m_chunk != nullptr)
	{
		neighbour.m_chunk->SetChunkToDirty();
		if (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			MarkLightingDirty(neighbour);
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t World::ComputeIndoorLightInfluence(const BlockIterator& blockIter) const
{
	uint8_t maxComputedLightInfluence = 0;
	uint8_t computedLightInfluence = 0;
	Block& block = *blockIter.GetBlock();
	if (BlockDef::DoesBlockTypeEmitLight(block.GetTypeID()))
	{
		computedLightInfluence = BlockDef::s_blockDefs[block.GetTypeID()].m_indoorLightInfluence;
		if (computedLightInfluence > maxComputedLightInfluence)
		{
			maxComputedLightInfluence = computedLightInfluence;
		}
	}
	if (!BlockDef::IsBlockTypeOpaque(block.GetTypeID()))
	{
		computedLightInfluence = GetHighestIndoorLightInfluenceAmongNeighbours(blockIter);
		if (computedLightInfluence > 0)
		{
			computedLightInfluence--;
		}
		if (computedLightInfluence > maxComputedLightInfluence)
		{
			maxComputedLightInfluence = computedLightInfluence;
		}
	}

	return maxComputedLightInfluence;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t World::ComputeOutdoorLightInfluence(const BlockIterator& blockIter) const
{
	uint8_t maxComputedLightInfluence = 0;
	uint8_t computedLightInfluence = 0;
	Block& block = *blockIter.GetBlock();
	if (block.IsBlockSky())
	{
		computedLightInfluence = 15;
		if (computedLightInfluence > maxComputedLightInfluence)
		{
			maxComputedLightInfluence = computedLightInfluence;
		}
	}
	if (!BlockDef::IsBlockTypeOpaque(block.GetTypeID()))
	{
		computedLightInfluence = GetHighestOutdoorLightInfluenceAmongNeighbours(blockIter);
		if (computedLightInfluence > 0)
		{
			computedLightInfluence--;
		}
		if (computedLightInfluence > maxComputedLightInfluence)
		{
			maxComputedLightInfluence = computedLightInfluence;
		}
	}

	return maxComputedLightInfluence;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t World::GetHighestOutdoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const
{
	uint8_t maxOutdoorLightInfluence = 0;
	uint8_t outdoorLightInfluence = 0;
	BlockIterator neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	neighbour = blockIter.GetEastNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	neighbour = blockIter.GetSouthNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	neighbour = blockIter.GetWestNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	neighbour = blockIter.GetAboveNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	neighbour = blockIter.GetBelowNeighbour();
	if (neighbour.m_chunk)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
		{
			maxOutdoorLightInfluence = outdoorLightInfluence;
		}
	}

	return maxOutdoorLightInfluence;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t World::GetHighestIndoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const
{
	uint8_t maxIndoorLightInfluence = 0;
	uint8_t indoorLightInfluence = 0;
	BlockIterator neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	neighbour = blockIter.GetEastNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	neighbour = blockIter.GetSouthNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	neighbour = blockIter.GetWestNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	neighbour = blockIter.GetAboveNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	neighbour = blockIter.GetBelowNeighbour();
	if (neighbour.m_chunk)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
		{
			maxIndoorLightInfluence = indoorLightInfluence;
		}
	}

	return maxIndoorLightInfluence;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::CopyDataToCBOAndBindIt() const
{
	MinecraftGameConstants gameConstants;
	Vec3 camPos = m_camPosition;
	gameConstants.cameraWorldPos = Vec4(camPos.x, camPos.y, camPos.z, 0.f);
	(m_indoorLightColor * m_glowStrength).GetAsFloats(gameConstants.indoorLightColor);
	m_currentOutdoorLightColor.GetAsFloats(gameConstants.outdoorLightColor);
	m_currentSkyColor.GetAsFloats(gameConstants.skyColor);
	gameConstants.fogStartDistance = m_fogStartDistance;
	gameConstants.fogEndDistance = m_fogEndDistance;
	gameConstants.fogMaxAlpha = m_fogMaxAlpha;
	gameConstants.worldTime = m_worldTime;

	g_theRenderer->CopyCPUtoGPU(&gameConstants, sizeof(MinecraftGameConstants), m_gameCBO);
	g_theRenderer->BindConstantBuffer(k_mincecraftGameConstantsSlot, m_gameCBO);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::UpdateDayCycle(float deltaSeconds)
{
	if (g_theInput->IsKeyDown('Y'))
	{
		m_currentWorldTimeScaleAccelerationFactor = 10000.0f / m_worldTimeScale; // acceleration factor when Y is pressed (50x faster)
	}
	else
	{
		m_currentWorldTimeScaleAccelerationFactor = 1.0f;
	}

	m_worldTime += (deltaSeconds * m_worldTimeScale * m_currentWorldTimeScaleAccelerationFactor) / (60.f * 60.f * 24.f);

	if (!m_debugDisableGlowstone)
	{
		float glowPerlin = Compute1dPerlinNoise(m_worldTime * 1000.f, 2.f, 5);
		m_glowStrength = RangeMapClamped(glowPerlin, -1.f, 1.f, 0.8f, 1.f);
	}

	if (!m_debugDisableFog)
	{
		float lightningPerlin = Compute1dPerlinNoise(m_worldTime * 1000.f, 2.f, 9);
		m_lightingStrength = RangeMapClamped(lightningPerlin, 0.6f, 0.9f, 0.f, 1.f);
	}

	float fractionPartOfTime = m_worldTime - static_cast<float>(RoundDownToInt(m_worldTime));
	UpdateSkyAndLightColors(fractionPartOfTime);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::UpdateSkyAndLightColors(float fractionPartOfTime)
{
	if (!m_debugDisableSky)
	{
		if (fractionPartOfTime < 0.25f || fractionPartOfTime > 0.75f)
		{
			m_currentSkyColor = m_nightSkyColor;
			m_currentOutdoorLightColor = m_nightOutdoorLightColor;
		}
		else
		{
			Rgba8 startSkyColor, targetSkyColor, startOutdoorLight, targetOutdoorLight;
			float input = 0.f;

			if (fractionPartOfTime < 0.5f)
			{
				startSkyColor = m_nightSkyColor;
				targetSkyColor = m_daySkyColor;
				startOutdoorLight = m_nightOutdoorLightColor;
				targetOutdoorLight = m_dayOutdoorLightColor;
				input = RangeMap(fractionPartOfTime, 0.25f, 0.5f, 0.f, 1.f);
			}
			else
			{
				startSkyColor = m_daySkyColor;
				targetSkyColor = m_nightSkyColor;
				startOutdoorLight = m_dayOutdoorLightColor;
				targetOutdoorLight = m_nightOutdoorLightColor;
				input = RangeMap(fractionPartOfTime, 0.5f, 0.75f, 0.f, 1.f);
			}

			m_currentSkyColor = Interpolate(startSkyColor, targetSkyColor, input);
			m_currentSkyColor = Interpolate(m_currentSkyColor, Rgba8::WHITE, m_lightingStrength);
			m_currentOutdoorLightColor = Interpolate(startOutdoorLight, targetOutdoorLight, input);
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
std::string World::GetWorldTimeAsHoursAndMinutes() const
{
	float fractionPartOfTime = m_worldTime - static_cast<int>(m_worldTime);  // get fraction part of the day
	float hour24 = fractionPartOfTime * 24.0f;  // convert to hours (24h format)
	int hour = static_cast<int>(hour24);  // take integer part as the current hour
	float minutesFraction = hour24 - hour;  // get fraction part of the hour
	int minutes = static_cast<int>(minutesFraction * 60.0f);  // convert to minutes

	// convert 24h format to 12h format with AM/PM
	std::string am_pm = (hour < 12 || hour == 24) ? "AM" : "PM";
	if (hour > 12) hour -= 12;
	if (hour == 0) hour = 12;  // midnight is 12 AM, not 0

	// format time as a string
	std::stringstream ss;
	ss << std::setw(2) << std::setfill('0') << hour << ":" << std::setw(2) << std::setfill('0') << minutes << " " << am_pm;
	return ss.str();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
GameRaycastResult3D World::RaycastVsWorld(const Vec3& start, const Vec3& direction, float distance)
{
	GameRaycastResult3D hit;
	Chunk* currentChunk = nullptr;
	int blockIndex = 0;
	BlockIterator blockIter(currentChunk, blockIndex);

	if (start.z < 0 || start.z > CHUNK_MAX_Z)
		return hit;

	IntVec2 chunkCoords = Chunk::GetChunkCoordinatesForWorldPosition(start);
	std::map<IntVec2, Chunk*>::iterator iter = m_activeChunks.find(chunkCoords);
	if (iter != m_activeChunks.end())
	{
		currentChunk = iter->second;
	}
	else
	{
		return hit;
	}
	Vec3 chunkBoundsMins = currentChunk->GetWorldBounds().m_mins;
	IntVec3 localCoords = IntVec3(int(start.x - chunkBoundsMins.x), int(start.y - chunkBoundsMins.y), int(start.z));
	blockIndex = Chunk::GetBlockIndexFromLocalCoords(localCoords);
	blockIter = BlockIterator( currentChunk, blockIndex );

	if (BlockDef::IsBlockTypeOpaque(blockIter.GetBlock()->GetTypeID()))
	{
		hit.m_didImpact = true;
		hit.m_impactDist = 0.f;
		hit.m_impactPos = start;
		hit.m_impactNormal = -1 * direction;
		hit.m_blockImpacted = blockIter;
		return hit;
	}

	float fwdDistPerXCrossing = 1.0f / fabsf(direction.x);
	int tileStepDirectionX = direction.x <= 0 ? -1 : 1;
	float xAtFirstXCrossing = (float)(RoundDownToInt(start.x) + (tileStepDirectionX + 1) / 2);
	float xDistToFirstXCrossing = xAtFirstXCrossing - start.x;
	float fwdDistAtNextXCrossing = fabsf(xDistToFirstXCrossing) * fwdDistPerXCrossing;

	float fwdDistPerYCrossing = 1.0f / fabsf(direction.y);
	int tileStepDirectionY = direction.y <= 0 ? -1 : 1;
	float yAtFirstYCrossing = (float)(RoundDownToInt(start.y) + (tileStepDirectionY + 1) / 2);
	float yDistToFirstYCrossing = yAtFirstYCrossing - start.y;
	float fwdDistAtNextYCrossing = fabsf(yDistToFirstYCrossing) * fwdDistPerYCrossing;

	float fwdDistPerZCrossing = 1.0f / fabsf(direction.z);
	int tileStepDirectionZ = direction.z <= 0 ? -1 : 1;
	float zAtFirstZCrossing = (float)(localCoords.z + (tileStepDirectionZ + 1) / 2);
	float zDistToFirstZCrossing = zAtFirstZCrossing - start.z;
	float fwdDistAtNextZCrossing = fabsf(zDistToFirstZCrossing) * fwdDistPerZCrossing;

	while (true)
	{
		if (fwdDistAtNextXCrossing <= fwdDistAtNextYCrossing && fwdDistAtNextXCrossing <= fwdDistAtNextZCrossing)
		{
			blockIter = tileStepDirectionX > 0 ? blockIter.GetEastNeighbour() : blockIter.GetWestNeighbour();
			if (BlockDef::IsBlockTypeOpaque(blockIter.GetBlock()->GetTypeID()))
			{
				Vec3 impactPos = start + (direction * fwdDistAtNextXCrossing);
				hit.m_didImpact = true;
				hit.m_impactDist = fwdDistAtNextXCrossing;
				hit.m_impactPos = impactPos;
				hit.m_impactNormal = Vec3(start.x - hit.m_impactPos.x, 0.f, 0.f).GetNormalized();
				hit.m_blockImpacted = blockIter;
				return hit;
			}

			fwdDistAtNextXCrossing += fwdDistPerXCrossing;
		}
		else if (fwdDistAtNextYCrossing <= fwdDistAtNextXCrossing && fwdDistAtNextYCrossing <= fwdDistAtNextZCrossing)
		{
			blockIter = tileStepDirectionY > 0 ? blockIter.GetNorthNeighbour() : blockIter.GetSouthNeighbour();
			if (BlockDef::IsBlockTypeOpaque(blockIter.GetBlock()->GetTypeID()))
			{
				Vec3 impactPos = start + (direction * fwdDistAtNextYCrossing);
				hit.m_didImpact = true;
				hit.m_impactDist = fwdDistAtNextYCrossing;
				hit.m_impactPos = start + (direction * fwdDistAtNextYCrossing);
				hit.m_impactNormal = Vec3(0.f, start.y - hit.m_impactPos.y, 0.f).GetNormalized();
				hit.m_blockImpacted = blockIter;
				return hit;
			}

			fwdDistAtNextYCrossing += fwdDistPerYCrossing;
		}
		else
		{
			blockIter = tileStepDirectionZ > 0 ? blockIter.GetAboveNeighbour() : blockIter.GetBelowNeighbour();
			if (BlockDef::IsBlockTypeOpaque(blockIter.GetBlock()->GetTypeID()))
			{
				Vec3 impactPos = start + (direction * fwdDistAtNextZCrossing);
				hit.m_didImpact = true;
				hit.m_impactDist = fwdDistAtNextZCrossing;
				hit.m_impactPos = start + (direction * fwdDistAtNextZCrossing);
				hit.m_impactNormal = Vec3(0.f, 0.f, start.z - hit.m_impactPos.z).GetNormalized();
				hit.m_blockImpacted = blockIter;
				return hit;
			}

			fwdDistAtNextZCrossing += fwdDistPerZCrossing;
		}

		//past max distance, then return
		if (fwdDistAtNextXCrossing > distance && fwdDistAtNextYCrossing > distance && fwdDistAtNextZCrossing > distance)
		{
			return hit;
		}
	}

	hit.m_blockImpacted = BlockIterator( nullptr, 0 );
	return hit;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::PerformRaycast()
{
	if (!m_freezeRaycast)
	{
		m_cameraStart = m_camPosition;
		m_cameraForward = m_worldCamera.GetForwardVector();
	}

	m_raycastResult = RaycastVsWorld(m_cameraStart, m_cameraForward, m_raycastDistance);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::RenderRaycast()
{
	std::vector<Vertex_PCU> verts;
	if (m_raycastResult.m_didImpact)
	{
		verts.clear();
		
		AddVertsForRaycastImpactedFaces(verts);
		
		Shader* defaultShader = g_theRenderer->CreateShaderOrGetFromFile("Default");
		g_theRenderer->BindShader(defaultShader);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(int(verts.size()), verts.data());
	}

	if (m_freezeRaycast)
	{
		verts.clear();
		Vec3 start = m_cameraStart;
		Vec3 end = m_cameraStart + m_cameraForward * m_raycastDistance;
		float duration = 0.0f; 
		Rgba8 color = m_raycastResult.m_didImpact ? Rgba8::RED : Rgba8::GREEN;
		DebugAddWorldLine(start, end, 0.01f, duration, color, color, DebugRenderMode::X_RAY);
		if (m_raycastResult.m_didImpact)
		{
			AddVertsForRaycastImpactedFaces(verts);
		}
		AddVertsForSphere3D(verts, m_raycastResult.m_impactPos, 0.03f, Rgba8::BLACK);
		AddVertsForArrow3D(verts, m_raycastResult.m_impactPos, m_raycastResult.m_impactPos + 0.2f * m_raycastResult.m_impactNormal, 0.01f, Rgba8(0, 0, 255));
		
		Shader* defaultShader = g_theRenderer->CreateShaderOrGetFromFile("Default");
		g_theRenderer->BindShader(defaultShader);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(int(verts.size()), verts.data());
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::AddVertsForRaycastImpactedFaces(std::vector<Vertex_PCU>& verts)
{
	int index = m_raycastResult.m_blockImpacted.m_blockIndex;
	IntVec3 localCoords = m_raycastResult.m_blockImpacted.m_chunk->GetLocalCoordsFromBlockIndex(index);
	AABB3 blockBounds = m_raycastResult.m_blockImpacted.m_chunk->GetBoundsForBlock(localCoords.x, localCoords.y, localCoords.z);

	Vec3 mins = blockBounds.m_mins;
	Vec3 maxs = blockBounds.m_maxs;
	if (m_raycastResult.m_impactNormal == Vec3(1, 0, 0)) // East face
	{
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, mins.y, mins.z), Vec3(maxs.x, maxs.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, maxs.y, mins.z), Vec3(maxs.x, maxs.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, maxs.y, maxs.z), Vec3(maxs.x, mins.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, mins.y, maxs.z), Vec3(maxs.x, mins.y, mins.z), 0.01f, Rgba8::RED);
	}
	else if (m_raycastResult.m_impactNormal == Vec3(-1, 0, 0)) // West face
	{
		AddVertsForLineSegment3D(verts, Vec3(mins.x, maxs.y, mins.z), Vec3(mins.x, mins.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, mins.y, mins.z), Vec3(mins.x, mins.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, mins.y, maxs.z), Vec3(mins.x, maxs.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, maxs.y, maxs.z), Vec3(mins.x, maxs.y, mins.z), 0.01f, Rgba8::RED);
	}
	else if (m_raycastResult.m_impactNormal == Vec3(0, 1, 0)) // North face
	{
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, maxs.y, mins.z), Vec3(mins.x, maxs.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, maxs.y, mins.z), Vec3(mins.x, maxs.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, maxs.y, maxs.z), Vec3(maxs.x, maxs.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, maxs.y, maxs.z), Vec3(maxs.x, maxs.y, mins.z), 0.01f, Rgba8::RED);
	}
	else if (m_raycastResult.m_impactNormal == Vec3(0, -1, 0)) // South face
	{
		AddVertsForLineSegment3D(verts, Vec3(mins.x, mins.y, mins.z), Vec3(maxs.x, mins.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, mins.y, mins.z), Vec3(maxs.x, mins.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, mins.y, maxs.z), Vec3(mins.x, mins.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, mins.y, maxs.z), Vec3(mins.x, mins.y, mins.z), 0.01f, Rgba8::RED);
	}
	else if (m_raycastResult.m_impactNormal == Vec3(0, 0, 1)) // Top face
	{
		AddVertsForLineSegment3D(verts, Vec3(mins.x, mins.y, maxs.z), Vec3(maxs.x, mins.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, mins.y, maxs.z), Vec3(maxs.x, maxs.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, maxs.y, maxs.z), Vec3(mins.x, maxs.y, maxs.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, maxs.y, maxs.z), Vec3(mins.x, mins.y, maxs.z), 0.01f, Rgba8::RED);
	}
	else if (m_raycastResult.m_impactNormal == Vec3(0, 0, -1)) // Bottom face
	{
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, mins.y, mins.z), Vec3(mins.x, mins.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, mins.y, mins.z), Vec3(mins.x, maxs.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(mins.x, maxs.y, mins.z), Vec3(maxs.x, maxs.y, mins.z), 0.01f, Rgba8::RED);
		AddVertsForLineSegment3D(verts, Vec3(maxs.x, maxs.y, mins.z), Vec3(maxs.x, mins.y, mins.z), 0.01f, Rgba8::RED);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::CheckForCompletedJobs()
{
	Job* completedJob = g_theJobSystem->RetrieveCompletedJobs();

	while (completedJob)
	{
		if (completedJob->m_jobType == CHUNK_GENERATION_JOB_TYPE)
		{
			ChunkGenerationJob* chunkJob = (ChunkGenerationJob*)completedJob;
			if (chunkJob)
			{
				Chunk* chunk = chunkJob->m_chunk;

				if (chunk && chunk->m_status == ChunkState::ACTIVATING_GENERATE_COMPLETE)
				{
					ActivateNewChunk(chunk->GetChunkCoordinates());
				}
			}
		}
		else if (completedJob->m_jobType == DISK_JOB_TYPE)
		{
			ChunkDiskLoadJob* loadJob = dynamic_cast<ChunkDiskLoadJob*>(completedJob);
			if (loadJob) 
			{
				Chunk* chunk = loadJob->m_chunk;

				if (chunk && chunk->m_status == ChunkState::ACTIVATING_LOAD_COMPLETE)
				{
					ProcessChunkAfterDiskLoadJob(chunk);
				}
			}
			else // It must have been a save job then
			{
				ChunkDiskSaveJob* saveJob = dynamic_cast<ChunkDiskSaveJob*>(completedJob);
				Chunk* chunk = saveJob->m_chunk;

				if (chunk && chunk->m_status == ChunkState::DEACTIVATING_SAVE_COMPLETE)
				{
					ProcessChunkAfterDiskSaveJob(chunk);
				}
			}
		}

		delete completedJob;
		completedJob = g_theJobSystem->RetrieveCompletedJobs();
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::SetInitialCameraPosition()
{
	m_camPosition = Vec3(0.f, 0.f, 90.f);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::SetChunkConstantsValues()
{
	m_chunkDeactivationRange = m_chunkActivationRange + static_cast<float>(CHUNK_SIZE_X + CHUNK_SIZE_Y);
	m_maxChunkRadiusX = 1 + int(m_chunkActivationRange) / CHUNK_SIZE_X;
	m_maxChunkRadiusY = 1 + int(m_chunkActivationRange) / CHUNK_SIZE_Y;
	m_maxChunks = (2 * m_maxChunkRadiusX) * (2 * m_maxChunkRadiusY);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::InitializeChunk(IntVec2 const& coords)
{
	Chunk* newChunk = nullptr;

	m_initiliazedChunksMutex.lock();

	newChunk = new Chunk(this, coords);
	m_initializedChunks[coords] = newChunk;

	m_initiliazedChunksMutex.unlock();

	if (newChunk->CanBeLoadedFromFile()) 
	{
		ChunkDiskLoadJob* newChunkLoadJob = new ChunkDiskLoadJob(newChunk);
		g_theJobSystem->QueueJob(newChunkLoadJob);
	}
	else 
	{
		ChunkGenerationJob* newChunkGenJob = new ChunkGenerationJob(newChunk);
		g_theJobSystem->QueueJob(newChunkGenJob);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::ProcessChunkAfterDiskJob(Chunk* chunk)
{
	if (chunk->m_status == ChunkState::ACTIVATING_LOAD_COMPLETE)
	{
		ActivateNewChunk(chunk->GetChunkCoordinates());
	}
	else if (chunk->m_status == ChunkState::DEACTIVATING_SAVE_COMPLETE)
	{
		DeactivateChunk(chunk);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::ProcessChunkAfterDiskLoadJob(Chunk* chunk)
{
	if (chunk->m_status == ChunkState::ACTIVATING_LOAD_COMPLETE)
	{
		ActivateNewChunk(chunk->GetChunkCoordinates());
	}
	else
	{
		// Handle unexpected state by throwing an error and terminating the program
		ERROR_AND_DIE("Unexpected chunk state after completing disk load job");
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::ProcessChunkAfterDiskSaveJob(Chunk* chunk)
{
	if (chunk->m_status == ChunkState::DEACTIVATING_SAVE_COMPLETE)
	{
		DeactivateChunk(chunk);
	}
	else
	{
		// Handle unexpected state by throwing an error and terminating the program
		ERROR_AND_DIE("Unexpected chunk state after completing disk save job");
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::UnlinkChunkFromNeighbors(Chunk* chunkToUnlink)
{
	IntVec2 chunkCoords = chunkToUnlink->GetChunkCoordinates();
	Chunk* neighborChunk = GetChunkForChunkCoordinates(chunkCoords + NorthStep);
	if (neighborChunk) 
	{
		neighborChunk->m_southNeighbor = nullptr;
	}

	neighborChunk = GetChunkForChunkCoordinates(chunkCoords + SouthStep);
	if (neighborChunk)
	{
		neighborChunk->m_northNeighbor = nullptr;
	}

	neighborChunk = GetChunkForChunkCoordinates(chunkCoords + WestStep);
	if (neighborChunk) 
	{
		neighborChunk->m_eastNeighbor = nullptr;
	}

	neighborChunk = GetChunkForChunkCoordinates(chunkCoords + EastStep);
	if (neighborChunk) 
	{
		neighborChunk->m_westNeighbor = nullptr;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::LinkChunkToNeighbors(Chunk* chunkToLink)
{
	IntVec2 chunkCoords = chunkToLink->GetChunkCoordinates();
	Chunk* neighborChunk = GetChunkForChunkCoordinates(chunkCoords + NorthStep);
	
	if (neighborChunk) 
	{
		neighborChunk->m_southNeighbor = chunkToLink;
		chunkToLink->m_northNeighbor = neighborChunk;
	}

	neighborChunk = GetChunkForChunkCoordinates(chunkCoords + SouthStep);
	if (neighborChunk) 
	{
		neighborChunk->m_northNeighbor = chunkToLink;
		chunkToLink->m_southNeighbor = neighborChunk;
	}

	neighborChunk = GetChunkForChunkCoordinates(chunkCoords + WestStep);
	if (neighborChunk) 
	{
		neighborChunk->m_eastNeighbor = chunkToLink;
		chunkToLink->m_westNeighbor = neighborChunk;
	}

	neighborChunk = GetChunkForChunkCoordinates(chunkCoords + EastStep);
	if (neighborChunk) 
	{
		neighborChunk->m_westNeighbor = chunkToLink;
		chunkToLink->m_eastNeighbor = neighborChunk;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::QueueForSaving(Chunk* chunk)
{
	ChunkDiskSaveJob* newSaveJob = new ChunkDiskSaveJob(chunk);
	//m_activeChunks.erase(chunk->GetChunkCoordinates());

	g_theJobSystem->QueueJob(newSaveJob);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void World::CreateConstantBufferForMinecraftConstants()
{
	m_gameCBO = g_theRenderer->CreateConstantBuffer(sizeof(MinecraftGameConstants));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------