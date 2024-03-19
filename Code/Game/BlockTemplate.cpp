#include "Game/BlockTemplate.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
std::vector<BlockTemplate*> BlockTemplate::s_blockTemplates;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockTemplateEntry::BlockTemplateEntry(BlockDefID blockType, IntVec3 const& offset)
	:m_blockType(blockType), m_offset(offset)
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockTemplate::BlockTemplate()
{

}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockTemplate::BlockTemplate(char const* name, std::vector<BlockTemplateEntry>& blockTemplateEntries)
	:m_templateName(name), m_blockTemplateEntries(blockTemplateEntries)
{
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void BlockTemplate::InitializeBlockTemplateDefinitions()
{
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
	//For cactus----------------------------------------------------
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
	BlockDefID const& cactus = BlockDef::GetBlockDefIDByName("cactus");
	std::vector<BlockTemplateEntry> cactusTemplateEntries;
	
	cactusTemplateEntries.push_back(BlockTemplateEntry(cactus, IntVec3::ZERO));
	cactusTemplateEntries.push_back(BlockTemplateEntry(cactus, IntVec3(0, 0, 1)));
	cactusTemplateEntries.push_back(BlockTemplateEntry(cactus, IntVec3(0, 0, 2)));
	cactusTemplateEntries.push_back(BlockTemplateEntry(cactus, IntVec3(0, 0, 3)));
	
	BlockTemplate* cactusTemplate = new  BlockTemplate("cactus", cactusTemplateEntries);
	s_blockTemplates.push_back(cactusTemplate);
	
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
	//For oak tree--------------------------------------------------
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
	BlockDefID const& oakLog = BlockDef::GetBlockDefIDByName("oak_log");
	std::vector<BlockTemplateEntry> oakTreeTemplateEntries;
	
	//For trunk/log
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLog, IntVec3::ZERO));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLog, IntVec3(0, 0, 1)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLog, IntVec3(0, 0, 2)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLog, IntVec3(0, 0, 3)));
	
	//For oak tree leaves
 	BlockDefID const& oakLeaves = BlockDef::GetBlockDefIDByName("oak_leaves");
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(0, 0, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(0, 0, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(1, 0, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(0, 1, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(1, 1, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(-1, 1, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(1, -1, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(-1, 0, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(0, -1, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(-1, -1, 4)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(1, 0, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(0, 1, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(1, 1, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(-1, 1, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(1, -1, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(-1, 0, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(0, -1, 5)));
	oakTreeTemplateEntries.push_back(BlockTemplateEntry(oakLeaves, IntVec3(-1, -1, 5)));
 	
	BlockTemplate* oakTreeTemplate = new BlockTemplate("oak_tree", oakTreeTemplateEntries);
	s_blockTemplates.push_back(oakTreeTemplate);
	
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
	//For spruce trees-------------------------------------------------
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
 	BlockDefID const& spruceLog = BlockDef::GetBlockDefIDByName("spruce_log");
	std::vector<BlockTemplateEntry> spruceTreeTemplateEntries;

	//For trunk/log
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLog, IntVec3::ZERO));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLog, IntVec3(0, 0, 1)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLog, IntVec3(0, 0, 2)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLog, IntVec3(0, 0, 3)));

	//For spruce tree leaves
	BlockDefID const& spruceLeaves = BlockDef::GetBlockDefIDByName("spruce_leaves");
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(0, 0, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(0, 0, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(1, 0, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(0, 1, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(1, 1, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(-1, 1, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(1, -1, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(-1, 0, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(0, -1, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(-1, -1, 4)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(1, 0, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(0, 1, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(1, 1, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(-1, 1, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(1, -1, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(-1, 0, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(0, -1, 5)));
	spruceTreeTemplateEntries.push_back(BlockTemplateEntry(spruceLeaves, IntVec3(-1, -1, 5)));

	BlockTemplate* spruceTreeTemplate = new BlockTemplate("spruce_tree", spruceTreeTemplateEntries);
 	s_blockTemplates.push_back(spruceTreeTemplate);
	//--------------------------------------------------------------------------------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void BlockTemplate::DestroyBlockTemplateDefinitions()
{
	for (int templateIndex = 0; templateIndex < s_blockTemplates.size(); templateIndex++)
	{
		BlockTemplate* blockTemplate = s_blockTemplates[templateIndex];
		if (blockTemplate)
		{
			delete blockTemplate;
			blockTemplate = nullptr;
		}
	}
	s_blockTemplates.clear();
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
BlockTemplate const* BlockTemplate::GetBlockTemplateByName(char const* name)
{
	for (int i = 0; i < s_blockTemplates.size(); i++)
	{
		BlockTemplate const* blockTemplate = s_blockTemplates[i];
		if (blockTemplate)
		{
			if (blockTemplate->m_templateName == name)
			{
				return blockTemplate;
			}
		}
	}

	std::string message = "Invalid block name";
	message.append(name);
	ERROR_AND_DIE(message);
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------