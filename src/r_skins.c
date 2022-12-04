// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_skins.c
/// \brief Loading skins

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "r_local.h"
#include "st_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_misc.h"
#include "info.h" // spr2names
#include "i_video.h" // rendermode
#include "i_system.h"
#include "r_things.h"
#include "r_skins.h"
#include "p_local.h"
#include "dehacked.h" // get_number (for thok)
#include "m_cond.h"
#include "k_kart.h"
#include "m_random.h"
#if 0
#include "k_kart.h" // K_KartResetPlayerColor
#endif
#include "k_grandprix.h" // K_CanChangeRules
#ifdef HWRENDER
#include "hardware/hw_md2.h"
#endif

INT32 numskins = 0;
skin_t skins[MAXSKINS];

// FIXTHIS: don't work because it must be inistilised before the config load
//#define SKINVALUES
#ifdef SKINVALUES
CV_PossibleValue_t skin_cons_t[MAXSKINS+1];
#endif

CV_PossibleValue_t Forceskin_cons_t[MAXSKINS+2];

//
// P_GetSkinSprite2
// For non-super players, tries each sprite2's immediate predecessor until it finds one with a number of frames or ends up at standing.
// For super players, does the same as above - but tries the super equivalent for each sprite2 before the non-super version.
//

UINT8 P_GetSkinSprite2(skin_t *skin, UINT8 spr2, player_t *player)
{
	UINT8 super = 0, i = 0;

	(void)player;

	if (!skin)
		return 0;

	if ((playersprite_t)(spr2 & ~FF_SPR2SUPER) >= free_spr2)
		return 0;

	while (!skin->sprites[spr2].numframes
		&& spr2 != SPR2_STIL
		&& ++i < 32) // recursion limiter
	{
		if (spr2 & FF_SPR2SUPER)
		{
			super = FF_SPR2SUPER;
			spr2 &= ~FF_SPR2SUPER;
			continue;
		}

		switch(spr2)
		{
			// Normal special cases.
			// (none in kart)

			// Use the handy list, that's what it's there for!
			default:
				spr2 = spr2defaults[spr2];
				break;
		}

		spr2 |= super;
	}

	if (i >= 32) // probably an infinite loop...
		return 0;

	return spr2;
}

static void Sk_SetDefaultValue(skin_t *skin)
{
	INT32 i;
	//
	// set default skin values
	//
	memset(skin, 0, sizeof (skin_t));
	snprintf(skin->name,
		sizeof skin->name, "skin %u", (UINT32)(skin-skins));
	skin->name[sizeof skin->name - 1] = '\0';
	skin->wadnum = INT16_MAX;

	skin->flags = 0;

	strcpy(skin->realname, "Someone");
	skin->starttranscolor = 96;
	skin->prefcolor = SKINCOLOR_GREEN;
	skin->supercolor = SKINCOLOR_SUPERGOLD1;
	skin->prefoppositecolor = 0; // use tables

	skin->kartspeed = 5;
	skin->kartweight = 5;

	skin->followitem = 0;

	skin->highresscale = FRACUNIT;

	for (i = 0; i < sfx_skinsoundslot0; i++)
		if (S_sfx[i].skinsound != -1)
			skin->soundsid[S_sfx[i].skinsound] = i;
}

//
// Initialize the basic skins
//
void R_InitSkins(void)
{
	size_t i;

	// it can be is do before loading config for skin cvar possible value
	// (... what the fuck did you just say to me? "it can be is do"?)
#ifdef SKINVALUES
	for (i = 0; i <= MAXSKINS; i++)
	{
		skin_cons_t[i].value = 0;
		skin_cons_t[i].strvalue = NULL;
	}
#endif

	// no default skin!
	numskins = 0;

	for (i = 0; i < numwadfiles; i++)
	{
		R_AddSkins((UINT16)i, true);
		R_PatchSkins((UINT16)i, true);
		R_LoadSpriteInfoLumps(i, wadfiles[i]->numlumps);
	}
	ST_ReloadSkinFaceGraphics();
}

