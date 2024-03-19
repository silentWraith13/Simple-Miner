#pragma once
#include "Engine/Core/vertexUtils.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
typedef uint8_t BlockDefID;
constexpr BlockDefID BLOCKDEF_ID_INVALID = BlockDefID(-1);
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class BlockDef
{
public:
	BlockDef();
	~BlockDef();

	static void				InitializeBlockDefs();
	static BlockDefID		GetBlockDefIDByName(char const* blockDefname, bool errorIfMissing = true);
	static BlockDef const&  GetBlockDefByName(char const* blockDefName);
	static BlockDef const&  GetBlockDefByID(BlockDefID blockDefID);
	static bool				IsBlockTypeOpaque(int blockDefIndex);
	static bool				DoesBlockTypeEmitLight(int blockDefIndex);

protected:
	static void				CreateNewBlockDef(std::string const& name, bool isVisible, bool isSolid, bool isOpaque, uint8_t indoorLightInfluence, IntVec2 topSpriteCoords,
							IntVec2 sideSpriteCoords, IntVec2 bottomSpriteCoords);
public:
	static std::vector<BlockDef> s_blockDefs;
	static Texture*				 s_blockSpriteTexture;
	std::string					 m_name;
	bool						 m_isVisible	= true;
	bool						 m_isSolid		= true;
	bool						 m_isOpaque	= true;
	uint8_t						 m_indoorLightInfluence = 0;
	AABB2						 m_uvsTop		= AABB2::ZERO_TO_ONE;
	AABB2						 m_uvsSides	= AABB2::ZERO_TO_ONE;
	AABB2						 m_uvsBottom	= AABB2::ZERO_TO_ONE;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------