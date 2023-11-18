/// \file  menus/extras-1.c
/// \brief Extras Menu

#include "../k_menu.h"
#include "../m_cond.h"
#include "../m_cheat.h"
#include "../s_sound.h"
#include "../f_finale.h"

static void M_Credits(INT32 choice)
{
	(void)choice;
	restoreMenu = currentMenu;
	M_ClearMenus(true);
	F_StartCredits();
}

menuitem_t EXTRAS_Main[] =
{
	// The following has NULL strings for text and tooltip.
	// These are populated in M_InitExtras depending on unlock state.
	// (This is legal - they're (const char)*'s, not const (char*)'s.

	{IT_STRING | IT_CALL, NULL, NULL,
		NULL, {.routine = M_Addons}, 0, 0},

	{IT_STRING | IT_CALL, "Tutorial", "Help Dr. Robotnik and Tails test out their new Ring Racers.",
		NULL, {.routine = M_LevelSelectInit}, 0, GT_TUTORIAL},

	{IT_STRING | IT_CALL, "Challenges", "View the requirements for some of the secret content you can unlock!",
		NULL, {.routine = M_Challenges}, 0, 0},

	{IT_STRING | IT_CALL, "Statistics", "Look back on some of your greatest achievements such as your playtime and wins!",
		NULL, {.routine = M_Statistics}, 0, 0},

	{IT_STRING | IT_CALL, NULL, NULL,
		NULL, {.routine = M_EggTV}, 0, 0},

	{IT_STRING | IT_CALL, NULL, NULL,
		NULL, {.routine = M_SoundTest}, 0, 0},

	{IT_STRING | IT_CVAR | IT_CV_STRING, "Password", "If you don't know any passwords, come back later!",
		NULL, {.cvar = &cv_dummyextraspassword}, 0, 0},

	{IT_STRING | IT_CALL, "Credits", "It's important to know who makes the video games you play.",
		NULL, {.routine = M_Credits}, 0, 0},
};

// the extras menu essentially reuses the options menu stuff
menu_t EXTRAS_MainDef = {
	sizeof (EXTRAS_Main) / sizeof (menuitem_t),
	&MainDef,
	0,
	EXTRAS_Main,
	0, 0,
	0, 0,
	0,
	"EXTRAS",
	28, 5,
	M_DrawExtras,
	M_ExtrasTick,
	NULL,
	NULL,
	M_ExtrasInputs
};

// Extras menu;
// this is copypasted from the options menu but all of these are different functions in case we ever want it to look more unique

struct extrasmenu_s extrasmenu;

void M_InitExtras(INT32 choice)
{
	// Addons
	if (M_SecretUnlocked(SECRET_ADDONS, true))
	{
		EXTRAS_Main[extras_addons].status = IT_STRING | IT_CALL;
		EXTRAS_Main[extras_addons].text = "Addons";
		EXTRAS_Main[extras_addons].tooltip = "Add files to customize your experience.";
	}
	else
	{
		EXTRAS_Main[extras_addons].status = IT_STRING | IT_TRANSTEXT;
		EXTRAS_Main[extras_addons].text = EXTRAS_Main[extras_addons].tooltip = "???";
		if (EXTRAS_MainDef.lastOn == extras_addons)
		{
			EXTRAS_MainDef.lastOn = extras_challenges;
		}
	}

	// Tutorial
	{
		UINT16 map = G_GetFirstMapOfGametype(GT_TUTORIAL);

		EXTRAS_Main[extras_tutorial].status = (IT_STRING | 
			((map == NEXTMAP_INVALID) ? IT_TRANSTEXT : IT_CALL));
	}

	// Egg TV
	if (M_SecretUnlocked(SECRET_EGGTV, true))
	{
		EXTRAS_Main[extras_eggtv].status = IT_STRING | IT_CALL;
		EXTRAS_Main[extras_eggtv].text = "Egg TV";
		EXTRAS_Main[extras_eggtv].tooltip = "Watch the replays you've saved throughout your many races & battles!";
	}
	else
	{
		EXTRAS_Main[extras_eggtv].status = IT_STRING | IT_TRANSTEXT;
		EXTRAS_Main[extras_eggtv].text = EXTRAS_Main[extras_eggtv].tooltip = "???";
	}

	// Stereo Mode
	if (M_SecretUnlocked(SECRET_SOUNDTEST, true))
	{
		EXTRAS_Main[extras_stereo].status = IT_STRING | IT_CALL;
		EXTRAS_Main[extras_stereo].text = "Stereo Mode";
		EXTRAS_Main[extras_stereo].tooltip = "You can listen to your favourite tunes here!";
	}
	else
	{
		EXTRAS_Main[extras_stereo].status = IT_STRING | IT_TRANSTEXT;
		EXTRAS_Main[extras_stereo].text = EXTRAS_Main[extras_stereo].tooltip = "???";
	}

	if (choice == -1)
		return;

	extrasmenu.ticker = 0;
	extrasmenu.offset = 0;

	extrasmenu.extx = 0;
	extrasmenu.exty = 0;
	extrasmenu.textx = 0;
	extrasmenu.texty = 0;

	M_SetupNextMenu(&EXTRAS_MainDef, false);
}