UINT8 *R_GetSkinAvailabilities(boolean demolock)
{
	UINT8 i, shif, byte;
	INT32 skinid;
	static UINT8 responsebuffer[MAXAVAILABILITY];

	memset(&responsebuffer, 0, sizeof(responsebuffer));

	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		if (unlockables[i].type != SECRET_SKIN)
			continue;

		if (gamedata->unlocked[i] != true && !demolock)
			continue;

		skinid = M_UnlockableSkinNum(&unlockables[i]);

		if (skinid < 0 || skinid >= MAXSKINS)
			continue;

		shif = (skinid % 8);
		byte = (skinid / 8);

		responsebuffer[byte] |= (1 << shif);
	}

	return responsebuffer;
}

// returns true if available in circumstances, otherwise nope
// warning don't use with an invalid skinnum other than -1 which always returns true
boolean R_SkinUsable(INT32 playernum, INT32 skinnum, boolean demoskins)
{
	boolean needsunlocked = false;
	boolean useplayerstruct = (Playing() && playernum != -1);
	UINT8 i;
	INT32 skinid;

	if (skinnum == -1)
	{
		// Simplifies things elsewhere, since there's already plenty of checks for less-than-0...
		return true;
	}

	if (K_CanChangeRules(true) && (cv_forceskin.value == skinnum))
	{
		// Being forced to play as this character by the server
		return true;
	}

	// Determine if this character is supposed to be unlockable or not
	if (useplayerstruct && demo.playback)
	{
		if (!demoskins)
			skinnum = demo.skinlist[skinnum].mapping;
		needsunlocked = demo.skinlist[skinnum].unlockrequired;
	}
	else
	{
		for (i = 0; i < MAXUNLOCKABLES; i++)
		{
			if (unlockables[i].type != SECRET_SKIN)
				continue;

			skinid = M_UnlockableSkinNum(&unlockables[i]);

			if (skinid != skinnum)
				continue;

			// i is now the unlockable index, we can use this later
			needsunlocked = true;
			break;
		}
	}

	if (needsunlocked == false)
	{
		// Didn't trip anything, so we can use this character.
		return true;
	}

	// Ok, you can use this character IF you have it unlocked.
	if (useplayerstruct)
	{
		// Use the netgame synchronized unlocks.
		UINT8 shif = (skinnum % 8);
		UINT8 byte = (skinnum / 8);
		return !!(players[playernum].availabilities[byte] & (1 << shif));
	}

	// Use the unlockables table directly
	return (boolean)(gamedata->unlocked[i]);
}

// returns true if the skin name is found (loaded from pwad)
// warning return -1 if not found
INT32 R_SkinAvailable(const char *name)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
	{
		// search in the skin list
		if (stricmp(skins[i].name,name)==0)
			return i;
	}
	return -1;
}

// Auxillary function that actually sets the skin
static void SetSkin(player_t *player, INT32 skinnum)
{
	skin_t *skin = &skins[skinnum];

	player->skin = skinnum;

	player->followitem = skin->followitem;

	player->kartspeed = skin->kartspeed;
	player->kartweight = skin->kartweight;
	player->charflags = skin->flags;

#if 0
	if (!CV_CheatsEnabled() && !(netgame || multiplayer || demo.playback))
	{
		for (i = 0; i <= r_splitscreen; i++)
		{
			if (playernum == g_localplayers[i])
			{
				CV_StealthSetValue(&cv_playercolor[i], skin->prefcolor);
			}
		}

		player->skincolor = skin->prefcolor;
		K_KartResetPlayerColor(player);
	}
#endif

	if (player->followmobj)
	{
		P_RemoveMobj(player->followmobj);
		P_SetTarget(&player->followmobj, NULL);
	}

	if (player->mo)
	{
		player->mo->skin = skin;
		P_SetScale(player->mo, player->mo->scale);
		P_SetPlayerMobjState(player->mo, player->mo->state-states); // Prevent visual errors when switching between skins with differing number of frames
	}

	// for replays: We have changed our skin mid-game; let the game know so it can do the same in the replay!
	demo_extradata[(player-players)] |= DXD_SKIN;
}

