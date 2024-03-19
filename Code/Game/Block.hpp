#pragma once
#include "Game/BlockDef.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
constexpr uint8_t BLOCK_BIT_IS_SKY = 0b00000001;
constexpr uint8_t BLOCK_BIT_IS_LIGHT_DIRTY = 0b00000010;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class Block
{
public:
	Block();
	~Block();

	void			  SetTypeID(BlockDefID newType);
	BlockDefID		  GetTypeID() const { return m_blockDefID; };
	BlockDef const&	  GetType();
	uint8_t			  GetIndoorLightInfluence() const;
	void			  SetIndoorLightInfluence(int indoorLightInfluence);
	uint8_t			  GetOutdoorLightInfluence() const;
	void			  SetOutdoorLightInfluence(int outdoorLightInfluence);
	bool			  IsBlockSky() const;
	void			  SetIsBlockSky(bool isSky);
	bool			  IsBlockLightDirty() const;
	void			  SetIsBlockLightDirty(bool isLightDirty);

private:
	BlockDefID  m_blockDefID = 0;
	uint8_t		m_lightInfluence = 0;
	uint8_t		m_bitflags = 0;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------