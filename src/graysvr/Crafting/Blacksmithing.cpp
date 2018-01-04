#include <cmath>
#include "../graysvr.h"	// predef header.

bool CClient::CraftingBlacksmithing(CItem *pIngots)
{
	ADDTOCALLSTACK("CClient::CraftingBlacksmithing");
	ASSERT(m_pChar);
	if (!pIngots || !pIngots->IsType(IT_INGOT))
	{
		SysMessageDefault(DEFMSG_SMITHING_FAIL);
		return false;
	}

	ASSERT(m_Targ_UID == pIngots->GetUID());
	if (pIngots->GetTopLevelObj() != m_pChar)
	{
		SysMessageDefault(DEFMSG_SMITHING_REACH);
		return false;
	}

	// Must have smith hammer equipped
	CItem *pSmithHammer = m_pChar->LayerFind(LAYER_HAND1);
	if (!pSmithHammer || !pSmithHammer->IsType(IT_WEAPON_MACE_SMITH))
	{
		SysMessageDefault(DEFMSG_SMITHING_HAMMER);
		return false;
	}

	// Select the blacksmith item type.
	// repair items or make type of items.
	if (!g_World.IsItemTypeNear(m_pChar->GetTopPoint(), IT_FORGE, 3, false))
	{
		SysMessageDefault(DEFMSG_SMITHING_FORGE);
		return false;
	}

	// Call for Blacksmithing crafting menu.
	return CraftingSelect(SKILL_BLACKSMITHING);
}
