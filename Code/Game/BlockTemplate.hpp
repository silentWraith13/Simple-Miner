#pragma once
#include "Game/BlockDef.hpp"
#include "Engine/Math/IntVec3.hpp"
#include <string>
#include <vector>

//--------------------------------------------------------------------------------------------------------------------------------------------------------
struct BlockTemplateEntry 
{
public:
	BlockTemplateEntry(BlockDefID blockType, IntVec3 const& offset);

	BlockDefID m_blockType;
	IntVec3    m_offset;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------
class BlockTemplate 
{
public:
	BlockTemplate();
	explicit BlockTemplate(char const* name, std::vector<BlockTemplateEntry>& blockTemplateEntries);

	static void					InitializeBlockTemplateDefinitions();
	static void					DestroyBlockTemplateDefinitions();
	static BlockTemplate const* GetBlockTemplateByName(char const* name);
	
	std::vector<BlockTemplateEntry>     m_blockTemplateEntries;
	std::string						    m_templateName;
	static std::vector<BlockTemplate*>  s_blockTemplates;
};
//--------------------------------------------------------------------------------------------------------------------------------------------------------