// For statistics, will maybe remain unused for a while
boolean M_ExtrasQuit(void)
{
	extrasmenu.textx = 140-1;
	extrasmenu.texty = 70+1;

	return true;	// Always allow quitting, duh.
}

void M_ExtrasTick(void)
{
	extrasmenu.offset /= 2;
	extrasmenu.ticker++;

	extrasmenu.extx += (extrasmenu.textx - extrasmenu.extx)/2;
	extrasmenu.exty += (extrasmenu.texty - extrasmenu.exty)/2;

	if (abs(extrasmenu.extx - extrasmenu.exty) < 2)
	{
		extrasmenu.extx = extrasmenu.textx;
		extrasmenu.exty = extrasmenu.texty;	// Avoid awkward 1 px errors.
	}

	// Move the button for cool animations
	if (currentMenu == &EXTRAS_MainDef)
	{
		M_ExtrasQuit();	// reset the options button.
	}
	else
	{
		extrasmenu.textx = 160;
		extrasmenu.texty = 50;
	}

	if (menutyping.active == false && cv_dummyextraspassword.string[0] != '\0')
	{
		if (M_ConditionInterpret(cv_dummyextraspassword.string) == true)
		{
			if (M_UpdateUnlockablesAndExtraEmblems(true, true))
			{
				M_Challenges(0);
			}
		}
		else if (cht_Interpret(cv_dummyextraspassword.string) == true && menuactive == true)
		{
			M_InitExtras(-1);
		}

		CV_StealthSet(&cv_dummyextraspassword, "");
	}
}

boolean M_ExtrasInputs(INT32 ch)
{

	const UINT8 pid = 0;
	(void) ch;

	if (menucmd[pid].dpad_ud > 0)
	{
		extrasmenu.offset += 48;
		M_NextOpt();
		S_StartSound(NULL, sfx_s3k5b);

		if (itemOn == 0)
			extrasmenu.offset -= currentMenu->numitems*48;

		M_SetMenuDelay(pid);
		return true;
	}

	else if (menucmd[pid].dpad_ud < 0)
	{
		extrasmenu.offset -= 48;
		M_PrevOpt();
		S_StartSound(NULL, sfx_s3k5b);

		if (itemOn == currentMenu->numitems-1)
			extrasmenu.offset += currentMenu->numitems*48;

		M_SetMenuDelay(pid);
		return true;
	}

	else if (M_MenuConfirmPressed(pid))
	{

		if (currentMenu->menuitems[itemOn].status & IT_TRANSTEXT)
			return true;	// No.

		extrasmenu.extx = 140;
		extrasmenu.exty = 70;	// Default position for the currently selected option.

		M_SetMenuDelay(pid);
		return false;	// Don't eat.
	}
	return false;
}
