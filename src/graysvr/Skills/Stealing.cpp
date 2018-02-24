#include "../graysvr.h"	// predef header.
#include "../CClient.h"
#include "../../network/send.h"

// m_Act_Targ = object to steal.
// RETURN:
// -SKTRIG_QTY = no chance. and not a crime
// -SKTRIG_FAIL = no chance and caught.
// 0-100 = difficulty = percent chance of failure.
int CChar::Skill_Stealing(SKTRIG_TYPE stage)
{
	ADDTOCALLSTACK("CChar::Skill_Stealing");
	if (stage == SKTRIG_STROKE)
		return(0);

	// We're trying to steal. If there's no item (pItem)
	// Try to find if we targeted a char (pCharMark)
	CItem * pItem = m_Act_Targ.ItemFind();
	CChar * pCharMark = NULL;
	if (pItem == NULL)
	{
		pCharMark = m_Act_Targ.CharFind();
		if (pCharMark == NULL)
		{
			SysMessageDefault(DEFMSG_STEALING_NOTHING);
			return(-SKTRIG_QTY);
		}
		
		// If there's no pack.
		CItemContainer *pPack = pCharMark->GetContainer(LAYER_PACK);
		if (pPack == NULL)
		{
			cantsteal:
			SysMessageDefault(DEFMSG_STEALING_EMPTY);
			return(-SKTRIG_QTY);
		}

		// Can't find random item in pack.
		pItem = pPack->GetAt(Calc_GetRandVal(pPack->GetCount()));
		if (!pItem)
			goto cantsteal;

		m_Act_Targ = pItem->GetUID();
	}

	// Why trying to steal from a corpse?
	CContainer *pContainer = dynamic_cast<CContainer *>(pItem->GetParentObj());
	if (pContainer)
	{
		CItemCorpse *pCorpse = dynamic_cast<CItemCorpse *>(pContainer);
		if (pCorpse)
		{
			SysMessageDefault(DEFMSG_STEALING_CORPSE);
			return(-SKTRIG_ABORT);
		}
	}

	// Don't steal game pieces or trade window
	CItem *pCItem = dynamic_cast<CItem *>(pItem->GetParentObj());
	if (pCItem)
	{
		if (pCItem->GetType() == IT_GAME_BOARD)
		{
			SysMessageDefault(DEFMSG_STEALING_GAMEBOARD);
			return(-SKTRIG_ABORT);
		}
		if (pCItem->GetType() == IT_EQ_TRADE_WINDOW)
		{
			SysMessageDefault(DEFMSG_STEALING_TRADE);
			return(-SKTRIG_ABORT);
		}
	}

	// We're training.
	if (pItem->IsType(IT_TRAIN_PICKPOCKET))
	{
		SysMessageDefault(DEFMSG_STEALING_PICKPOCKET);
		return -SKTRIG_QTY;
	}

	// Is a game piece.
	if (pItem->IsType(IT_GAME_PIECE))
	{
		return -SKTRIG_QTY;
	}

	// I can't see it or reach it.
	if (!CanTouch(pItem))
	{
		SysMessageDefault(DEFMSG_STEALING_REACH);
		return(-SKTRIG_ABORT);
	}

	// We're too heavy.
	if (!CanMove(pItem) || !CanCarry(pItem))
	{
		SysMessageDefault(DEFMSG_STEALING_HEAVY);
		return(-SKTRIG_ABORT);
	}

	// No crime, we just can pick it up.
	if (!IsTakeCrime(pItem, &pCharMark))
	{
		SysMessageDefault(DEFMSG_STEALING_NONEED);
		return(-SKTRIG_QTY);
	}

	// Can't steal if you're in safe place.
	if (m_pArea->IsFlag(REGION_FLAG_SAFE))
	{
		SysMessageDefault(DEFMSG_STEALING_STOP);
		return(-SKTRIG_QTY);
	}

	Reveal();	// If we take an item off the ground we are revealed.

	bool fGround = false;
	if (pCharMark != NULL)
	{
		// We're too far away.
		if (GetTopDist3D(pCharMark) > 2)
		{
			SysMessageDefault(DEFMSG_STEALING_MARK);
			return -SKTRIG_QTY;
		}

		// We're in a no pvp zone!
		if (m_pArea->IsFlag(REGION_FLAG_NO_PVP) && pCharMark->m_pPlayer && !IsPriv(PRIV_GM))
		{
			SysMessageDefault(DEFMSG_STEALING_SAFE);
			return(-1);
		}

		// Can't steal from GMs.
		if (GetPrivLevel() < pCharMark->GetPrivLevel())
		{
			return -SKTRIG_FAIL;
		}

		// The item is equipped.
		if (pItem->IsItemEquipped())
		{
			SysMessageDefault(DEFMSG_STEALING_SAFE);
			Noto_Karma(-100, -1000, true);
			Reveal();
			return(-1);
		}

		// We're starting, calculate the difficulty.
		if (stage == SKTRIG_START)
		{
			return g_Cfg.Calc_StealingItem(this, pItem, pCharMark);
		}
	}
	else
	{
		// stealing off the ground should always succeed.
		// it's just a matter of getting caught.
		if (stage == SKTRIG_START)
			return(1);	// town stuff on the ground is too easy.

		fGround = true;
	}

	// Deliver the goods.

	if (stage == SKTRIG_SUCCESS || fGround)
	{
		pItem->ClrAttr(ATTR_OWNED);	// Now it's mine
		CItemContainer *pPack = GetContainer(LAYER_PACK);
		if (pPack && (pItem->GetParent() != pPack))
		{
			pItem->RemoveFromView();
			// Put in my invent.
			pPack->ContentAdd(pItem);
		}
	}

	if (m_Act_Difficulty == 0)
		return(0);	// Too easy to be bad. hehe

	// You should only be able to go down to -1000 karma by stealing.
	if (Calc_GetRandVal(pCharMark->Stat_GetMax(STAT_INT)) > Stat_GetMax(STAT_INT) / 2)
	{
		if (CheckCrimeSeen(SKILL_STEALING, pCharMark, pItem, (stage == SKTRIG_FAIL) ? g_Cfg.GetDefaultMsg(DEFMSG_STEALING_YOUR) : g_Cfg.GetDefaultMsg(DEFMSG_STEALING_SOMEONE)))
			Noto_Karma(-100, -1000, true);
	}
		
	return(0);
}