// Gets the player to the first usuable skin in the game.
// (If your mod locked them all, then you kinda stupid)
static INT32 GetPlayerDefaultSkin(INT32 playernum)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
	{
		if (R_SkinUsable(playernum, i, false))
		{
			return i;
		}
	}

	I_Error("All characters are locked!");
	return 0;
}

// network code calls this when a 'skin change' is received
void SetPlayerSkin(INT32 playernum, const char *skinname)
{
	INT32 i = R_SkinAvailable(skinname);
	player_t *player = &players[playernum];

	if ((i != -1) && R_SkinUsable(playernum, i, false))
	{
		SetSkin(player, i);
		return;
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Skin '%s' not found.\n"), skinname);
	else if(server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, M_GetText("Player %d (%s) skin '%s' not found\n"), playernum, player_names[playernum], skinname);

	SetSkin(player, GetPlayerDefaultSkin(playernum));
}

// Same as SetPlayerSkin, but uses the skin #.
// network code calls this when a 'skin change' is received
void SetPlayerSkinByNum(INT32 playernum, INT32 skinnum)
{
	player_t *player = &players[playernum];

	if (skinnum >= 0 && skinnum < numskins && R_SkinUsable(playernum, skinnum, false)) // Make sure it exists!
	{
		SetSkin(player, skinnum);
		return;
	}

	if (P_IsLocalPlayer(player))
		CONS_Alert(CONS_WARNING, M_GetText("Requested skin %d not found\n"), skinnum);
	else if(server || IsPlayerAdmin(consoleplayer))
		CONS_Alert(CONS_WARNING, "Player %d (%s) skin %d not found\n", playernum, player_names[playernum], skinnum);

	SetSkin(player, GetPlayerDefaultSkin(playernum)); // not found put the eggman skin
}

// Set mo skin but not player_t skin, for ironman
void SetFakePlayerSkin(player_t* player, INT32 skinid)
{
	if (player->fakeskin != skinid)
	{
		if (player->fakeskin != MAXSKINS)
			player->lastfakeskin = player->fakeskin;
		player->fakeskin = skinid;
	}

	if (demo.playback)
	{
		player->kartspeed = demo.skinlist[skinid].kartspeed;
		player->kartweight = demo.skinlist[skinid].kartweight;
		player->charflags = demo.skinlist[skinid].flags;
		skinid = demo.skinlist[skinid].mapping;
	}
	else
	{
		player->kartspeed = skins[skinid].kartspeed;
		player->kartweight = skins[skinid].kartweight;
		player->charflags = skins[skinid].flags;
	}

	player->mo->skin = &skins[skinid];
}

// Loudly rerandomize
void SetRandomFakePlayerSkin(player_t* player, boolean fast)
{
	INT32 i;
	UINT8 usableskins = 0, maxskinpick;
	UINT8 grabskins[MAXSKINS];

	maxskinpick = (demo.playback ? demo.numskins : numskins);

	for (i = 0; i < maxskinpick; i++)
	{
		if (i == player->lastfakeskin)
			continue;
		if (demo.playback)
		{
			if (demo.skinlist[i].flags & SF_IRONMAN)
				continue;
		}
		else if (skins[i].flags & SF_IRONMAN)
			continue;
		if (!R_SkinUsable(player-players, i, true))
			continue;
		grabskins[usableskins++] = i;
	}

	if (!usableskins)
		I_Error("SetRandomFakePlayerSkin: No valid skins to pick from!?");

	i = grabskins[P_RandomKey(PR_RANDOMSKIN, usableskins)];

	SetFakePlayerSkin(player, i);

	if (player->mo)
	{
		S_StartSound(player->mo, sfx_kc33);
		S_StartSound(player->mo, sfx_cdfm44);
		
		mobj_t *parent = player->mo;
		fixed_t baseangle = P_RandomRange(PR_DECORATION, 0, 359);
		INT32 j;

		for (j = 0; j < 6; j++)	// 0-3 = sides, 4 = top, 5 = bottom
		{
			mobj_t *box = P_SpawnMobjFromMobj(parent, 0, 0, 0, MT_MAGICIANBOX);
			P_SetTarget(&box->target, parent);
			box->angle = FixedAngle((baseangle + j*90) * FRACUNIT);
			box->flags2 |= MF2_AMBUSH;
			if (fast)
			{
				box->extravalue1 = 10; // Rotation rate
				box->extravalue2 = 5*TICRATE/4; // Lifetime
			}
			else
			{
				box->extravalue1 = 1;
				box->extravalue2 = 3*TICRATE/2;
			}

			// cusval controls behavior that should run only once, like disappear FX and RF_DONTDRAW handling.
			// NB: Order of thinker execution matters here!
			// We want the other sides to inherit the player's "existing" RF_DONTDRAW before the last side writes to it.
			// See the MT_MAGICIANBOX thinker in p_mobj.c.
			if (j == 5)
				box->cusval = 1;
			else
				box->cusval = 0;
			
			if (j > 3)
			{
				P_SetMobjState(box, (j == 4) ? S_MAGICIANBOX_TOP : S_MAGICIANBOX_BOTTOM);
				box->renderflags |= RF_NOSPLATBILLBOARD;
				box->angle = FixedAngle(baseangle*FRACUNIT);
			}
		}

		K_SpawnMagicianParticles(player->mo, 10);
	}
}

// Return to base skin from an SF_IRONMAN randomization
void ClearFakePlayerSkin(player_t* player)
{
	UINT8 skinid;
	UINT32 flags;

	if (demo.playback)
	{
		skinid = demo.currentskinid[(player-players)];
		flags = demo.skinlist[skinid].flags;
	}
	else
	{
		skinid = player->skin;
		flags = skins[player->skin].flags;
	}

	if ((flags & SF_IRONMAN) && !P_MobjWasRemoved(player->mo))
	{
		SetFakePlayerSkin(player, skinid);
		S_StartSound(player->mo, sfx_s3k9f);
		K_SpawnMagicianParticles(player->mo, 5);
	}

	player->fakeskin = MAXSKINS;
}

// Finds a skin with the closest stats if the expected skin doesn't exist.
INT32 GetSkinNumClosestToStats(UINT8 kartspeed, UINT8 kartweight, UINT32 flags, boolean unlock)
{
	INT32 i, closest_skin = 0;
	UINT8 closest_stats, stat_diff;
	boolean doflagcheck = true;
	UINT32 flagcheck = flags;

flaglessretry:
	closest_stats = stat_diff = UINT8_MAX;

	for (i = 0; i < numskins; i++)
	{
		if (!unlock && !R_SkinUsable(-1, i, false))
		{
			continue;
		}
		stat_diff = abs(skins[i].kartspeed - kartspeed) + abs(skins[i].kartweight - kartweight);
		if (doflagcheck && (skins[i].flags & flagcheck) != flagcheck)
		{
			continue;
		}
		if (stat_diff < closest_stats)
		{
			closest_stats = stat_diff;
			closest_skin = i;
		}
	}

	if (stat_diff && (doflagcheck || closest_stats == UINT8_MAX))
	{
		// Just grab *any* SF_IRONMAN if we don't get it on the first pass.
		if ((flagcheck & SF_IRONMAN) && (flagcheck != SF_IRONMAN))
		{
			flagcheck = SF_IRONMAN;
		}

		doflagcheck = false;

		goto flaglessretry;
	}

	return closest_skin;
}

//
// Add skins from a pwad, each skin preceded by 'S_SKIN' marker
//

// Does the same is in w_wad, but check only for
// the first 6 characters (this is so we can have S_SKIN1, S_SKIN2..
// for wad editors that don't like multiple resources of the same name)
//
static UINT16 W_CheckForSkinMarkerInPwad(UINT16 wadid, UINT16 startlump)
{
	UINT16 i;
	const char *S_SKIN = "S_SKIN";
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;
		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
			if (memcmp(lump_p->name,S_SKIN,6)==0)
				return i;
	}
	return INT16_MAX; // not found
}

