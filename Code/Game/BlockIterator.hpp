#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Game/Block.hpp"
#include "Game/Chunks.hpp"
//--------------------------------------------------------------------------------------------------------------------------------------------------------
struct BlockIterator
{
public:
	BlockIterator();
	explicit BlockIterator(Chunk* chunk, int blockIndex);
	~BlockIterator();
	Block*		  GetBlock() const;
	Vec3		  GetWorldCenter() const;
	BlockIterator GetEastNeighbour() const;
	BlockIterator GetNorthNeighbour() const;
	BlockIterator GetWestNeighbour() const;
	BlockIterator GetSouthNeighbour() const;
	BlockIterator GetAboveNeighbour() const;
	BlockIterator GetBelowNeighbour() const;

	//Member variables
	Chunk* m_chunk = nullptr;
	int	   m_blockIndex = 0;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------