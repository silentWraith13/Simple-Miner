#pragma once
#include "Game/Chunks.hpp"
#include "Game/BlockIterator.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include <deque>

//--------------------------------------------------------------------------------------------------------------------------------------------------------
static IntVec2 const NorthStep = IntVec2(0, 1);
static IntVec2 const SouthStep = IntVec2(0, -1);
static IntVec2 const EastStep = IntVec2(1, 0);
static IntVec2 const WestStep = IntVec2(-1, 0);
//--------------------------------------------------------------------------------------------------------------------------------------------------------
struct GameRaycastResult3D : public RaycastResult3D
{
	BlockIterator m_blockImpacted;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class World
{
public:
	World();
	~World();

	void				Update(float deltaSeconds);
	void				Render();

	//Camera functions
	void				SetScreenCamera();
	void				SetWorldCamera();
	void				UpdateWorldCamera(float deltaSeconds);
	void				UpdateCameraKeyboardControls(float deltaSeconds);
	void				SetInitialCameraPosition();
	
	//Miscellaneous
	void				AddCameraCompass();
	void				AddBasisAtOrigin() const;
	void				AddUI();
	void				HandleInput();
	void				LoadGameConfig();
	void				InitializeShader();
	int					GetWorldSeed() const;

	//Chunk functions
	void				RenderChunk();
	void				AddDebugDrawVertsForAllChunks(std::vector<Vertex_PCU>& verts);
	void				RenderDebugDrawChunk();
	void				ToggleDebugDrawChunk();
	void				RandomizeChunks();
	void				ActivateNewChunk(const IntVec2& chunkCoords);
	void				DeactivateFurthestChunk();
	void				CheckChunksForMeshUpdate();
	void				OnDiggingOfBlock();
	void				OnPlacingOfBlock(BlockDefID id);
	IntVec3				GetHighestNonAirBlock();
	Chunk*				GetChunkForWorldPosition(const Vec3& position);
	Chunk*				GetChunkForChunkCoordinates(const IntVec2& chunkCoords);
	IntVec2				GetChunkCoordinatesForWorldPosition(Vec3 const& pos) const;
	bool				ActivateNearestMissingChunk();
	bool				DoesChunkExist(const IntVec2& chunkCoords);
	void				DeactivateAllChunks();
	void 				DeactivateChunk(Chunk* chunk);
	void				SetChunkConstantsValues();
	void				InitializeChunk(IntVec2 const& coords);
	void 				ProcessChunkAfterDiskJob(Chunk* chunk);
	void				ProcessChunkAfterDiskLoadJob(Chunk* chunk);
	void				ProcessChunkAfterDiskSaveJob(Chunk* chunk);
	void				UnlinkChunkFromNeighbors(Chunk* chunkToUnlink);
	void				LinkChunkToNeighbors(Chunk* chunkToLink);
	void				QueueForSaving(Chunk* chunk);

	//Lighting functions
	void				ProcessDirtyLighting();
	void				MarkLightingDirty(const BlockIterator& blockIter);
	void				MarkLightingDirtyIfNotOpaque(const BlockIterator& blockIter);
	void				MarkLightingDirtyIfNotSkyAndNotOpaque(const BlockIterator& blockIter);
	void				MarkNeighbouringChunksAndBlocksAsDirty(const BlockIterator& blockIter);
	uint8_t				ComputeIndoorLightInfluence(const BlockIterator& blockIter) const;
	uint8_t				ComputeOutdoorLightInfluence(const BlockIterator& blockIter) const;
	uint8_t				GetHighestOutdoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const;
	uint8_t				GetHighestIndoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const;

	//Constant buffer functions
	void				CopyDataToCBOAndBindIt() const;
	void				CreateConstantBufferForMinecraftConstants();
	
	//Time and day cycle functions
	void				UpdateDayCycle(float deltaSeconds);
	void				UpdateSkyAndLightColors(float fractionPartOfTime);
	float				GetWorldTime() const { return m_worldTime; }
	std::string			GetWorldTimeAsHoursAndMinutes() const;

	//Raycasting functions
	GameRaycastResult3D RaycastVsWorld(const Vec3& start, const Vec3& direction, float distance);
	void				PerformRaycast();
	void				RenderRaycast();
	void				AddVertsForRaycastImpactedFaces(std::vector<Vertex_PCU>& verts);

	void				CheckForCompletedJobs();
		
	//Member variables
	Camera						m_worldCamera;
	Camera						m_screenTextCamera;
	Vec3						m_camPosition;
	Vec3						m_velocity;
	Vec3						m_cameraStart;
	Vec3						m_cameraForward;
	EulerAngles					m_camOrientation;
	std::vector<Vertex_PCU>		m_debugDrawChunkVerts;
	std::map<IntVec2, Chunk*>   m_activeChunks;
	std::map<IntVec2, Chunk*>   m_initializedChunks;
	std::mutex					m_initiliazedChunksMutex;
	std::deque<BlockIterator>	m_dirtyLightBlocks;
	float						m_chunkDeactivationRange = 0.f;
	float						m_raycastDistance = 10.f;
	int							m_maxChunkRadiusX = 0;
	int							m_maxChunkRadiusY = 0;
	int							m_maxChunks = 0;
	int							m_blockTypeToAdd = 8;
	int							m_worldSeed = 6;
	bool						m_shouldActivateChunk = false;
	bool						m_debugDrawChunk = false;
	bool						m_showCompass = false;
	bool						m_freezeRaycast = false;
	bool						m_placeBlockAtCurrentPos = false;
	Shader*						m_shader = nullptr;
	ConstantBuffer*				m_gameCBO = nullptr;
	GameRaycastResult3D			m_raycastResult;
	Rgba8						m_currentOutdoorLightColor;
	Rgba8						m_currentSkyColor;

	//GameConfig.Xml variables
	bool						m_autoCreateChunks = false;
	float						m_chunkActivationRange = 0.f;
	bool						m_loadSavedChunks = false;
	bool 						m_saveModifiedChunks = false;
	float						m_worldSecondsPerRealSecond = 0.f;
	bool						m_debugStepLightPropagation = false;
	bool						m_debugDrawLightMarkers = false;
	bool						m_debugUseWhiteBlocks = false;
	bool						m_debugDisableFog = false;
	bool						m_debugDisableSky = false;
	bool						m_debugDisablePerlinNoise = false;
	bool						m_debugDisableHSR = false;
	bool						m_debugDisableGlowstone = false;
	bool						m_debugDisableWorldShader = false;
	float						m_glowStrength = 0.f;
	float						m_lightingStrength = 0.f;
	float						m_fogStartDistance = 0.f;
	float						m_fogEndDistance = 0.f;
	float						m_fogMaxAlpha = 0.f;
	float						m_worldTime = 0.5f;
	float						m_worldTimeScale = 0.f;
	float						m_currentWorldTimeScaleAccelerationFactor = 1.f;
	Rgba8						m_nightSkyColor = Rgba8(255, 255, 255);
	Rgba8						m_daySkyColor = Rgba8(255, 255, 255);
	Rgba8						m_nightOutdoorLightColor = Rgba8(255, 255, 255);
	Rgba8						m_dayOutdoorLightColor = Rgba8(255, 255, 255);
	Rgba8						m_indoorLightColor = Rgba8(255, 255, 255);
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------