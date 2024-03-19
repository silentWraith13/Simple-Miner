#include "Game/BlockIterator.hpp"
#include "Engine/Math/AABB3.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator::BlockIterator()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator::BlockIterator(Chunk* chunk, int blockIndex)
	:m_chunk(chunk), m_blockIndex(blockIndex)
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator::~BlockIterator()
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Block* BlockIterator::GetBlock() const
{
	if (!m_chunk || m_chunk->m_blocks == nullptr)
	{
		return nullptr;
	}

	GUARANTEE_OR_DIE(m_blockIndex >= 0 && m_blockIndex < CHUNK_BLOCKS_TOTAL, "Bad index - out of bounds");
	return &m_chunk->m_blocks[m_blockIndex];
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Vec3 BlockIterator::GetWorldCenter() const
{
	AABB3 worldChunkBounds = m_chunk->GetWorldBounds();
	IntVec3 localBlockCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	Vec3 worldCenter = worldChunkBounds.m_mins + static_cast<Vec3>(localBlockCoords) + Vec3(0.5f, 0.5f, 0.5f);
	return worldCenter;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetEastNeighbour() const
{
	if (!m_chunk || !m_chunk->m_blocks)
		return BlockIterator(nullptr, -1);

	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.x == CHUNK_MAX_X)
	{
		blockIterator.m_chunk = m_chunk->m_eastNeighbor;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(0, localCoords.y, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunk = m_chunk;
	blockIterator.m_blockIndex = m_chunk->GetBlockIndexFromLocalCoords(localCoords + IntVec3(1, 0, 0));
	return blockIterator;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetNorthNeighbour() const
{
	if (!m_chunk || !m_chunk->m_blocks)
		return BlockIterator(nullptr, -1);

	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.y == CHUNK_MAX_Y)
	{
		blockIterator.m_chunk = m_chunk->m_northNeighbor;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(localCoords.x, 0, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunk = m_chunk;
	blockIterator.m_blockIndex = m_chunk->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, 1, 0));
	return blockIterator;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetWestNeighbour() const
{
	if (!m_chunk || !m_chunk->m_blocks)
		return BlockIterator(nullptr, -1);

	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.x == 0)
	{
		blockIterator.m_chunk = m_chunk->m_westNeighbor;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(CHUNK_MAX_X, localCoords.y, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunk = m_chunk;
	blockIterator.m_blockIndex = m_chunk->GetBlockIndexFromLocalCoords(localCoords + IntVec3(-1, 0, 0));
	return blockIterator;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetSouthNeighbour() const
{
	if (!m_chunk || !m_chunk->m_blocks)
		return BlockIterator(nullptr, -1);

	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.y == 0)
	{
		blockIterator.m_chunk = m_chunk->m_southNeighbor;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(localCoords.x, CHUNK_MAX_Y, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunk = m_chunk;
	blockIterator.m_blockIndex = m_chunk->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, -1, 0));
	return blockIterator;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetAboveNeighbour() const
{
	if (!m_chunk || !m_chunk->m_blocks)
		return BlockIterator(nullptr, -1);

	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.z == CHUNK_MAX_Z)
	{
		return *this;
	}

	blockIterator.m_chunk = m_chunk;
	blockIterator.m_blockIndex = m_chunk->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, 0, 1));
	return blockIterator;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetBelowNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunk->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.z == 0)
	{
		return *this;
	}

	blockIterator.m_chunk = m_chunk;
	blockIterator.m_blockIndex = m_chunk->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, 0, -1));
	return blockIterator;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------

