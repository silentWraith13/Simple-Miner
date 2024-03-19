#pragma once
#include "Game/BlockDef.hpp"
#include "Game/Block.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Core/JobSystem.hpp"
#include <vector>

//--------------------------------------------------------------------------------------------------------------------------------------------------------
class VertexBuffer;
class World;
struct BlockIterator;
class  BlockTemplate;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
constexpr int CHUNK_BITS_X = 4;
constexpr int CHUNK_BITS_Y = 4;
constexpr int CHUNK_BITS_Z = 7;

constexpr int CHUNK_SIZE_X = 1 << CHUNK_BITS_X;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_BITS_Y;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_BITS_Z;

constexpr int CHUNK_MAX_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_MAX_Y = CHUNK_SIZE_Y - 1;
constexpr int CHUNK_MAX_Z = CHUNK_SIZE_Z - 1;

constexpr int CHUNK_BLOCKS_PER_LAYER = CHUNK_SIZE_X * CHUNK_SIZE_Y;
constexpr int CHUNK_BLOCKS_TOTAL = CHUNK_BLOCKS_PER_LAYER * CHUNK_SIZE_Z;

constexpr int SEA_LEVEL = CHUNK_SIZE_Z / 2;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
struct CaveInfo
{
	CaveInfo(IntVec2 chunkCoords)
		:m_startChunkCoords(chunkCoords) {};
	
	IntVec2 m_startChunkCoords;
	Vec3    m_startWorldPos;
	std::vector<IntVec3> m_caveNodePositions;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
enum ChunkState
{

	MISSING,							//chunk not present yet
	ACTIAVTING_QUEUED_LOAD,
	ACTIVATING_LOAD_COMPLETE,
	
	ACTIVATING_QUEUED_GENERATE,			//chunk has been queued for generating blocks in a thread
	ACTIVATING_GENERATING,				//worker thread is executing its generation function
	ACTIVATING_GENERATE_COMPLETE,		//worker thread has finished its generation and now can be retrieved by main thread
	ACTIVE,								//is in the active chunks list	
	
