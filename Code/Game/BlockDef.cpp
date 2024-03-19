#include "Game/BlockDef.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "GameCommon.hpp"
//--------------------------------------------------------------------------------------------------------------------------------------------------------
std::vector<BlockDef> BlockDef::s_blockDefs;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockDef::BlockDef()
{

}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockDef::~BlockDef()
{

}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void BlockDef::InitializeBlockDefs()
{
	CreateNewBlockDef("air",		   false, false,  false, 0,  IntVec2(0, 0),   IntVec2(0, 0),   IntVec2(0, 0));
	CreateNewBlockDef("water",		   true,  false,  true,  0,  IntVec2(32, 44), IntVec2(32, 44), IntVec2(32, 44));
	CreateNewBlockDef("stone",		   true,  true,   true,  0,  IntVec2(33, 32), IntVec2(33, 32), IntVec2(33, 32));
	CreateNewBlockDef("dirt",		   true,  true,   true,  0,  IntVec2(32, 34), IntVec2(32, 34), IntVec2(32, 34));
	CreateNewBlockDef("grass",		   true,  true,   true,  0,  IntVec2(32, 33), IntVec2(33, 33), IntVec2(32, 34));
	CreateNewBlockDef("coal",		   true,  true,   true,  0,  IntVec2(63, 34), IntVec2(63, 34), IntVec2(63, 34));
	CreateNewBlockDef("iron",		   true,  true,   true,  0,  IntVec2(63, 35), IntVec2(63, 35), IntVec2(63, 35));
	CreateNewBlockDef("gold",		   true,  true,   true,  0,  IntVec2(63, 36), IntVec2(63, 36), IntVec2(63, 36));
	CreateNewBlockDef("glowstone",     true,  true,   true,  15, IntVec2(46, 34), IntVec2(46, 34), IntVec2(46, 34));
	CreateNewBlockDef("brick",		   true,  true,   true,  0,  IntVec2(57, 32), IntVec2(57, 32), IntVec2(57, 32));
	CreateNewBlockDef("cobblestone",   true,  true,   true,  0,  IntVec2(34, 32), IntVec2(34, 32), IntVec2(34, 32));
	CreateNewBlockDef("diamond",	   true,  true,   true,  0,  IntVec2(63, 37), IntVec2(63, 37), IntVec2(63, 37));
	CreateNewBlockDef("sand",		   true,  true,   true,  0,  IntVec2(34, 34), IntVec2(34, 34), IntVec2(34, 34));
	CreateNewBlockDef("ice",		   true,  true,   true,  0,  IntVec2(45, 34), IntVec2(45, 34), IntVec2(45, 34));
	CreateNewBlockDef("snow",		   true,  true,   true,  0,  IntVec2(36, 35), IntVec2(36, 35), IntVec2(36, 35));
	CreateNewBlockDef("oak_log",	   true,  true,   true,  0,  IntVec2(38, 33), IntVec2(36, 33), IntVec2(38, 33));
	CreateNewBlockDef("oak_leaves",    true,  true,   true,  0,  IntVec2(32, 35), IntVec2(32, 35), IntVec2(32, 35));
	CreateNewBlockDef("spruce_log",    true,  true,   true,  0,  IntVec2(38, 33), IntVec2(37, 33), IntVec2(38, 33));
	CreateNewBlockDef("spruce_leaves", true,  true,   true,  0,  IntVec2(62, 41), IntVec2(62, 41), IntVec2(62, 41));
	CreateNewBlockDef("cactus",		   true,  true,   true,  0,  IntVec2(39, 36), IntVec2(37, 36), IntVec2(38, 36));
	CreateNewBlockDef("cloud",		   true,  true,   true,  0,  IntVec2(0, 4),   IntVec2(0, 4),   IntVec2(0, 4));
	CreateNewBlockDef("snowgrass",	   true,  true,   true,  0,	 IntVec2(36, 35), IntVec2(32, 34), IntVec2(33, 35));
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockDefID BlockDef::GetBlockDefIDByName(char const* blockDefname, bool errorIfMissing /*= true*/)
{
	for (BlockDefID blockDefId = 0; blockDefId < s_blockDefs.size(); blockDefId++)
	{
		if (s_blockDefs[blockDefId].m_name == blockDefname)
		{
			return blockDefId;
		}
	}

	if (errorIfMissing)
	{
		int numBlockTypes = int(s_blockDefs.size());
		std::string errorText = Stringf("No block def named \"%s\" found! \n", blockDefname);
		errorText += Stringf("Must be one of the these %i block types: \n", numBlockTypes);
		ERROR_AND_DIE(errorText);
	}

	return BLOCKDEF_ID_INVALID;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockDef const& BlockDef::GetBlockDefByName(char const* blockDefName)
{
	for (int i = 0; i < s_blockDefs.size(); i++)
	{
		if (s_blockDefs[i].m_name == blockDefName)
		{
			return s_blockDefs[i];
		}
	}

	std::string message = "Invalid block name";
	message.append(blockDefName);
	ERROR_AND_DIE(message);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockDef const& BlockDef::GetBlockDefByID(BlockDefID blockDefID)
{
	BlockDef const& blockDef = s_blockDefs[(int)blockDefID];
	return blockDef;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool BlockDef::IsBlockTypeOpaque(int blockDefIndex)
{
	return s_blockDefs[blockDefIndex].m_isOpaque;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
bool BlockDef::DoesBlockTypeEmitLight(int blockDefIndex)
{
	return s_blockDefs[blockDefIndex].m_indoorLightInfluence > 0;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void BlockDef::CreateNewBlockDef(std::string const& name, bool isVisible, bool isSolid, bool isOpaque, uint8_t indoorLightInfluence, IntVec2 topSpriteCoords, IntVec2 sideSpriteCoords, IntVec2 bottomSpriteCoords)
{
	BlockDef def;
	def.m_name = name;
	def.m_isVisible = isVisible;
	def.m_isSolid = isSolid;
	def.m_isOpaque = isOpaque;
	def.m_indoorLightInfluence = indoorLightInfluence;

	int const SPRITES_PER_ROW = TERRAIN_SPRITE_LAYOUT.x;

	int topSpriteIndex = topSpriteCoords.x + (topSpriteCoords.y * SPRITES_PER_ROW);
	def.m_uvsTop = g_terrainSpriteSheet->GetSpriteDef(topSpriteIndex).GetUVs();

	int sideSpriteIndex = sideSpriteCoords.x + (sideSpriteCoords.y * SPRITES_PER_ROW);
	def.m_uvsSides = g_terrainSpriteSheet->GetSpriteDef(sideSpriteIndex).GetUVs();

	int bottomSpriteIndex = bottomSpriteCoords.x + (bottomSpriteCoords.y * SPRITES_PER_ROW);
	def.m_uvsBottom = g_terrainSpriteSheet->GetSpriteDef(bottomSpriteIndex).GetUVs();

	s_blockDefs.push_back(def);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
