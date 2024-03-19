#include "Game/Chunks.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Game/World.hpp"
#include "Game/BlockIterator.hpp"
#include "Game/BlockTemplate.hpp"
#include "ThirdParty/Squirrel/SmoothNoise.hpp"
#include "ThirdParty/Squirrel/RawNoise.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
Chunk::Chunk(World* world, IntVec2 const& chunkCoords)
	:m_world(world), m_chunkCoords(chunkCoords)
{
	m_blocks = new Block[CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z];
	
	m_worldBounds.m_mins.x = (float)CHUNK_SIZE_X * (float)chunkCoords.x;
	m_worldBounds.m_mins.y = (float)CHUNK_SIZE_Y * (float)chunkCoords.y;
	m_worldBounds.m_mins.z = 0;

	m_worldBounds.m_maxs.x = m_worldBounds.m_mins.x + (float)CHUNK_SIZE_X;
	m_worldBounds.m_maxs.y = m_worldBounds.m_mins.y + (float)CHUNK_SIZE_Y;
	m_worldBounds.m_maxs.z = (float)CHUNK_SIZE_Z;
	
	m_numBlocks = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
	m_worldSeed = m_world->GetWorldSeed();
	//Generateblocks();
	InitializeVertexBuffer();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Chunk::~Chunk()
{
//  	if (m_needsSaving)
//  	{
//  		Job* saveJob = new ChunkDiskSaveJob(this);
//  		g_theJobSystem->QueueJob(saveJob);
//  		m_needsSaving = false;
//  
//  		// Wait for the save job to complete
//  		while (m_status != ChunkState::DEACTIVATING_SAVE_COMPLETE)
//  		{
//  			// Sleep or yield to let the job system process the save job
//  			std::this_thread::yield();
//  		}
//  	}

	delete m_gpuMeshVBO;
	m_gpuMeshVBO = nullptr;
	
	delete[] m_blocks; 
	m_blocks = nullptr;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::Update()
{
	if (ShouldTheMeshBeRebuilt())
	{
		RebuildMesh();
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::Render()
{	
	if (m_world->m_debugUseWhiteBlocks)
	{
		g_theRenderer->BindTexture(nullptr);
	}
	else
	{
		g_theRenderer->BindTexture(&g_terrainSpriteSheet->GetTexture());
	}
	g_theRenderer->DrawVertexBuffer(m_gpuMeshVBO, (int)(m_cpuMesh.size()));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::Generateblocks()
{
	RandomNumberGenerator rng(m_worldSeed);
	unsigned int worldCaveSeed = m_worldSeed + 7;

	static BlockDefID dirt = BlockDef::GetBlockDefIDByName("dirt");
	static BlockDefID coal = BlockDef::GetBlockDefIDByName("coal");
	static BlockDefID diamond = BlockDef::GetBlockDefIDByName("diamond");
	static BlockDefID grass = BlockDef::GetBlockDefIDByName("grass");
	static BlockDefID gold = BlockDef::GetBlockDefIDByName("gold");
	static BlockDefID stone = BlockDef::GetBlockDefIDByName("stone");
	static BlockDefID air = BlockDef::GetBlockDefIDByName("air");
	static BlockDefID iron = BlockDef::GetBlockDefIDByName("iron");
	static BlockDefID water = BlockDef::GetBlockDefIDByName("water");
	static BlockDefID sand = BlockDef::GetBlockDefIDByName("sand");
	static BlockDefID ice = BlockDef::GetBlockDefIDByName("ice");
	static BlockDefID cloud = BlockDef::GetBlockDefIDByName("cloud");
	static BlockDefID snow = BlockDef::GetBlockDefIDByName("snow");
	static BlockDefID snowgrass = BlockDef::GetBlockDefIDByName("snowgrass");

	int maxIceDepth = 20;
	int maxSandDepth = 8;
	int oceanHeightZ = CHUNK_SIZE_Z / 2;
	int freezeLevel = 0;

	int   groundHeightZ = 0;
	float temperature = 0.f;
	float humidity = 0.f;
	float cloudness = 0.f;

	for (int localY = 0; localY < CHUNK_SIZE_Y; localY++)
	{
		float globalY = m_worldBounds.m_mins.y + float(localY);

		for (int localX = 0; localX < CHUNK_SIZE_X; localX++)
		{
			float globalX = m_worldBounds.m_mins.x + float(localX);

			//Determine biome factors for this column
			temperature = 0.5f + 0.5f * Compute2dPerlinNoise(globalX, globalY, 800.f, 9, 0.5f, 2.f, true, m_worldSeed + 1);
			temperature += 0.01f * Get2dNoiseZeroToOne(static_cast<int>(globalX), static_cast<int>(globalY), m_worldSeed + 21);
			humidity = 0.5f + 0.5f * Compute2dPerlinNoise(globalX, globalY, 800.f, 5, 0.5f, 2.f, true, m_worldSeed + 2);
			cloudness = RangeMapClamped(Compute2dPerlinNoise(globalX, globalY, 30.f, 9, 0.5f, 2.f, true, m_worldSeed + 9), -1.f, 1.f, 0.f, 1.f);

			groundHeightZ = CalculateGroundZHeightForGlobalXY(globalX, globalY);
			
			//Calculate ice depth(should be zero for areas with no ice) based on [lowness of] temp.
			int iceDepth = RoundDownToInt(RangeMapClamped(temperature, 0.f, 0.4f, float(maxIceDepth), 0.f));
			int iceHeightZ = oceanHeightZ - iceDepth;

			//Calculate sand depth(should be zero for areas with no sand) based on [lowness of] humidity.
			int sandDepth = RoundDownToInt(RangeMapClamped(humidity, 0.f, 0.4f, float(maxSandDepth), 0.f));
			int sandHeightZ = groundHeightZ - sandDepth;
			freezeLevel = oceanHeightZ + static_cast<int>(temperature * 50);

			int dirtMinZ = groundHeightZ - rng.RollRandomIntInRange(3, 4);

			for (int localZ = 0; localZ < CHUNK_SIZE_Z; localZ++)
			{
				BlockDefID blockType = air;

				if (localZ < dirtMinZ)
				{
					blockType = stone;

					if (rng.RollRandomChance(0.05f))
						blockType = coal;

					if (rng.RollRandomChance(0.02f))
						blockType = iron;

					if (rng.RollRandomChance(0.005f))
						blockType = gold;

					if (rng.RollRandomChance(0.001f))
						blockType = diamond;
				}

				else if (localZ < groundHeightZ)
				{
					blockType = dirt;
					if (localZ > sandHeightZ)
					{
						blockType = sand;
					}
				}

				else if (localZ == groundHeightZ)
				{
					blockType = grass;

					if (humidity < 0.65f && localZ == oceanHeightZ)
					{
						blockType = sand;
					}
					if (localZ > sandHeightZ)
					{
						blockType = sand;
					}
				}
				else if (localZ <= oceanHeightZ && blockType == air)
				{
					blockType = water;
					if (localZ > iceHeightZ)
					{
						blockType = ice;
					}
				}

				if (localZ == 125)
				{
					if (cloudness > 0.7f)
						blockType = cloud;
				}

				if (localZ >= freezeLevel && localZ <= groundHeightZ)
				{
					if (localZ == freezeLevel)
					{
						blockType = snowgrass;
					}
					else
					{
						blockType = snow;
					}
				}

				SetBlockType(localX, localY, localZ, blockType);
			}
		}
	}
	
	AddTrees();
	AddCaves(worldCaveSeed);
	
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::RebuildMesh()
{
	m_cpuMesh.clear();

	for (int localZ = 0; localZ < CHUNK_SIZE_Z; localZ++)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; localY++)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; localX++)
			{
				AddVertsForBlock(m_cpuMesh, localX, localY, localZ);
			}
		}
	}

	if (m_gpuMeshVBO != nullptr)
	{
		g_theRenderer->CopyCPUToGPU(m_cpuMesh.data(), m_cpuMesh.size() * sizeof(Vertex_PCU), m_gpuMeshVBO);
	}

	m_isChunkDirty = false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::SetBlockType(int localX, int localY, int localZ, BlockDefID blockType)
{
	if (!IsInBoundsLocal(localX, localY, localZ))
	{
		return;
	}

	int blockIndex = GetBlockIndex(localX, localY, localZ);
	m_blocks[blockIndex].SetTypeID(blockType);
	m_isChunkDirty = true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::SetBlockTypeID(IntVec3 const& localCoords, BlockDefID blockType)
{
	SetBlockType(localCoords.x, localCoords.y, localCoords.z, blockType);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::InitializeVertexBuffer()
{
	if (m_gpuMeshVBO != nullptr)
	{
		delete m_gpuMeshVBO;
		m_gpuMeshVBO = nullptr;
	}

	m_gpuMeshVBO = g_theRenderer->CreateVertexBuffer(1, sizeof(Vertex_PCU));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::IsInBoundsLocal(int localX, int localY, int localZ)
{
	return localX >= 0 && localX < CHUNK_SIZE_X
		&& localY >= 0 && localY < CHUNK_SIZE_Y
		&& localZ >= 0 && localZ < CHUNK_SIZE_Z;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int Chunk::GetBlockIndex(int localX, int localY, int localZ)
{
	return localX + (localY * CHUNK_SIZE_X) + (localZ * CHUNK_BLOCKS_PER_LAYER);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
AABB3 Chunk::GetBoundsForBlock(int localX, int localY, int localZ)
{
	AABB3 bounds;
	bounds.m_mins = m_worldBounds.m_mins + Vec3( (float)localX, (float)localY, (float)localZ);
	bounds.m_maxs = bounds.m_mins + Vec3(1.f, 1.f, 1.f);
	return bounds;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
AABB3 Chunk::GetChunkBoundsForChunkCoords(IntVec2 chunkCoords)
{
	float minX = float(chunkCoords.x * CHUNK_SIZE_X);
	float minY = float(chunkCoords.y * CHUNK_SIZE_Y);
	float minZ = 0.f;
	float maxX = minX + float(CHUNK_SIZE_X);
	float maxY = minY + float(CHUNK_SIZE_Y);
	float maxZ = float(CHUNK_SIZE_Z);

	return AABB3(minX, minY, minZ, maxX, maxY, maxZ);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::IsBlockOpaque(int localX, int localY, int localZ)
{
	if (localX < 0 || localY < 0 || localZ < 0 || localX >= CHUNK_SIZE_X || localY >= CHUNK_SIZE_Y || localZ >= CHUNK_SIZE_Z)
	{
		return false;
	}

	int blockIndex = GetBlockIndex(localX, localY, localZ);
	Block const* block = m_blocks + blockIndex;
	BlockDef const& blockDef = BlockDef::GetBlockDefByID(block->GetTypeID());

	return blockDef.m_isOpaque;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Block* Chunk::GetBlock(int blockIndex) const
{
	return &m_blocks[blockIndex];
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Block* Chunk::GetBlockAtCoords(int localX, int localY, int localZ)
{
	if (!IsInBoundsLocal(localX, localY, localZ)) 
	{
		return nullptr;
	}
	int index = GetBlockIndex(localX, localY, localZ);
	return GetBlock(index);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int Chunk::GetNumBlocks()
{
	return m_numBlocks;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
AABB3 Chunk::GetWorldBounds()
{
	return m_worldBounds;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int Chunk::GetChunkMeshVertices()
{
	return (int)m_cpuMesh.size();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec2 Chunk::GetChunkCoordinates()
{
	return m_chunkCoords;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::IsChunkDirty()
{
	if (m_isChunkDirty)
	{
		return true;
	}

	return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::SetChunkToDirty()
{
	m_isChunkDirty = true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::ShouldTheMeshBeRebuilt()
{
	return m_isChunkDirty && HasAllValidNeighbours();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec2 Chunk::GetChunkCoordinatesForWorldPosition(const Vec3& position)
{
	int x = int(floorf(position.x)) >> CHUNK_BITS_X;
	int y = int(floorf(position.y)) >> CHUNK_BITS_Y;
	return IntVec2(x, y);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Vec2 Chunk::GetChunkCenterXYForChunkCoords(const IntVec2& chunkCoords)
{
	float chunkCenterX = static_cast<float>(chunkCoords.x << CHUNK_BITS_X) + 0.5f * static_cast<float>(CHUNK_SIZE_X);
	float chunkCenterY = static_cast<float>(chunkCoords.y << CHUNK_BITS_Y) + 0.5f * static_cast<float>(CHUNK_SIZE_Y);
	return Vec2(chunkCenterX, chunkCenterY);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int Chunk::GetBlockIndexFromLocalCoords(const IntVec3& localCoords)
{
	return localCoords.x | (localCoords.y << CHUNK_BITS_X) | (localCoords.z << (CHUNK_BITS_X + CHUNK_BITS_Y));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetLocalCoordsFromBlockIndex(int blockIndex) const
{
	int x = blockIndex & CHUNK_MAX_X;
	int y = (blockIndex >> CHUNK_BITS_X) & CHUNK_MAX_Y;
	int z = (blockIndex >> (CHUNK_BITS_X + CHUNK_BITS_Y)) & CHUNK_MAX_Z;
	return IntVec3(x, y, z);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoordsForIndex(int blockIndex) const
{
	IntVec3 localCoords = GetLocalCoordsFromBlockIndex(blockIndex);
	return IntVec3(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y, 0) + localCoords;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetLocalCoordsForGlobalCoords(IntVec3 const& globalCoords)
{
	int chunkX = m_chunkCoords.x * CHUNK_SIZE_X;
	int chunkY = m_chunkCoords.y * CHUNK_SIZE_Y;

	return IntVec3(globalCoords.x - chunkX, globalCoords.y - chunkY, globalCoords.z);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int Chunk::GetHighestZNonAirBlock(int localX, int localY)
{
	for (int localZ = CHUNK_SIZE_Z - 1; localZ >= 0; localZ--)
	{
		int blockIndex = GetBlockIndex(localX, localY, localZ);
		Block* block = GetBlock(blockIndex);

		if (block->GetTypeID() != BlockDef::GetBlockDefIDByName("air"))
		{
			return localZ;  
		}
	}

	return -1;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::CanBeLoadedFromFile()
{
	std::string fileName = GetChunkFileName();
	return DoesFileExist(fileName);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
std::string Chunk::GetChunkFileName()
{
	return Stringf("Saves/World_%u/Chunk(%d,%d).chunk", m_worldSeed, m_chunkCoords.x, m_chunkCoords.y);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::LoadBlocksFromFile()
{
	std::string filePath = Stringf("Saves/World_%u/Chunk(%d,%d).chunk", m_worldSeed, m_chunkCoords.x, m_chunkCoords.y);

	if (DoesFileExist(filePath))
	{
		std::vector<uint8_t> buffer;
		FileReadToBuffer(buffer, filePath);
		if (buffer[0] == 'G' && buffer[1] == 'C' && buffer[2] == 'H' && buffer[3] == 'K' &&
			buffer[4] == 1 && buffer[5] == CHUNK_BITS_X && buffer[6] == CHUNK_BITS_Y && buffer[7] == CHUNK_BITS_Z)
		{
			unsigned int seedInFile;
			memcpy(&seedInFile, &buffer[8], sizeof(unsigned int));
			if (seedInFile != m_worldSeed)
			{
				ERROR_AND_DIE(Stringf("Loaded file world seed does not match for chunk (%d, %d)", m_chunkCoords.x, m_chunkCoords.y));
				return false;
			}

			int blockIndex = 0;
			for (int i = 12; i < buffer.size(); i += 2)
			{
				uint8_t blockTypeIndex = static_cast<int>(buffer[i]);
				int numberOfBlocks = static_cast<int>(buffer[i + 1]);
				for (int j = 0; j < numberOfBlocks; j++)
				{
					m_blocks[blockIndex].SetTypeID(blockTypeIndex);
					blockIndex++;
				}
			}
			return true;
		}
		else
		{
			ERROR_AND_DIE(Stringf("Loaded file signature did not match for chunk (%d, %d)", m_chunkCoords.x, m_chunkCoords.y));
			return false;
		}
	}
	return false;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::SaveBlockToFile()
{
	std::vector<uint8_t> buffer;
	buffer.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Y);
	buffer.push_back('G');
	buffer.push_back('C');
	buffer.push_back('H');
	buffer.push_back('K');
	buffer.push_back(1);
	buffer.push_back(CHUNK_BITS_X);
	buffer.push_back(CHUNK_BITS_Y);
	buffer.push_back(CHUNK_BITS_Z);

	// Add seed to the buffer
	for (int i = 0; i < sizeof(unsigned int); i++)
	{
		buffer.push_back(reinterpret_cast<uint8_t*>(&m_worldSeed)[i]);
	}

	uint8_t currentBlockType = m_blocks[0].GetTypeID();
	uint8_t currentBlockCount = 1;

	for (int blockIndex = 1; blockIndex < CHUNK_BLOCKS_TOTAL; blockIndex++)
	{
		uint8_t type = m_blocks[blockIndex].GetTypeID();

		if (currentBlockType == type)
		{
			if (currentBlockCount == 255)
			{
				buffer.push_back(currentBlockType);
				buffer.push_back(currentBlockCount);
				currentBlockCount = 0;
			}
			currentBlockCount++;
		}

		else
		{
			buffer.push_back(currentBlockType);
			buffer.push_back(currentBlockCount);
			currentBlockType = type;
			currentBlockCount = 1;
		}
	}

	buffer.push_back(currentBlockType);
	buffer.push_back(currentBlockCount);

	std::string folderPath = Stringf("Saves\\World_%u", m_worldSeed);
	if (!DoesFileExist(folderPath))
	{
		ForceCreateWorldFolder();
	}
	std::string filePath = Stringf("%s\\Chunk(%d,%d).chunk", folderPath.c_str(), m_chunkCoords.x, m_chunkCoords.y);
	FileWriteFromBuffer(buffer, filePath);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::DisconnectFromNeighbors()
{
	if (m_northNeighbor != nullptr)
	{
		m_northNeighbor->m_southNeighbor = nullptr;
		m_northNeighbor = nullptr;
	}

	if (m_southNeighbor != nullptr)
	{
		m_southNeighbor->m_northNeighbor = nullptr;
		m_southNeighbor = nullptr;
	}

	if (m_eastNeighbor != nullptr)
	{
		m_eastNeighbor->m_westNeighbor = nullptr;
		m_eastNeighbor = nullptr;
	}

	if (m_westNeighbor != nullptr)
	{
		m_westNeighbor->m_eastNeighbor = nullptr;
		m_westNeighbor = nullptr;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::AddVertsForBlock(std::vector<Vertex_PCU>& verts, int localX, int localY, int localZ)
{
	int blockIndex = GetBlockIndex(localX, localY, localZ);
	Block const* block = m_blocks + blockIndex;
	BlockDef const& blockDef = BlockDef::GetBlockDefByID(block->GetTypeID());
	IntVec3 localCoords = GetLocalCoordsFromBlockIndex(blockIndex);
	BlockIterator blockIter(this, GetBlockIndexFromLocalCoords(localCoords));
	
	if (blockDef.m_isVisible)
	{
		// Adding world translation for chunk
		Vec3 worldOffset(m_worldBounds.m_mins.x, m_worldBounds.m_mins.y, m_worldBounds.m_mins.z);
		Vec3 mins = Vec3((float)localX + worldOffset.x, (float)localY + worldOffset.y, (float)localZ + worldOffset.z);
		Vec3 maxs((float)localX + 1.f + worldOffset.x, (float)localY + 1.f + worldOffset.y, (float)localZ + 1.f + worldOffset.z);

		bool shouldApplyHSR = !m_world->m_debugDisableHSR;

		// +x face (East)
		if (!shouldApplyHSR || !BlockDef::IsBlockTypeOpaque(blockIter.GetEastNeighbour().GetBlock()->GetTypeID()))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetEastNeighbour());
			AddVertsForQuad3D(verts,
				Vec3(maxs.x, mins.y, mins.z), Vec3(maxs.x, maxs.y, mins.z),
				Vec3(maxs.x, maxs.y, maxs.z), Vec3(maxs.x, mins.y, maxs.z),
				tileColor, blockDef.m_uvsSides);
		}

		// -x face (West)
		if (!shouldApplyHSR || !BlockDef::IsBlockTypeOpaque(blockIter.GetWestNeighbour().GetBlock()->GetTypeID()))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetWestNeighbour());
			AddVertsForQuad3D(verts,
				Vec3(mins.x, maxs.y, mins.z), Vec3(mins.x, mins.y, mins.z),
				Vec3(mins.x, mins.y, maxs.z), Vec3(mins.x, maxs.y, maxs.z),
				tileColor, blockDef.m_uvsSides);
		}

		// +y face(North)
		if (!shouldApplyHSR || !BlockDef::IsBlockTypeOpaque(blockIter.GetNorthNeighbour().GetBlock()->GetTypeID()))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetNorthNeighbour());
			AddVertsForQuad3D(verts,
				Vec3(maxs.x, maxs.y, mins.z), Vec3(mins.x, maxs.y, mins.z),
				Vec3(mins.x, maxs.y, maxs.z), Vec3(maxs.x, maxs.y, maxs.z),
				tileColor, blockDef.m_uvsSides);
		}

		// -y face(South)
		if (!shouldApplyHSR || !BlockDef::IsBlockTypeOpaque(blockIter.GetSouthNeighbour().GetBlock()->GetTypeID()))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetSouthNeighbour());
			AddVertsForQuad3D(verts,
				Vec3(mins.x, mins.y, mins.z), Vec3(maxs.x, mins.y, mins.z),
				Vec3(maxs.x, mins.y, maxs.z), Vec3(mins.x, mins.y, maxs.z),
				tileColor, blockDef.m_uvsSides);
		}

		// +z face(Top)
		if (!shouldApplyHSR || !BlockDef::IsBlockTypeOpaque(blockIter.GetAboveNeighbour().GetBlock()->GetTypeID()))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetAboveNeighbour());
			AddVertsForQuad3D(verts,
				Vec3(mins.x, mins.y, maxs.z), Vec3(maxs.x, mins.y, maxs.z),
				Vec3(maxs.x, maxs.y, maxs.z), Vec3(mins.x, maxs.y, maxs.z),
				tileColor, blockDef.m_uvsTop);
		}

		// -z face(Bottom)
		if (!shouldApplyHSR || !BlockDef::IsBlockTypeOpaque(blockIter.GetBelowNeighbour().GetBlock()->GetTypeID()))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetBelowNeighbour());
			AddVertsForQuad3D(verts,
				Vec3(maxs.x, mins.y, mins.z), Vec3(mins.x, mins.y, mins.z),
				Vec3(mins.x, maxs.y, mins.z), Vec3(maxs.x, maxs.y, mins.z),
				tileColor, blockDef.m_uvsBottom);
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::HasAllValidNeighbours() const
{
	return m_northNeighbor && m_eastNeighbor && m_southNeighbor && m_westNeighbor;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::ProcessLightingForDugBlock(const BlockIterator& blockIter)
{
	m_world->MarkLightingDirty(blockIter);
	IntVec3 localCoords = GetLocalCoordsFromBlockIndex(blockIter.m_blockIndex);
	bool isSkyBlock = blockIter.GetAboveNeighbour().GetBlock()->IsBlockSky();
	if (isSkyBlock)
	{
		BlockIterator neighbour = blockIter;
		while (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			neighbour.GetBlock()->SetIsBlockSky(true);
			m_world->MarkLightingDirty(neighbour);
			neighbour = neighbour.GetBelowNeighbour();
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::ProcessLightingForAddedBlock(const BlockIterator& blockIter)
{
	Block* block = blockIter.GetBlock();
	m_world->MarkLightingDirty(blockIter);
	if (block->IsBlockSky() && BlockDef::IsBlockTypeOpaque(block->GetTypeID()))
	{
		block->SetIsBlockSky(false);
		BlockIterator neighbour = blockIter.GetBelowNeighbour();
		while (!BlockDef::IsBlockTypeOpaque(neighbour.GetBlock()->GetTypeID()))
		{
			neighbour.GetBlock()->SetIsBlockSky(false);
			m_world->MarkLightingDirty(neighbour);
			neighbour = neighbour.GetBelowNeighbour();
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Rgba8 Chunk::GetFaceColor(const BlockIterator& blockIterator)
{
	Rgba8 color;
	Block* block = blockIterator.GetBlock();
	if (!block)
	{
		color = Rgba8(255, 255, 255, 255); 
	}
	else
	{
		color.r = unsigned char(RangeMap(block->GetOutdoorLightInfluence(), 0.f, 15.f, 0.f, 255.f));
		color.g = unsigned char(RangeMap(block->GetIndoorLightInfluence(), 0.f, 15.f, 0.f, 255.f));
		color.b = 127;
		color.a = 255;
	}
	return color;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::InitializeLighting()
{
	//mark non opaque blocks at the edge of a chunk as dirty
	for (int i = 0; i < CHUNK_BLOCKS_TOTAL; i++)
	{
		Block& block = m_blocks[i];
		if (!BlockDef::IsBlockTypeOpaque(block.GetTypeID()))
		{
			IntVec3 localCoords = GetLocalCoordsFromBlockIndex(i);
			if (localCoords.x == 0 || localCoords.x == CHUNK_MAX_X ||
				localCoords.y == 0 || localCoords.y == CHUNK_MAX_Y ||
				localCoords.z == 0 || localCoords.z == CHUNK_MAX_Z)
			{
				BlockIterator iter(this, i);
				m_world->MarkLightingDirty(iter);
			}
		}
	}

	//make blocks as sky which are in direct line of sight of the sky
	for (int y = 0; y < CHUNK_SIZE_Y; y++)
	{
		for (int x = 0; x < CHUNK_SIZE_X; x++)
		{
			int z = CHUNK_MAX_Z;
			int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
			Block* block = &m_blocks[blockIndex];
			if (block)
			{
				while (z >= 0 && !BlockDef::IsBlockTypeOpaque(block->GetTypeID()))
				{
					block->SetIsBlockSky(true);
					z--;
					if (z >= 0)
					{
						blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
						block = &m_blocks[blockIndex];
					}
				}
			}
		}
	}

	//set each sky blocks outdoor lighting influence to max and dirty it's neighboring blocks
	for (int y = 0; y < CHUNK_SIZE_Y; y++)
	{
		for (int x = 0; x < CHUNK_SIZE_X; x++)
		{
			int z = CHUNK_MAX_Z;
			int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
			Block* block = &m_blocks[blockIndex];
			if (block)
			{
				while (z>= 0 && !BlockDef::IsBlockTypeOpaque(block->GetTypeID()))
				{
					block->SetOutdoorLightInfluence(15);

					BlockIterator blockIter( this, blockIndex );
					m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetNorthNeighbour());
					m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetEastNeighbour());
					m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetSouthNeighbour());
					m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetWestNeighbour());
					z--;
					if (z >= 0)
					{
						blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
						block = &m_blocks[blockIndex];
					}
				}

			}
		}
	}

	for (int i = 0; i < CHUNK_BLOCKS_TOTAL; i++)
	{
		Block& block = m_blocks[i];
		if (BlockDef::DoesBlockTypeEmitLight(block.GetTypeID()))
		{
			BlockIterator iter(this, i );
			m_world->MarkLightingDirty(iter);
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::DigBlock(const BlockIterator& blockIter)
{
	m_isChunkDirty = true;
	m_needsSaving = true;
	blockIter.GetWestNeighbour().m_chunk->SetChunkToDirty();
	blockIter.GetEastNeighbour().m_chunk->SetChunkToDirty();
	blockIter.GetSouthNeighbour().m_chunk->SetChunkToDirty();
	blockIter.GetNorthNeighbour().m_chunk->SetChunkToDirty();
	ProcessLightingForDugBlock(blockIter);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::PlaceBlock(const BlockIterator& blockIter)
{
	int blockIndex = blockIter.m_blockIndex;
	m_blocks[blockIndex].SetTypeID(static_cast<uint8_t>(m_world->m_blockTypeToAdd));
	m_isChunkDirty = true;
	m_needsSaving = true;
	ProcessLightingForAddedBlock(blockIter);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::SpawnBlockTemplate(std::string const& templateName, IntVec3 const& templateLocalCoords)
{
	IntVec3 placementCoords = templateLocalCoords;
	placementCoords.z += 1;

	BlockTemplate const* blockTemplate = BlockTemplate::GetBlockTemplateByName(templateName.c_str());
	GUARANTEE_OR_DIE(blockTemplate != nullptr, Stringf("No such block templated named %s", templateName.c_str()));
	for (int i = 0; i < blockTemplate->m_blockTemplateEntries.size(); i++)
	{
		BlockTemplateEntry const& blockToSpawn = blockTemplate->m_blockTemplateEntries[i];
		IntVec3 blockLocalCoords = placementCoords + blockToSpawn.m_offset;
		SetBlockTypeID(blockLocalCoords, blockToSpawn.m_blockType);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::AddCaves(unsigned int worldCaveSeed)
{
	//Set various cave generation constants
  	constexpr float CHANCE_FOR_CAVE_TO_START_IN_A_CHUNK = 0.01f;
  	constexpr float CAVE_MAX_DISTANCE_BLOCKS = 200.f;
  	constexpr float CAVE_STEP_LENGTH = 10.f;
  	constexpr float CAVE_MAX_TURN_DEGREES = 30.f;
  	constexpr float CAVE_MAX_DIVE_DEGREES = 10.f;
  	constexpr int   MAX_CAVE_STEPS = 50;
  	
  	//Calculate how big of a search area we will need to check chunks in order to detect for caves
  	constexpr int CHUNK_WIDTH = (CHUNK_SIZE_X < CHUNK_SIZE_Y ? CHUNK_SIZE_X : CHUNK_SIZE_Y);
  	constexpr int CAVE_SEARCH_RADIUS = 1 + int(CAVE_MAX_DISTANCE_BLOCKS / CHUNK_WIDTH);
  
  	//Search all chunks in a large rectangular region around us for possible cave start locations
  	
  	IntVec2 chunkSearchMins = m_chunkCoords - IntVec2((CAVE_SEARCH_RADIUS), (CAVE_SEARCH_RADIUS));
  	IntVec2 chunkSearchMaxs = m_chunkCoords + IntVec2(CAVE_SEARCH_RADIUS, CAVE_SEARCH_RADIUS);
  	for (int chunkY = chunkSearchMins.y; chunkY <= chunkSearchMaxs.y; chunkY++)
  	{
  		for (int chunkX = chunkSearchMins.x; chunkX <= chunkSearchMaxs.x; chunkX++)
  		{
  			float caveOriginationNoise = Get2dNoiseZeroToOne(chunkX, chunkY, worldCaveSeed);
  			if (caveOriginationNoise < CHANCE_FOR_CAVE_TO_START_IN_A_CHUNK)
  			{
  				//make a note that "a cave definitely starts somewhere in a chunk"
  				m_nearbyCaves.push_back( CaveInfo(IntVec2(chunkX, chunkY)) );
  			}
  		}
  	}
  
  	//For each cave originating nearby, determine its eaxct start location, then start wandering
  	for (int i = 0; i < int(m_nearbyCaves.size()); i++)
  	{
  		//Create a (mostly) unique seed for THIS CAVE, and create a private RNG based on that seed
  		CaveInfo& cave = m_nearbyCaves[i];   //ToDo#come up with a seed that is consistent for this cave
  		unsigned int seedForThisCave = Get2dNoiseUint(cave.m_startChunkCoords.x, cave.m_startChunkCoords.y, worldCaveSeed + i);
  		RandomNumberGenerator caveRng(seedForThisCave);
  		
  		//Pick a random starting position withing the cave's starting chunk (which is prolly not me)
  		AABB3 startChunkBounds = GetChunkBoundsForChunkCoords(cave.m_startChunkCoords);
  		Vec3 crawlWorldPosition;
  		cave.m_startWorldPos.x = caveRng.RollRandomFloatInRange(startChunkBounds.m_mins.x, startChunkBounds.m_maxs.x);
  		cave.m_startWorldPos.y = caveRng.RollRandomFloatInRange(startChunkBounds.m_mins.y, startChunkBounds.m_maxs.y);
  		cave.m_startWorldPos.z = caveRng.RollRandomFloatInRange(30.f, 50.f);
  		EulerAngles crawlOrientation;
  		crawlOrientation.m_yawDegrees = caveRng.RollRandomFloatInRange(0.f, 360.f);
  		crawlOrientation.m_pitchDegrees = 0.f;
  		crawlOrientation.m_rollDegrees = 0.f;
 
  
  		//Note the starting pos, then start crawling
  		Vec3 crawlWorldPos = cave.m_startWorldPos;
  		for (int crawlStep = 0; crawlStep < 25; crawlStep++)
  		{
  			//Vec3 forwardDisplacement = Vec3(Vec2::MakeFromPolarDegrees(yawDegrees, CAVE_STEP_LENGTH));
  			Vec3 forwardDisplacement = CAVE_STEP_LENGTH * crawlOrientation.GetForwardVector();
  			crawlWorldPosition += forwardDisplacement;
			crawlOrientation.m_yawDegrees += caveRng.RollRandomFloatInRange(-CAVE_MAX_TURN_DEGREES, CAVE_MAX_TURN_DEGREES);
			float noise = Compute1dPerlinNoise(float(crawlStep), 5.f, 3, 0.5f, 2.f, true, seedForThisCave + 5);
			crawlOrientation.m_pitchDegrees = RangeMapClamped(noise, -1.f, 1.f, -89.f, 89.f);
  			cave.m_caveNodePositions.push_back(crawlWorldPosition);
  		}
  	}


	for (int caveIndex = 0; caveIndex < m_nearbyCaves.size(); caveIndex++)
	{
		for (int caveNodeIndex = 0; caveNodeIndex < m_nearbyCaves[caveIndex].m_caveNodePositions.size(); caveNodeIndex++)
		{
			if (caveNodeIndex == 0)
			{
				CarveCapsule3D(m_nearbyCaves[caveIndex].m_startWorldPos, m_nearbyCaves[caveIndex].m_caveNodePositions[caveNodeIndex], 5.f);
			}
			else
			{
				CarveCapsule3D(m_nearbyCaves[caveIndex].m_caveNodePositions[caveNodeIndex - 1], m_nearbyCaves[caveIndex].m_caveNodePositions[caveNodeIndex], 5.f);
			}
		}
	}
	//CarveAABB3D(Vec3(10.f, 10.f, 20.f), Vec3(12.f, 7.f, 4.f));
// 	Vec3 a(-30.f, -40.f, 8.f);
// 	Vec3 b(70.f, 10.f, 30.f);
// 	Vec3 c(90.f, 30.f, 35.f);
// 	Vec3 d(120.f, 40.f, 25.f);
// 	CarveCapsule3D(a, b, 5.f);
// 	CarveCapsule3D(b, c, 5.f);
// 	CarveCapsule3D(c, d, 5.f);
	//Get a cave noise values based on chunk coords for each chunk in a huge region around me
	//Each chunk with a noise values > same threshold has acave that STARTS in that chunk
	//Each cave will also need a mostly unique seed number for its RNG to make it diff.
	//Each cave's RNG seed will be based on raw noise of its chunk coords
	//Generate each carve, carving blocks as it goes; only MY blocks will actually be carved
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::ForceCreateWorldFolder()
{
	std::string command = Stringf("mkdir Saves\\World_%u", m_worldSeed);
	system(command.c_str());
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::CarveAABB3D(Vec3 worldCenter, Vec3 halfDimensions)
{
	static BlockDefID air = BlockDef::GetBlockDefIDByName("air");
	Vec3 chunkMins = m_worldBounds.m_mins;
	AABB3 carveWorldBounds(worldCenter - halfDimensions, worldCenter + halfDimensions);
	AABB3 carveLocalBounds(carveWorldBounds.m_mins - chunkMins, carveWorldBounds.m_maxs - chunkMins);
	int minLocalX = RoundDownToInt(carveLocalBounds.m_mins.x);
	int maxLocalX = RoundDownToInt(carveLocalBounds.m_maxs.x);
	int minLocalY = RoundDownToInt(carveLocalBounds.m_mins.y);
	int maxLocalY = RoundDownToInt(carveLocalBounds.m_maxs.y);
	int minLocalZ = RoundDownToInt(carveLocalBounds.m_mins.z);
	int maxLocalZ = RoundDownToInt(carveLocalBounds.m_maxs.z);

	for (int localZ = minLocalZ; localZ <= maxLocalZ; localZ++)
	{
		if (localZ < 0 || localZ > CHUNK_MAX_Z)
		{
			continue;
		}
		for (int localY = minLocalY; localY <= maxLocalY; localY++)
		{
			if (localY < 0 || localY > CHUNK_MAX_Y)
			{
				continue;
			}

			for (int localX = minLocalX; localX <= maxLocalX; localX++)
			{
				if (localX < 0 || localX > CHUNK_MAX_X)
				{
					continue;
				}

				SetBlockTypeID(IntVec3(localX, localY, localZ), air);
			}
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::CarveCapsule3D(Vec3 capsuleWorldStart, Vec3 capsuleWorldEnd, float capsuleRadius)
{
	static BlockDefID air = BlockDef::GetBlockDefIDByName("air");
	float minX = std::min(capsuleWorldStart.x, capsuleWorldEnd.x) - capsuleRadius;
	float maxX = std::max(capsuleWorldStart.x, capsuleWorldEnd.x) + capsuleRadius;

	float minY = std::min(capsuleWorldStart.y, capsuleWorldEnd.y) - capsuleRadius;
	float maxY = std::max(capsuleWorldStart.y, capsuleWorldEnd.y) + capsuleRadius;

	float minZ = std::min(capsuleWorldStart.z, capsuleWorldEnd.z) - capsuleRadius;
	float maxZ = std::max(capsuleWorldStart.z, capsuleWorldEnd.z) + capsuleRadius;

	Vec3 chunkMins = m_worldBounds.m_mins;
	AABB3 capsuleWorldBounds(minX, minY, minZ, maxX, maxY, maxZ);
	AABB3 capsuleLocalBounds(capsuleWorldBounds.m_mins - chunkMins, capsuleWorldBounds.m_maxs - chunkMins);
	if (!DoAAB3DsOverlap(capsuleWorldBounds, m_worldBounds))
	{
		return;
	}
	int minLocalX = RoundDownToInt(capsuleLocalBounds.m_mins.x);
	int maxLocalX = RoundDownToInt(capsuleLocalBounds.m_maxs.x);
	int minLocalY = RoundDownToInt(capsuleLocalBounds.m_mins.y);
	int maxLocalY = RoundDownToInt(capsuleLocalBounds.m_maxs.y);
	int minLocalZ = RoundDownToInt(capsuleLocalBounds.m_mins.z);
	int maxLocalZ = RoundDownToInt(capsuleLocalBounds.m_maxs.z);

	//For loop through all blocks in our AABB3 bounding region
	for (int localZ = minLocalZ; localZ <= maxLocalZ; localZ++)
	{
		if (localZ < 0 || localZ > CHUNK_MAX_Z)
		{
			continue;
		}
		float blockCenterWorldZ = chunkMins.z + float(localZ) + 0.5f;
		for (int localY = minLocalY; localY <= maxLocalY; localY++)
		{
			if (localY < 0 || localY > CHUNK_MAX_Y)
			{
				continue;
			}
			float blockCenterWorldY = chunkMins.y + float(localY) + 0.5f;
			for (int localX = minLocalX; localX <= maxLocalX; localX++)
			{
				if (localX < 0 || localX > CHUNK_MAX_X)
				{
					continue;
				}
				float blockCenterWorldX = chunkMins.x + float(localX) + 0.5f;
				Vec3 blockWorldCenter(blockCenterWorldX, blockCenterWorldY, blockCenterWorldZ);
				if (IsPointInsideCapsule3D(blockWorldCenter, capsuleWorldStart, capsuleWorldEnd, capsuleRadius))
				{
					
					
						SetBlockTypeID(IntVec3(localX, localY, localZ), air);
					
				}
			}
		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Vec3 Chunk::GetChunkCenter()
{
	int centerX = (m_chunkCoords.x * CHUNK_SIZE_X) + CHUNK_SIZE_X / 2;
	int centerY = (m_chunkCoords.y * CHUNK_SIZE_Y) + CHUNK_SIZE_Y / 2;
	int centerZ = CHUNK_SIZE_Z / 2;

	return Vec3(float(centerX), float(centerY), float(centerZ));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
int Chunk::CalculateGroundZHeightForGlobalXY(float globalX, float globalY)
{
	int riverDepth = 6;
	int oceanHeightZ = CHUNK_SIZE_Z / 2;
	int lowestOceanFloorZ = CHUNK_SIZE_Z / 4;
	int maxOceanLowering = oceanHeightZ - lowestOceanFloorZ;

	int   groundHeightZ = 0;
	float oceanness = 0.f;
	float hilliness = 0.f;
	
	oceanness = 0.5f + 0.5f * Compute2dPerlinNoise(globalX, globalY, 800.f, 7, 0.5f, 2.f, true, m_worldSeed + 3);
	oceanness = SmoothStep3(SmoothStep3(oceanness));
	hilliness = 0.5f + 0.5f * Compute2dPerlinNoise(globalX, globalY, 400.f, 5, 0.5f, 2.f, true, m_worldSeed + 4);
	hilliness = SmoothStep3(SmoothStep3(hilliness));		//smooth step makes the extreme more extremey.

	//calculate terrain height, considering oceanness(lowers land and hilliness(exaggerates height changes))
	int mountainMaxHeight = CHUNK_SIZE_Z - oceanHeightZ + riverDepth;
	float mountainHeight = hilliness * float(mountainMaxHeight);
	float heightNoise = fabsf(Compute2dPerlinNoise(globalX, globalY, 200.f, 7, 0.5f, 2.f, true, m_worldSeed));
	groundHeightZ = oceanHeightZ - riverDepth + int(mountainHeight * heightNoise);

	//Lower terrain where oceanness is high(and do the transition or lerp)
	float oceanLoweringEffectStrength = RangeMapClamped(oceanness, 0.5f, 1.f, 0.f, 1.f);
	int oceanLoweringAmount = int(oceanLoweringEffectStrength * float(maxOceanLowering));
	groundHeightZ -= oceanLoweringAmount;

	return groundHeightZ;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::AddTrees()
{
	//int maxSandDepth = 8;
	int oceanHeightZ = CHUNK_SIZE_Z / 2;
	std::vector<IntVec3> treeSpawnLocalCoordsList;

	//Create a larger grid of tree noise that extends into neighbors
	constexpr int MAX_TREE_RADIUS = 3;
	constexpr int TREE_GRID_SIZE_X = CHUNK_SIZE_X + (MAX_TREE_RADIUS * 2) + 1 + 1;
	constexpr int TREE_GRID_SIZE_Y = CHUNK_SIZE_Y + (MAX_TREE_RADIUS * 2) + 1 + 1;
	constexpr int TREE_GRID_COUNT = TREE_GRID_SIZE_X * TREE_GRID_SIZE_Y;
	float treeNoise[TREE_GRID_COUNT] = {};

	for (int treeY = 0; treeY < TREE_GRID_SIZE_Y; treeY++)
	{
		for (int treeX = 0; treeX < TREE_GRID_SIZE_X; treeX++)
		{
			int chunkMinsGlobalX = m_chunkCoords.x * CHUNK_SIZE_X;
			int chunkMinsGlobalY = m_chunkCoords.y * CHUNK_SIZE_Y;
			int treeSpaceMinsGlobalX = chunkMinsGlobalX - MAX_TREE_RADIUS - 1;
			int treeSpaceMinsGlobalY = chunkMinsGlobalY - MAX_TREE_RADIUS - 1;
			int globalX = treeSpaceMinsGlobalX + treeX;
			int globalY = treeSpaceMinsGlobalY + treeY;
			float treeNoiseHere = Get2dNoiseZeroToOne(globalX, globalY, m_worldSeed + 5);
			int treeNoiseIndex = treeX + (treeY * TREE_GRID_SIZE_X);
			treeNoise[treeNoiseIndex] = treeNoiseHere;
		}
	}

	std::vector<IntVec2> treeLocalXYs;

	//Scan through the tree noise grid looking for values that are greater than their neighbors
	for (int treeY = 1; treeY < TREE_GRID_SIZE_Y - 1; treeY++)
	{
		for (int treeX = 1; treeX < TREE_GRID_SIZE_X - 1; treeX++)
		{
			int globalX = (m_chunkCoords.x * CHUNK_SIZE_X) + treeX - TREE_GRID_SIZE_X - 1;
			int globalY = (m_chunkCoords.y * CHUNK_SIZE_Y) + treeY - TREE_GRID_SIZE_Y - 1;
			float forestness = 0.5f + 0.5f * Compute2dPerlinNoise(static_cast<float>(globalX), static_cast<float>(globalY), 200.f, 10, 0.5f, 2.f, true, m_worldSeed + 6);
			float minimumTreeNoiseToSpawnHere = RangeMapClamped(forestness, 0.75f, 1.f, 1.00f, 0.5f);
			int myTreeNoiseIndex = treeX + (treeY * TREE_GRID_SIZE_X);
			float myTreeNoise = treeNoise[myTreeNoiseIndex];
			if (myTreeNoise < minimumTreeNoiseToSpawnHere)
			{
				continue;
			}

			//Tree x, Tree y is what we are checking for "is this a tree?"
			//But for that, we need to check if the tree noise value at 
			// (treeX,treeY) is greater than the 8 neighboring chunks
			//So we for loop through the 8 neighbors
			//If any of them has a tree noise equal to greater than ours, just stop(no tree)
			bool isHighest = true;
			for (int neighborTreeY = treeY - 1; neighborTreeY <= treeY + 1; neighborTreeY++)
			{
				for (int neighborTreeX = treeX - 1; neighborTreeX <= treeX + 1; neighborTreeX++)
				{
					if (neighborTreeX == treeX && neighborTreeY == treeY)
					{
						continue;
					}

					int neighborTreeNoiseIndex = neighborTreeX + (neighborTreeY * TREE_GRID_SIZE_X);
					if (treeNoise[neighborTreeNoiseIndex] >= myTreeNoise)
					{
						isHighest = false;
						break;
					}
				}
				if (!isHighest)
				{
					break;
				}

			}
			if (isHighest)
			{
				int localX = treeX - MAX_TREE_RADIUS - 1;  //eg treeX = 2 means localX = -1.
				int localY = treeY - MAX_TREE_RADIUS - 1;
				treeLocalXYs.push_back(IntVec2(localX, localY));
			}
		}
	}
	for (int localXY = 0; localXY < treeLocalXYs.size(); localXY++)
	{
		int localX = treeLocalXYs[localXY].x;
		int localY = treeLocalXYs[localXY].y;

		float globalX = m_worldBounds.m_mins.x + float(localX);
		float globalY = m_worldBounds.m_mins.y + float(localY);

		int groundHeight = CalculateGroundZHeightForGlobalXY(globalX, globalY);

		if (groundHeight > oceanHeightZ)
		{
			treeSpawnLocalCoordsList.push_back(IntVec3(localX, localY, groundHeight));
		}
	}
	for (int i = 0; i < int(treeSpawnLocalCoordsList.size()); i++)
	{
		
		IntVec3 localSpawnPos = treeSpawnLocalCoordsList[i];

		// get the temperature and humidity at the current spawn position
// 		float globalX = m_worldBounds.m_mins.x + float(localSpawnPos.x);
// 		float globalY = m_worldBounds.m_mins.y + float(localSpawnPos.y);

// 		float temperature = 0.5f + 0.5f * Compute2dPerlinNoise(globalX, globalY, 800.f, 9, 0.5f, 2.f, true, m_worldSeed + 1);
// 		float humidity = 0.5f + 0.5f * Compute2dPerlinNoise(globalX, globalY, 800.f, 5, 0.5f, 2.f, true, m_worldSeed + 2);
// 
// 		// get the sand depth at the current spawn position
// 		//int sandDepth = RoundDownToInt(RangeMapClamped(humidity, 0.f, 0.35f, float(maxSandDepth), 0.f));
// 
// 		// determine the type of tree to spawn based on the temperature and humidity
// 		if (temperature < 0.4f) // for low temperatures, spawn spruce trees
// 		{
// 			SpawnBlockTemplate("spruce_tree", localSpawnPos);
// 		}
// 		else if (humidity < 0.65f) // if there is sand (sandDepth > 0), it is a desert, spawn cacti
// 		{
// 			SpawnBlockTemplate("cactus", localSpawnPos);
// 		}
		
		SpawnBlockTemplate("oak_tree", localSpawnPos);

	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::GenerateCaves()
{
	std::map<IntVec2, float> startPerlinNoise;
	GetCavesStartPerlinNoise(startPerlinNoise);
	int diameter = m_caveCheckRadius * 2;

	IntVec2 startCoords = m_chunkCoords - IntVec2(m_caveCheckRadius, m_caveCheckRadius);


	for (int yOffset = 0; yOffset < diameter; yOffset++) 
	{
		for (int xOffset = 0; xOffset < diameter; xOffset++) 
		{
			IntVec2 resultingCoords = startCoords + IntVec2(xOffset, yOffset);

			bool isCaveMaxima = AreCoordsConsideredLocalMaxima(resultingCoords, m_caveCheckRadius, startPerlinNoise);
			if (isCaveMaxima)
			{
				std::vector<IntVec3> cavePoints;
				cavePoints.reserve(m_caveNodeAmount);

				GetCavePath(resultingCoords, cavePoints);
				CarveCavePath(cavePoints);

			}

		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::GetCavesStartPerlinNoise(std::map<IntVec2, float>& perlinNoiseHolder) 
{
	int furthestRadio = m_caveCheckRadius * 2; // I have to check on the grids corner a radius of chunks as well
	int diameter = furthestRadio * 8;

	IntVec2 startCoords = m_chunkCoords - IntVec2(furthestRadio, furthestRadio);

	for (int yOffset = 0; yOffset < diameter; yOffset++) 
	{
		for (int xOffset = 0; xOffset < diameter; xOffset++) 
		{
			IntVec2 resultingCoords = startCoords + IntVec2(xOffset, yOffset);
			float perlinNoise = 0.5f + 0.5f * Compute2dPerlinNoise((float)resultingCoords.x, (float)resultingCoords.y, 0.35f, 7, 0.9f, 20.0f, true, m_worldSeed + 25); // Good for caves

			perlinNoiseHolder[resultingCoords] = perlinNoise;

		}
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::GetCavePath(IntVec2 const& coords, std::vector<IntVec3>& cavePoints) 
{
	Vec2 chunkCenter2D = GetChunkCenterXYForChunkCoords(coords);
	IntVec2 chunkCenterBlockCoords = IntVec2(RoundDownToInt(chunkCenter2D.x), RoundDownToInt(chunkCenter2D.y));

	Vec3 currentPos = Vec3(chunkCenter2D.x, chunkCenter2D.y, (float)(CHUNK_SIZE_Z / 2));

	IntVec3 currentCoords = IntVec3(chunkCenterBlockCoords.x, chunkCenterBlockCoords.y, 0);
	int z = CalculateGroundZHeightForGlobalXY(chunkCenter2D.x, chunkCenter2D.y);
	currentCoords.z = z - m_caveDepthStart;
	currentPos.z = (float)currentCoords.z;

	float currentYaw = 0.0f;

	cavePoints.push_back(currentCoords);

	for (int nodeStep = 0; nodeStep < m_caveNodeAmount; nodeStep++) 
	{

		float yawTurnNoise = Compute2dPerlinNoise((float)currentCoords.x, (float)currentCoords.y, 20.0f, 2, 0.4f, 2.0f, true, m_worldSeed + 10);
		float pitch = 89.9f * Compute2dPerlinNoise((float)currentCoords.x, (float)currentCoords.y, 20.0f, 4, 0.4f, 2.0f, true, m_worldSeed + 11);

		float yawTurnChange = RangeMapClamped(yawTurnNoise, -1.0f, 1.0f, -m_caveTurnRate, m_caveTurnRate);

		currentYaw += yawTurnChange;

		EulerAngles dir(currentYaw, pitch, 0.0f);
		Vec3 currentFwd = dir.GetForwardVector();

		currentPos += currentFwd * (float)m_caveBlockSteps;

		currentCoords.x = RoundDownToInt(currentPos.x);
		currentCoords.y = RoundDownToInt(currentPos.y);
		currentCoords.z = RoundDownToInt(currentPos.z);

		currentPos.x = (float)currentCoords.x;
		currentPos.y = (float)currentCoords.y;
		currentPos.z = (float)currentCoords.z;

		cavePoints.push_back(currentCoords);

	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::CarveCavePath(std::vector<IntVec3>& cavePoints)
{

	for (int cavePointInd = 0; cavePointInd < cavePoints.size(); cavePointInd++) 
	{
		IntVec3 const& coords = cavePoints[cavePointInd];
		IntVec3 localCoords = GetLocalCoordsForGlobalCoords(coords);
		localCoords.z += 1;

		Vec3 const blockCenter = Vec3(coords.x + 0.5f, coords.y + 0.5f, coords.z + 0.5f);
		float radiusNoise = 0.5f + 0.5f * Compute2dPerlinNoise((float)coords.x, (float)coords.y, 2.0f, 5, 0.65f, 2.0f, true, m_worldSeed + 11);
		float radius = RangeMap(radiusNoise, 0.0f, 1.0f, m_caveMaxRadius - 1.0f, m_caveMaxRadius);

		CarveBlockInRadius(blockCenter, localCoords, radius, false, true);



	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Chunk::CarveBlockInRadius(Vec3 const& originPos, IntVec3 const& localCoords, float radius, bool replaceWithDirt, bool placeLampInMiddle)
{

	int radiusAsInt = RoundDownToInt(radius);
	int diameter = radiusAsInt * 2;
	float radiusSqr = radius * radius;
	bool changedAnything = false;

	IntVec3 startingCoords = localCoords - IntVec3(radiusAsInt, radiusAsInt, radiusAsInt);
	
	//BlockDefID water = BlockDef::GetBlockDefIDByName("water");
	BlockDefID lamp = BlockDef::GetBlockDefIDByName("glowstone");
	BlockDefID dirt = BlockDef::GetBlockDefIDByName("dirt");

	for (int zOffset = 0; zOffset < diameter; zOffset++) {
		for (int yOffset = 0; yOffset < diameter; yOffset++) {
			for (int xOffset = 0; xOffset < diameter; xOffset++) {

				IntVec3 resultingCoords = startingCoords + IntVec3(xOffset, yOffset, zOffset);
				if (resultingCoords.z <= 0) continue;

				IntVec3 globalCoords = GetGlobalCoordsForLocalCoords(resultingCoords);


				if (!AreLocalCoordsWithinChunk(resultingCoords)) continue;



				Vec3 blockCenter = Vec3(globalCoords.x + 0.5f, globalCoords.y + 0.5f, globalCoords.z + 0.5f);
				float distanceToBlock = GetDistanceSquared3D(blockCenter, originPos);

				if (!AreLocalCoordsWithinChunk(resultingCoords)) continue;
				
				

				int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(resultingCoords));
				Block& carvedBlock = m_blocks[blockIndex];

				if (placeLampInMiddle && distanceToBlock == 0.f && resultingCoords.z < 64)
				{
					carvedBlock.SetTypeID(lamp);
				}
				else if(distanceToBlock < radiusSqr)
				{
					if (carvedBlock.GetTypeID() != 0)
					{
						changedAnything = true;
						BlockIterator blockIter = BlockIterator(this, blockIndex);
						DigBlock(blockIter);
					}
				}
				else if (replaceWithDirt)
				{
					if (carvedBlock.GetTypeID() != 0)
					{
						changedAnything = true;
						carvedBlock.SetTypeID(dirt);
					}
				}
			}
		}
	}

	if (changedAnything)
	{
		m_isChunkDirty = true;
		if (m_northNeighbor) m_northNeighbor->m_isChunkDirty;
		if (m_southNeighbor) m_southNeighbor->m_isChunkDirty;
		if (m_eastNeighbor) m_eastNeighbor->m_isChunkDirty;
		if (m_westNeighbor) m_westNeighbor->m_isChunkDirty;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::AreCoordsConsideredLocalMaxima(IntVec2 const& coords, int radius, std::map<IntVec2, float> const& perlinNoiseHolder) const
{
	IntVec2 startCoords = coords - IntVec2(radius, radius);
	auto localNoiseIt = perlinNoiseHolder.find(coords);

	if (localNoiseIt == perlinNoiseHolder.end()) return false;

	float localNoise = localNoiseIt->second;


	int diameter = radius * 2;

	for (int yOffset = 0; yOffset < diameter; yOffset++)
	{
		for (int xOffset = 0; xOffset < diameter; xOffset++) 
		{
			IntVec2 resultingCoords = startCoords + IntVec2(xOffset, yOffset);
			if (resultingCoords == coords) continue;

			auto coordsNoiseIt = perlinNoiseHolder.find(resultingCoords);
			if (coordsNoiseIt == perlinNoiseHolder.end()) continue;


			float const& coordsNoise = coordsNoiseIt->second;
			if (localNoise <= coordsNoise) {
				return false;
			}

		}
	}

	return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Chunk::AreLocalCoordsWithinChunk(IntVec3 const& localCoords)
{
	if (localCoords.x < 0 || localCoords.x >= CHUNK_SIZE_X) return false;
	if (localCoords.y < 0 || localCoords.y >= CHUNK_SIZE_Y) return false;
	if (localCoords.z < 0 || localCoords.z >= CHUNK_SIZE_Z) return false;

	return true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoordsForLocalCoords(IntVec3 const& localCoords)
{
	int chunkX = m_chunkCoords.x * CHUNK_SIZE_X;
	int chunkY = m_chunkCoords.y * CHUNK_SIZE_Y;

	return IntVec3(localCoords.x + chunkX, localCoords.y + chunkY, localCoords.z);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void ChunkGenerationJob::Execute()
{
	m_chunk->m_status = ACTIVATING_GENERATING;
	m_chunk->Generateblocks();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void ChunkGenerationJob::OnFinished()
{
	m_chunk->m_status = ACTIVATING_GENERATE_COMPLETE;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void ChunkDiskLoadJob::Execute()
{
	
	m_chunk->m_status = ChunkState::ACTIAVTING_QUEUED_LOAD;
	m_loadingSuccessful = m_chunk->LoadBlocksFromFile();
	
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void ChunkDiskLoadJob::OnFinished()
{
	if (m_loadingSuccessful)
	{
		
			m_chunk->m_status = ChunkState::ACTIVATING_LOAD_COMPLETE;
		
	}
// 	else
// 	{
// 		ChunkGenerationJob* generationJob = new ChunkGenerationJob(m_chunk);
// 		g_theJobSystem->QueueJob(generationJob);
// 	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void ChunkDiskSaveJob::Execute()
{
	
		m_chunk->m_status = ChunkState::DEACTIVATING_QUEUED_SAVE;
		m_chunk->SaveBlockToFile();
	
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void ChunkDiskSaveJob::OnFinished()
{
	
		m_chunk->m_status = ChunkState::DEACTIVATING_SAVE_COMPLETE;
	
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------