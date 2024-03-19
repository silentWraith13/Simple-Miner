#include "Game/Block.hpp"
#include "Game/BlockDef.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
Block::Block()
{

}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
Block::~Block()
{

}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Block::SetTypeID(BlockDefID newType)
{
	m_blockDefID = newType;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockDef const& Block::GetType()
{
	return BlockDef::GetBlockDefByID(m_blockDefID);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t Block::GetIndoorLightInfluence() const
{
	return (m_lightInfluence & 0x0f); // & masks the upper 4 bits and returns the lower 4 bits.
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Block::SetIndoorLightInfluence(int lightInfluence)
{
	m_lightInfluence &= 0xf0;	//leaves upper 4 bits unchanged and set lower 4 bits to 0. 
	m_lightInfluence |= lightInfluence;	//set the lower 4 bits with the light influence value
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
uint8_t Block::GetOutdoorLightInfluence() const
{
	return m_lightInfluence >> 4;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Block::SetOutdoorLightInfluence(int lightInfluence)
{
	m_lightInfluence &= 0x0f;	//clear current outdoor light influence
	m_lightInfluence |= (lightInfluence << 4);	//set the higher 4 bits with the light influence value
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Block::IsBlockSky() const
{
	return ((m_bitflags & BLOCK_BIT_IS_SKY) == BLOCK_BIT_IS_SKY);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Block::SetIsBlockSky(bool isSky)
{
	if (isSky)
	{
		m_bitflags |= BLOCK_BIT_IS_SKY;
	}
	else
	{
		m_bitflags &= ~BLOCK_BIT_IS_SKY;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool Block::IsBlockLightDirty() const
{
	return ((m_bitflags & BLOCK_BIT_IS_LIGHT_DIRTY) == BLOCK_BIT_IS_LIGHT_DIRTY);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Block::SetIsBlockLightDirty(bool isLightDirty)
{
	if (isLightDirty)
	{
		m_bitflags |= BLOCK_BIT_IS_LIGHT_DIRTY;
	}
	else
	{
		m_bitflags &= ~BLOCK_BIT_IS_LIGHT_DIRTY;
	}
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------