// turn _ into spaces and . into katana dot
#define SYMBOLCONVERT(name) for (value = name; *value; value++)\
	{\
		if (*value == '_') *value = ' ';\
	}

//
// Patch skins from a pwad, each skin preceded by 'P_SKIN' marker
//

// Does the same is in w_wad, but check only for
// the first 6 characters (this is so we can have P_SKIN1, P_SKIN2..
// for wad editors that don't like multiple resources of the same name)
//
static UINT16 W_CheckForPatchSkinMarkerInPwad(UINT16 wadid, UINT16 startlump)
{
	UINT16 i;
	const char *P_SKIN = "P_SKIN";
	lumpinfo_t *lump_p;

	// scan forward, start at <startlump>
	if (startlump < wadfiles[wadid]->numlumps)
	{
		lump_p = wadfiles[wadid]->lumpinfo + startlump;
		for (i = startlump; i < wadfiles[wadid]->numlumps; i++, lump_p++)
			if (memcmp(lump_p->name,P_SKIN,6)==0)
				return i;
	}
	return INT16_MAX; // not found
}

static void R_LoadSkinSprites(UINT16 wadnum, UINT16 *lump, UINT16 *lastlump, skin_t *skin)
{
	UINT16 newlastlump;
	UINT8 sprite2;

	*lump += 1; // start after S_SKIN
	*lastlump = W_CheckNumForNamePwad("S_END",wadnum,*lump); // stop at S_END

	// old wadding practices die hard -- stop at S_SKIN (or P_SKIN) or S_START if they come before S_END.
	newlastlump = W_FindNextEmptyInPwad(wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;
	newlastlump = W_CheckForSkinMarkerInPwad(wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;
	newlastlump = W_CheckForPatchSkinMarkerInPwad(wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;
	newlastlump = W_CheckNumForNamePwad("S_START",wadnum,*lump);
	if (newlastlump < *lastlump) *lastlump = newlastlump;

	/*// ...and let's handle super, too
	newlastlump = W_CheckNumForNamePwad("S_SUPER",wadnum,*lump);
	if (newlastlump < *lastlump)
	{
		newlastlump++;
		// load all sprite sets we are aware of... for super!
		for (sprite2 = 0; sprite2 < free_spr2; sprite2++)
			R_AddSingleSpriteDef(spr2names[sprite2], &skin->sprites[FF_SPR2SUPER|sprite2], wadnum, newlastlump, *lastlump);

		newlastlump--;
		*lastlump = newlastlump; // okay, make the normal sprite set loading end there
	}*/

	// load all sprite sets we are aware of... for normal stuff.
	for (sprite2 = 0; sprite2 < free_spr2; sprite2++)
		R_AddSingleSpriteDef(spr2names[sprite2], &skin->sprites[sprite2], wadnum, *lump, *lastlump);

	if (skin->sprites[0].numframes == 0)
		I_Error("R_LoadSkinSprites: no frames found for sprite SPR2_%s\n", spr2names[0]);
}

// returns whether found appropriate property
static boolean R_ProcessPatchableFields(skin_t *skin, char *stoken, char *value)
{
	// custom translation table
	if (!stricmp(stoken, "startcolor"))
		skin->starttranscolor = atoi(value);

#define FULLPROCESS(field) else if (!stricmp(stoken, #field)) skin->field = get_number(value);
	// character type identification
	FULLPROCESS(flags)
	FULLPROCESS(followitem)
#undef FULLPROCESS

#define GETSKINCOLOR(field) else if (!stricmp(stoken, #field)) \
{ \
	UINT16 color = R_GetColorByName(value); \
	skin->field = (color ? color : SKINCOLOR_GREEN); \
}
	GETSKINCOLOR(prefcolor)
	GETSKINCOLOR(prefoppositecolor)
#undef GETSKINCOLOR
	else if (!stricmp(stoken, "supercolor"))
	{
		UINT16 color = R_GetSuperColorByName(value);
		skin->supercolor = (color ? color : SKINCOLOR_SUPERGOLD1);
	}

#define GETFLOAT(field) else if (!stricmp(stoken, #field)) skin->field = FLOAT_TO_FIXED(atof(value));
	GETFLOAT(highresscale)
#undef GETFLOAT

#define GETKARTSTAT(field) \
	else if (!stricmp(stoken, #field)) \
	{ \
		skin->field = atoi(value); \
		if (skin->field < 1) skin->field = 1; \
		if (skin->field > 9) skin->field = 9; \
	}
	GETKARTSTAT(kartspeed)
	GETKARTSTAT(kartweight)
#undef GETKARTSTAT


#define GETFLAG(field) else if (!stricmp(stoken, #field)) { \
	strupr(value); \
	if (atoi(value) || value[0] == 'T' || value[0] == 'Y') \
		skin->flags |= (SF_##field); \
	else \
		skin->flags &= ~(SF_##field); \
}
	// parameters for individual character flags
	// these are uppercase so they can be concatenated with SF_
	// 1, true, yes are all valid values
	GETFLAG(MACHINE)
	GETFLAG(IRONMAN)
#undef GETFLAG

	else // let's check if it's a sound, otherwise error out
	{
		boolean found = false;
		sfxenum_t i;
		size_t stokenadjust;

		// Remove the prefix. (We need to affect an adjusting variable so that we can print error messages if it's not actually a sound.)
		if ((stoken[0] == 'D' || stoken[0] == 'd') && (stoken[1] == 'S' || stoken[1] == 's')) // DS*
			stokenadjust = 2;
		else // sfx_*
			stokenadjust = 4;

		// Remove the prefix. (We can affect this directly since we're not going to use it again.)
		if ((value[0] == 'D' || value[0] == 'd') && (value[1] == 'S' || value[1] == 's')) // DS*
			value += 2;
		else // sfx_*
			value += 4;

		// copy name of sounds that are remapped
		// for this skin
		for (i = 0; i < sfx_skinsoundslot0; i++)
		{
			if (!S_sfx[i].name)
				continue;
			if (S_sfx[i].skinsound != -1
				&& !stricmp(S_sfx[i].name,
					stoken + stokenadjust))
			{
				skin->soundsid[S_sfx[i].skinsound] =
					S_AddSoundFx(value, S_sfx[i].singularity, S_sfx[i].pitch, true);
				found = true;
			}
		}
		return found;
	}
	return true;
}

//
// Find skin sprites, sounds & optional status bar face, & add them
//
void R_AddSkins(UINT16 wadnum, boolean mainfile)
{
	UINT16 lump, lastlump = 0;
	char *buf;
	char *buf2;
	char *stoken;
	char *value;
	size_t size;
	skin_t *skin;
	boolean realname;

	//
	// search for all skin markers in pwad
	//

	while ((lump = W_CheckForSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		// advance by default
		lastlump = lump + 1;

		if (numskins >= MAXSKINS)
		{
			CONS_Debug(DBG_RENDER, "ignored skin (%d skins maximum)\n", MAXSKINS);
			continue; // so we know how many skins couldn't be added
		}
		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_AddSkins: No more free memory\n");
		M_Memcpy(buf2,buf,size);
		buf2[size] = '\0';

		// set defaults
		skin = &skins[numskins];
		Sk_SetDefaultValue(skin);
		skin->wadnum = wadnum;
		realname = false;
		// parse
		stoken = strtok (buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n"); // skip end of line
				goto next_token;              // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_AddSkins: syntax error in S_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			// Some of these can't go in R_ProcessPatchableFields because they have side effects for future lines.
			// Others can't go in there because we don't want them to be patchable.
			if (!stricmp(stoken, "name"))
			{
				INT32 skinnum = R_SkinAvailable(value);
				strlwr(value);
				if (skinnum == -1)
					STRBUFCPY(skin->name, value);
				// the skin name must uniquely identify a single skin
				// if the name is already used I make the name 'namex'
				// using the default skin name's number set above
				else
				{
					const size_t stringspace =
						strlen(value) + sizeof (numskins) + 1;
					char *value2 = Z_Malloc(stringspace, PU_STATIC, NULL);
					snprintf(value2, stringspace,
						"%s%d", value, numskins);
					value2[stringspace - 1] = '\0';
					if (R_SkinAvailable(value2) == -1)
						// I'm lazy so if NEW name is already used I leave the 'skin x'
						// default skin name set in Sk_SetDefaultValue
						STRBUFCPY(skin->name, value2);
					Z_Free(value2);
				}

				// copy to hudname and fullname as a default.
				if (!realname)
				{
					STRBUFCPY(skin->realname, skin->name);
					SYMBOLCONVERT(skin->realname);
				}
			}
			else if (!stricmp(stoken, "realname"))
			{ // Display name (eg. "Knuckles")
				realname = true;
				STRBUFCPY(skin->realname, value);
				SYMBOLCONVERT(skin->realname)
			}
			else if (!stricmp(stoken, "rivals"))
			{
				size_t len = strlen(value);
				size_t i;
				char rivalname[SKINNAMESIZE] = "";
				UINT8 pos = 0;
				UINT8 numrivals = 0;

				// Can't use strtok, because this function's already using it.
				// Using it causes it to upset the saved pointer,
				// corrupting the reading for the rest of the file.

				// So instead we get to crawl through the value, character by character,
				// and write it down as we go, until we hit a comma or the end of the string.
				// Yaaay.

				for (i = 0; i <= len; i++)
				{
					if (numrivals >= SKINRIVALS)
					{
						break;
					}

					if (value[i] == ',' || i == len)
					{
						STRBUFCPY(skin->rivals[numrivals], rivalname);
						strlwr(skin->rivals[numrivals]);
						numrivals++;

						memset(rivalname, 0, sizeof (rivalname));
						pos = 0;

						continue;
					}

					rivalname[pos] = value[i];
					pos++;
				}
			}
			else if (!R_ProcessPatchableFields(skin, stoken, value))
				CONS_Debug(DBG_SETUP, "R_AddSkins: Unknown keyword '%s' in S_SKIN lump #%d (WAD %s)\n", stoken, lump, wadfiles[wadnum]->filename);

next_token:
			stoken = strtok(NULL, "\r\n= ");
		}
		free(buf2);

		// Add sprites
		R_LoadSkinSprites(wadnum, &lump, &lastlump, skin);
		//ST_LoadFaceGraphics(numskins); -- nah let's do this elsewhere

		R_FlushTranslationColormapCache();

		if (mainfile == false)
			CONS_Printf(M_GetText("Added skin '%s'\n"), skin->name);

#ifdef SKINVALUES
		skin_cons_t[numskins].value = numskins;
		skin_cons_t[numskins].strvalue = skin->name;
#endif

		// Update the forceskin possiblevalues
		Forceskin_cons_t[numskins+1].value = numskins;
		Forceskin_cons_t[numskins+1].strvalue = skins[numskins].name;

#ifdef HWRENDER
		if (rendermode == render_opengl)
			HWR_AddPlayerModel(numskins);
#endif

		numskins++;
	}
	return;
}

//
// Patch skin sprites
//
void R_PatchSkins(UINT16 wadnum, boolean mainfile)
{
	UINT16 lump, lastlump = 0;
	char *buf;
	char *buf2;
	char *stoken;
	char *value;
	size_t size;
	skin_t *skin;
	boolean noskincomplain, realname;

	//
	// search for all skin patch markers in pwad
	//

	while ((lump = W_CheckForPatchSkinMarkerInPwad(wadnum, lastlump)) != INT16_MAX)
	{
		INT32 skinnum = 0;

		// advance by default
		lastlump = lump + 1;

		buf = W_CacheLumpNumPwad(wadnum, lump, PU_CACHE);
		size = W_LumpLengthPwad(wadnum, lump);

		// for strtok
		buf2 = malloc(size+1);
		if (!buf2)
			I_Error("R_PatchSkins: No more free memory\n");
		M_Memcpy(buf2,buf,size);
		buf2[size] = '\0';

		skin = NULL;
		noskincomplain = realname = false;

		/*
		Parse. Has more phases than the parser in R_AddSkins because it needs to have the patching name first (no default skin name is acceptible for patching, unlike skin creation)
		*/

		stoken = strtok(buf2, "\r\n= ");
		while (stoken)
		{
			if ((stoken[0] == '/' && stoken[1] == '/')
				|| (stoken[0] == '#'))// skip comments
			{
				stoken = strtok(NULL, "\r\n"); // skip end of line
				goto next_token;              // find the real next token
			}

			value = strtok(NULL, "\r\n= ");

			if (!value)
				I_Error("R_PatchSkins: syntax error in P_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);

			if (!skin) // Get the name!
			{
				if (!stricmp(stoken, "name"))
				{
					strlwr(value);
					skinnum = R_SkinAvailable(value);
					if (skinnum != -1)
						skin = &skins[skinnum];
					else
					{
						CONS_Debug(DBG_SETUP, "R_PatchSkins: unknown skin name in P_SKIN lump# %d(%s) in WAD %s\n", lump, W_CheckNameForNumPwad(wadnum,lump), wadfiles[wadnum]->filename);
						noskincomplain = true;
					}
				}
			}
			else // Get the properties!
			{
				// Some of these can't go in R_ProcessPatchableFields because they have side effects for future lines.
				if (!stricmp(stoken, "realname"))
				{ // Display name (eg. "Knuckles")
					realname = true;
					STRBUFCPY(skin->realname, value);
					SYMBOLCONVERT(skin->realname)
				}
				else if (!stricmp(stoken, "rivals"))
				{
					size_t len = strlen(value);
					size_t i;
					char rivalname[SKINNAMESIZE] = "";
					UINT8 pos = 0;
					UINT8 numrivals = 0;

					// Can't use strtok, because this function's already using it.
					// Using it causes it to upset the saved pointer,
					// corrupting the reading for the rest of the file.

					// So instead we get to crawl through the value, character by character,
					// and write it down as we go, until we hit a comma or the end of the string.
					// Yaaay.

					for (i = 0; i <= len; i++)
					{
						if (numrivals >= SKINRIVALS)
						{
							break;
						}

						if (value[i] == ',' || i == len)
						{
							STRBUFCPY(skin->rivals[numrivals], rivalname);
							strlwr(skin->rivals[numrivals]);
							numrivals++;

							memset(rivalname, 0, sizeof (rivalname));
							pos = 0;

							continue;
						}

						rivalname[pos] = value[i];
						pos++;
					}
				}
				else if (!R_ProcessPatchableFields(skin, stoken, value))
					CONS_Debug(DBG_SETUP, "R_PatchSkins: Unknown keyword '%s' in P_SKIN lump #%d (WAD %s)\n", stoken, lump, wadfiles[wadnum]->filename);
			}

			if (!skin)
				break;

next_token:
			stoken = strtok(NULL, "\r\n= ");
		}
		free(buf2);

		if (!skin) // Didn't include a name parameter? What a waste.
		{
			if (!noskincomplain)
				CONS_Debug(DBG_SETUP, "R_PatchSkins: no skin name given in P_SKIN lump #%d (WAD %s)\n", lump, wadfiles[wadnum]->filename);
			continue;
		}

		// Patch sprites
		R_LoadSkinSprites(wadnum, &lump, &lastlump, skin);
		//ST_LoadFaceGraphics(skinnum); -- nah let's do this elsewhere

		R_FlushTranslationColormapCache();

		if (mainfile == false)
			CONS_Printf(M_GetText("Patched skin '%s'\n"), skin->name);
	}
	return;
}

#undef SYMBOLCONVERT
