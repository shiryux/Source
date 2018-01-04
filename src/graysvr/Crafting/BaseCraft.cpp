#include <cmath>
#include "../graysvr.h"	// predef header.


bool CClient::CraftingSelect(SKILL_TYPE skill)
{
	// Default menu is d_craft_menu
	// Open in page 0, args is skill used.
	LPCTSTR dSkillMenu = "d_craft_menu";
	
	CScriptTriggerArgs Args;
	Args.m_VarsLocal.SetStrNew("SkillMenu", dSkillMenu);
	Args.m_VarsLocal.SetNumNew("Skill", skill);
	if (IsTrigUsed(TRIGGER_SKILLMENU))
	{
		if (m_pChar->Skill_OnCharTrigger(skill, CTRIG_SkillMenu, &Args) == TRIGRET_RET_TRUE)
			return true;

		dSkillMenu = Args.m_VarsLocal.GetKeyStr("Skillmenu", false);
	}

	LPCTSTR SkillUsed = g_Cfg.GetSkillKey(skill);
	//g_Log.EventError("Crafting::Using skill %s \n", SkillUsed);
	if (IsValidDef(dSkillMenu))
		return Dialog_Setup(CLIMODE_DIALOG, g_Cfg.ResourceGetIDType(RES_DIALOG, dSkillMenu), 0, m_pChar, SkillUsed);
	else
		g_Log.EventError("Crafting::Not valid dialog %s \n", dSkillMenu);

	return false;
}