	DEACTIVATING_QUEUED_SAVE,
	DEACTIVATING_SAVE_COMPLETE,
	NUM_CHUNK_STATES
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class Chunk
{
public:
	Chunk(World* world, IntVec2 const& chunkCoords);
	~Chunk();

	void			Update();
	void			Render();
	
	void			Generateblocks();
	void			RebuildMesh();
	void			SetBlockType(int localX, int localY, int localZ, BlockDefID blockType);
	void			SetBlockTypeID(IntVec3 const& localCoords, BlockDefID blockType);
	void			InitializeVertexBuffer();
	static bool		IsInBoundsLocal(int localX, int localY, int localZ);
	static int		GetBlockIndex(int localX, int localY, int localZ);
	AABB3			GetBoundsForBlock(int localX, int localY, int localZ);
	static AABB3	GetChunkBoundsForChunkCoords(IntVec2 chunkCoords);
	bool			IsBlockOpaque(int localX, int localY, int localZ);
	Block*			GetBlock(int blockIndex) const;
	Block*			GetBlockAtCoords(int localX, int localY, int localZ);
	int				GetNumBlocks();
	AABB3			GetWorldBounds();
	int				GetChunkMeshVertices();
	IntVec2			GetChunkCoordinates();
	bool			IsChunkDirty();
	void			SetChunkToDirty();
	bool			ShouldTheMeshBeRebuilt();
	static IntVec2  GetChunkCoordinatesForWorldPosition(const Vec3& position);
	static Vec2	    GetChunkCenterXYForChunkCoords(const IntVec2& chunkCoords);
	static int	    GetBlockIndexFromLocalCoords(const IntVec3& localCoords);
	IntVec3			GetLocalCoordsFromBlockIndex(int blockIndex) const;
	IntVec3		    GetGlobalCoordsForIndex(int blockIndex) const;
	IntVec3			GetLocalCoordsForGlobalCoords(IntVec3 const& globalCoords);
	int				GetHighestZNonAirBlock(int localX, int localY);
	bool			CanBeLoadedFromFile();
	std::string		GetChunkFileName();
	bool			LoadBlocksFromFile();
	void			SaveBlockToFile();
	void			DisconnectFromNeighbors();
	void		    AddVertsForBlock(std::vector<Vertex_PCU>& verts, int localX, int localY, int localZ);
	bool			HasAllValidNeighbours() const;
	void			ProcessLightingForDugBlock(const BlockIterator& blockIter);
	void			ProcessLightingForAddedBlock(const BlockIterator& blockIter);
	Rgba8			GetFaceColor(const BlockIterator& blockIterator);
	void			InitializeLighting();
	void			DigBlock(const BlockIterator& blockIter);
	void			PlaceBlock(const BlockIterator& blockIter);
	void			SpawnBlockTemplate(std::string const& name, IntVec3 const& localCoords);
	void			AddCaves(unsigned int worldCaveSeed);
	void			ForceCreateWorldFolder();
	void			CarveAABB3D(Vec3 worldCenter, Vec3 halfDimensions);
	void			CarveCapsule3D(Vec3 worldStart, Vec3 worldEnd, float radius);
	Vec3			GetChunkCenter();
	int				CalculateGroundZHeightForGlobalXY(float globalX, float globalY);
	void			AddTrees();
	void			GenerateCaves();
	void			GetCavesStartPerlinNoise(std::map<IntVec2, float>& perlinNoiseHolder);
	void			GetCavePath(IntVec2 const& coords, std::vector<IntVec3>& cavePoints);
	void			CarveCavePath(std::vector<IntVec3>& cavePoints);
	void			CarveBlockInRadius(Vec3 const& originPos, IntVec3 const& localCoords, float radius, bool replaceWithDirt, bool placeLampInMiddle);
	bool			AreCoordsConsideredLocalMaxima(IntVec2 const& coords, int radius, std::map<IntVec2, float> const& perlinNoiseHolder) const;
	bool			AreLocalCoordsWithinChunk(IntVec3 const& localCoords);
	IntVec3			GetGlobalCoordsForLocalCoords(IntVec3 const& localCoords);
public:
	IntVec2					m_chunkCoords = IntVec2(0, 0);
	AABB3					m_worldBounds = AABB3::ZERO_TO_ONE;
	Block*					m_blocks = nullptr;
	int						m_numBlocks = 0;
	unsigned int			m_worldSeed = 0;
	std::vector<Vertex_PCU> m_cpuMesh;
	VertexBuffer*			m_gpuMeshVBO = nullptr;
	bool					m_isChunkDirty = true;
	bool					m_needsSaving = false;
	World*					m_world = nullptr;
	Chunk*					m_northNeighbor = nullptr;
	Chunk*					m_southNeighbor = nullptr;
	Chunk*					m_eastNeighbor = nullptr;
	Chunk*					m_westNeighbor = nullptr;
	std::vector<CaveInfo>	m_nearbyCaves;
	std::atomic<ChunkState> m_status = MISSING;
	int						m_caveCheckRadius = 40;
	int m_caveBlockSteps =  8;
	int m_caveNodeAmount = 70;
	int m_caveDepthStart = 20;
	float m_caveTurnRate = 35.0f;
	float m_caveMaxRadius = 10.0f;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
constexpr int DISK_JOB_TYPE = 1;
constexpr int CHUNK_GENERATION_JOB_TYPE = 1 << 1;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class ChunkGenerationJob : public Job {
public:
	ChunkGenerationJob(Chunk* chunk) :
		m_chunk(chunk),
		Job::Job(CHUNK_GENERATION_JOB_TYPE)
	{}

	virtual void Execute() override;
	virtual void OnFinished() override;
	
	Chunk* m_chunk = nullptr;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class ChunkDiskLoadJob : public Job
{
public:
	ChunkDiskLoadJob(Chunk* chunk) :
		m_chunk(chunk),
		Job::Job(DISK_JOB_TYPE) {}

	virtual void Execute() override;
	virtual void OnFinished() override;

	Chunk* m_chunk = nullptr;
	bool m_loadingSuccessful = false;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class ChunkDiskSaveJob : public Job 
{
public:
	ChunkDiskSaveJob(Chunk* chunk) :
		m_chunk(chunk),
		Job::Job(DISK_JOB_TYPE) {}

	virtual void Execute() override;
	virtual void OnFinished() override;

	Chunk* m_chunk = nullptr;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------