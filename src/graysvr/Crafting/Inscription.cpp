#include <cmath>
#include "../graysvr.h"	// predef header.

bool CClient::CraftingInscription()
{
	ADDTOCALLSTACK("CClient::CraftingInscription");
	// Select the scroll type to make.
	// iSelect = -1 = 1st setup.
	// iSelect = 0 = cancel
	// iSelect = x = execute the selection.
	// we should already be in inscription skill mode.

	ASSERT(m_pChar);

	CItem *pBlankScroll = m_pChar->ContentFind(RESOURCE_ID(RES_TYPEDEF, IT_SCROLL_BLANK));
	if (!pBlankScroll)
	{
		SysMessageDefault(DEFMSG_INSCRIPTION_FAIL);
		return false;
	}

	return CraftingSelect(SKILL_INSCRIPTION);
}
