// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2007-2016 by John "JTE" Muniz.
// Copyright (C) 2011-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  b_bot.c
/// \brief Basic bot handling

#include "doomdef.h"
#include "d_player.h"
#include "g_game.h"
#include "r_main.h"
#include "p_local.h"
#include "b_bot.h"
#include "lua_hook.h"

// If you want multiple bots, variables like this will
// have to be stuffed in something accessible through player_t.
static boolean lastForward = false;
static boolean lastBlocked = false;
static boolean blocked = false;

static boolean jump_last = false;
static boolean spin_last = false;
static UINT8 anxiety = 0;
static boolean panic = false;
static UINT8 flymode = 0;
static boolean spinmode = false;
static boolean thinkfly = false;
static mobj_t *overlay;

static inline void B_BuildTailsTiccmd(mobj_t *sonic, mobj_t *tails, ticcmd_t *cmd)
{
	boolean forward=false, backward=false, left=false, right=false, jump=false, spin=false;
	
	player_t *player = sonic->player, *bot = tails->player;
	ticcmd_t *pcmd = &player->cmd;
	boolean water = tails->eflags & MFE_UNDERWATER;
	boolean flip = P_MobjFlip(tails);
	boolean _2d = (tails->flags2 & MF2_TWOD) || twodlevel;
	fixed_t scale = tails->scale;
	
	fixed_t dist = P_AproxDistance(sonic->x - tails->x, sonic->y - tails->y);
	fixed_t zdist = flip * (sonic->z - tails->z);
	angle_t ang = R_PointToAngle2(tails->x, tails->y, sonic->x, sonic->y);
	fixed_t pmom = P_AproxDistance(sonic->momx, sonic->momy);
	fixed_t bmom = P_AproxDistance(tails->momx, tails->momy);
	fixed_t followmax = 128 * 8 *scale;
	fixed_t followthres = 92 * scale;
	fixed_t followmin = 32 * scale;
	fixed_t comfortheight = 96 * scale;
	fixed_t touchdist = 24 * scale;

	// We can't follow Sonic if he's not around!
	if (!sonic || sonic->health <= 0)
		return;

#ifdef HAVE_BLUA
	// Lua can handle it!
	if (LUAh_BotAI(sonic, tails, cmd))
		return;
#endif

	if (tails->player->powers[pw_carry] == CR_MACESPIN || tails->player->powers[pw_carry] == CR_GENERIC)
	{
		boolean isrelevant = (sonic->player->powers[pw_carry] == CR_MACESPIN || sonic->player->powers[pw_carry] == CR_GENERIC);
		dist = P_AproxDistance(tails->x-sonic->x, tails->y-sonic->y);
		if (sonic->player->cmd.buttons & BT_JUMP && (sonic->player->pflags & PF_JUMPED) && isrelevant)
			cmd->buttons |= BT_JUMP;
		if (isrelevant)
		{
			cmd->forwardmove = sonic->player->cmd.forwardmove;
			cmd->angleturn = abs((signed)(tails->angle - sonic->angle))>>16;
			if (sonic->angle < tails->angle)
				cmd->angleturn = -cmd->angleturn;
		} else if (dist > FixedMul(512*FRACUNIT, tails->scale))
			cmd->buttons |= BT_JUMP;
		return;
	}

	// Adapted from CobaltBW's tails_AI.wad
	
	// Check water
	if (water)
	{
		followmin = 0;
		followthres = 16*scale;
		followmax >>= 1;
		thinkfly = false;
	}
	
	// Check anxiety
	if (spinmode)
	{
		anxiety = 0;
		panic = false;
	}
	else if (dist > followmax || zdist > comfortheight)
	{
		anxiety = min(anxiety + 2, 70);
		if (anxiety >= 70)
			panic = true;
	}
	else
	{
		anxiety = max(anxiety - 1, 0);
		panic = false;
	}
	
	// Orientation
	if ((bot->pflags & (PF_SPINNING|PF_STARTDASH)) || flymode == 2)
	{
		tails->angle = sonic->angle;
	}
	else
	{
		tails->angle = ang;
	}
	
	// ********
	// FLY MODE
	// spinmode check
	if (spinmode)
		thinkfly = false;
	else
	{
		// Activate co-op flight
		if (thinkfly && player->pflags & PF_JUMPED)
		{
			if (!jump_last)
			{
				jump = true;
				if (bot->pflags & PF_JUMPED)
				{
					flymode = 1;
					thinkfly = false;
				}
			}
		}
		
		// Check positioning
		// Thinker for co-op flight
		if (!(water || pmom || bmom)
			&& (dist < touchdist)
			&& !(pcmd->forwardmove || pcmd->sidemove || player->dashspeed)
			&& P_IsObjectOnGround(sonic) && P_IsObjectOnGround(tails)
			&& !(player->pflags & PF_STASIS))
				thinkfly = true;
		else
			thinkfly = false;
		
		// Ready for takeoff
		if (flymode == 1)
		{
			thinkfly = false;
			if (zdist < -64*scale || (flip * tails->momz) > scale) // Make sure we're not too high up
				spin = true;
			else if (!jump_last)
				jump = true;
			
			// Abort if the player moves away or spins
			if (dist > followthres || player->dashspeed)
				flymode = 0;
			
			// Set carried state
			if (bot->pflags & PF_THOKKED && flymode == 1
				&& !P_IsObjectOnGround(sonic)
				&& dist < touchdist
				&& zdist < 32*scale
				&& flip * sonic->momz < 0)
			{
				P_SetTarget(&sonic->tracer, tails);
				player->powers[pw_carry] = CR_PLAYER;
				flymode = 2;
				player->pflags &= ~PF_JUMPED;
			}
		}
		// Read player inputs
		else if (flymode == 2)
		{
			cmd->forwardmove = pcmd->forwardmove;
			cmd->sidemove = pcmd->sidemove;
			if (pcmd->buttons & BT_USE)
			{
				spin = true;
				jump = false;
			}
			else if (!jump_last)
				jump = true;
			// End flymode
			if (player->pflags & PF_JUMPED
				|| player->powers[pw_carry] != CR_PLAYER
				|| P_IsObjectOnGround(sonic))
			{
				player->powers[pw_carry] = CR_NONE;
				P_SetTarget(&sonic->tracer, NULL);
				flymode = 0;
			}
		}
	}
	
	if (flymode && P_IsObjectOnGround(tails) && !(pcmd->buttons & BT_JUMP))
		flymode = 0;
	
	// ********
	// SPINNING
	if (panic || flymode || !(player->pflags & PF_SPINNING) || (player->pflags & PF_JUMPED))
		spinmode = false;
	else
	{
		if (!_2d)
		{
			// Spindash
			if (player->dashspeed)
			{
				if (dist < followthres && dist > touchdist) // Do positioning
				{
					tails->angle = ang;
					cmd->forwardmove = 50;
					spinmode = true;
				}
				else if (dist < touchdist && !bmom
					&& (!(bot->pflags & PF_SPINNING) || (bot->dashspeed && bot->pflags & PF_SPINNING)))
				{
					tails->angle = sonic->angle;
					spin = true;
					spinmode = true;
				}
				else
					spinmode = 0;
			}
			// Spin
			else if (player->dashspeed == bot->dashspeed && player->pflags & PF_SPINNING)
			{
				if (bot->pflags & PF_SPINNING || !spin_last)
				{
					spin = true;
					tails->angle = sonic->angle;
					cmd->forwardmove = MAXPLMOVE;
					spinmode = true;
				}
				else
					spinmode = false;
			}
		}
		// 2D mode
		else
		{
			if ((player->dashspeed && !bmom) || (player->dashspeed == bot->dashspeed && player->pflags & PF_SPINNING))
			{
				spin = true;
				spinmode = true;
			}
		}
	}
	
	// ********
	// FOLLOW
	if (!(flymode || spinmode))
	{
		// Too far
		if (panic || dist > followthres)
		{
			if (!_2d)
				cmd->forwardmove = MAXPLMOVE;
			else if (sonic->x > tails->x)
				cmd->sidemove = MAXPLMOVE;
			else
				cmd->sidemove = -MAXPLMOVE;
		}
		// Within threshold
		else if (!panic && dist > followmin && abs(zdist) < 192*scale)
		{
			if (!_2d)
				cmd->forwardmove = FixedHypot(pcmd->forwardmove, pcmd->sidemove);
			else
				cmd->sidemove = pcmd->sidemove;
		}
		// Below min
		else if (dist < followmin)
		{
			// Copy inputs
			tails->angle = sonic->angle;
			bot->drawangle = player->drawangle;
			cmd->forwardmove = 8 * pcmd->forwardmove / 10;
			cmd->sidemove = 8 * pcmd->sidemove / 10;
		}
	}
	
	// ********
	// JUMP
	if (!(flymode || spinmode))
	{
		// Flying catch-up
		if (bot->pflags & PF_THOKKED)
		{
			cmd->forwardmove = min(50, dist/scale/8);
			if (zdist < -64*scale)
				spin = true;
			else if (zdist > 0 && !jump_last)
				jump = true;
		}
		
		// Just landed
		if (tails->eflags & MFE_JUSTHITFLOOR)
			jump = false;
		// Start jump
		else if (!jump_last && !(bot->pflags & PF_JUMPED) && !(player->pflags & PF_SPINNING)
			&& ((zdist > 32*scale && player->pflags & PF_JUMPED) // Following
				|| (zdist > 64*scale && panic) // Vertical catch-up
				|| (bmom < scale>>3 && dist > followthres && !(bot->powers[pw_carry])) // Stopped & not in carry state
				|| (bot->pflags & PF_SPINNING && !(bot->pflags & PF_JUMPED)))) // Spinning
					jump = true;
		// Hold jump
		else if (bot->pflags & PF_JUMPED && jump_last && tails->momz*flip > 0 && (zdist > 0 || panic))
			jump = true;
		// Start flying
		else if (bot->pflags & PF_JUMPED && panic && !jump_last)
			jump = true;
	}
	
	// ********
	// HISTORY
	jump_last = jump;
	spin_last = spin;
	
	// ********
	// Thinkfly overlay
		// doing this later :P

	// Turn the virtual keypresses into ticcmd_t.
	B_KeysToTiccmd(tails, cmd, forward, backward, left, right, false, false, jump, spin);

	// Update our status
	lastForward = forward;
	lastBlocked = blocked;
	blocked = false;
}

void B_BuildTiccmd(player_t *player, ticcmd_t *cmd)
{
	// Can't build a ticcmd if we aren't spawned...
	if (!player->mo)
		return;

	if (player->playerstate == PST_DEAD)
	{
		if (B_CheckRespawn(player))
			cmd->buttons |= BT_JUMP;
		return;
	}

	// Bot AI isn't programmed in analog.
	CV_SetValue(&cv_analog2, false);

#ifdef HAVE_BLUA
	// Let Lua scripts build ticcmds
	if (LUAh_BotTiccmd(player, cmd))
		return;
#endif

	// We don't have any main character AI, sorry. D:
	if (player-players == consoleplayer)
		return;

	// Basic Tails AI
	B_BuildTailsTiccmd(players[consoleplayer].mo, player->mo, cmd);
}

void B_KeysToTiccmd(mobj_t *mo, ticcmd_t *cmd, boolean forward, boolean backward, boolean left, boolean right, boolean strafeleft, boolean straferight, boolean jump, boolean spin)
{
	// Turn the virtual keypresses into ticcmd_t.
	if (twodlevel || mo->flags2 & MF2_TWOD) {
		if (players[consoleplayer].climbing
		|| mo->player->pflags & PF_GLIDING) {
			// Don't mess with bot inputs during these unhandled movement conditions.
			// The normal AI doesn't use abilities, so custom AI should be sending us exactly what it wants anyway.
			if (forward)
				cmd->forwardmove += MAXPLMOVE<<FRACBITS>>16;
			if (backward)
				cmd->forwardmove -= MAXPLMOVE<<FRACBITS>>16;
			if (left || strafeleft)
				cmd->sidemove -= MAXPLMOVE<<FRACBITS>>16;
			if (right || straferight)
				cmd->sidemove += MAXPLMOVE<<FRACBITS>>16;
		} else {
			// In standard 2D mode, interpret "forward" as "the way you're facing" and everything else as "the way you're not facing"
			if (left || right)
				backward = true;
			left = right = false;
			if (forward) {
				if (mo->angle < ANGLE_90 || mo->angle > ANGLE_270)
					right = true;
				else
					left = true;
			} else if (backward) {
				if (mo->angle < ANGLE_90 || mo->angle > ANGLE_270)
					left = true;
				else
					right = true;
			}
			if (left || strafeleft)
				cmd->sidemove -= MAXPLMOVE<<FRACBITS>>16;
			if (right || straferight)
				cmd->sidemove += MAXPLMOVE<<FRACBITS>>16;
		}
	} else {
		if (forward)
			cmd->forwardmove += MAXPLMOVE<<FRACBITS>>16;
		if (backward)
			cmd->forwardmove -= MAXPLMOVE<<FRACBITS>>16;
		if (left)
			cmd->angleturn += 1280;
		if (right)
			cmd->angleturn -= 1280;
		if (strafeleft)
			cmd->sidemove -= MAXPLMOVE<<FRACBITS>>16;
		if (straferight)
			cmd->sidemove += MAXPLMOVE<<FRACBITS>>16;
		
		// cap inputs so the bot can't accelerate faster diagonally
		angle_t angle = R_PointToAngle2(0, 0, cmd->sidemove << FRACBITS, cmd->forwardmove << FRACBITS);
		INT32 maxforward = abs(P_ReturnThrustY(NULL, angle, MAXPLMOVE));
		INT32 maxside = abs(P_ReturnThrustX(NULL, angle, MAXPLMOVE));
		cmd->forwardmove = max(min(cmd->forwardmove, maxforward), -maxforward);
		cmd->sidemove = max(min(cmd->sidemove, maxside), -maxside);
	}
	if (jump)
		cmd->buttons |= BT_JUMP;
	if (spin)
		cmd->buttons |= BT_USE;
}

void B_MoveBlocked(player_t *player)
{
	(void)player;
	blocked = true;
}

boolean B_CheckRespawn(player_t *player)
{
	mobj_t *sonic = players[consoleplayer].mo;
	mobj_t *tails = player->mo;

	// We can't follow Sonic if he's not around!
	if (!sonic || sonic->health <= 0)
		return false;

	// Check if Sonic is busy first.
	// If he's doing any of these things, he probably doesn't want to see us.
	if (sonic->player->pflags & (PF_GLIDING|PF_SLIDING|PF_BOUNCING)
	|| (sonic->player->panim != PA_IDLE && sonic->player->panim != PA_WALK)
	|| (sonic->player->powers[pw_carry]))
		return false;

	// Low ceiling, do not want!
	if (sonic->ceilingz - sonic->z < 2*sonic->height)
		return false;

	// If you're dead, wait a few seconds to respawn.
	if (player->playerstate == PST_DEAD) {
		if (player->deadtimer > 4*TICRATE)
			return true;
		return false;
	}

	// If you can't see Sonic, I guess we should?
	if (!P_CheckSight(sonic, tails) && P_AproxDistance(P_AproxDistance(tails->x-sonic->x, tails->y-sonic->y), tails->z-sonic->z) > FixedMul(1024*FRACUNIT, tails->scale))
		return true;
	return false;
}

void B_RespawnBot(INT32 playernum)
{
	player_t *player = &players[playernum];
	fixed_t x,y,z;
	mobj_t *sonic = players[consoleplayer].mo;
	mobj_t *tails;

	if (!sonic || sonic->health <= 0)
		return;

	player->bot = 1;
	P_SpawnPlayer(playernum);
	tails = player->mo;

	x = sonic->x;
	y = sonic->y;
	if (sonic->eflags & MFE_VERTICALFLIP) {
		tails->eflags |= MFE_VERTICALFLIP;
		z = sonic->z - FixedMul(512*FRACUNIT,sonic->scale);
		if (z < sonic->floorz)
			z = sonic->floorz;
	} else {
		z = sonic->z + sonic->height + FixedMul(512*FRACUNIT,sonic->scale);
		if (z > sonic->ceilingz - sonic->height)
			z = sonic->ceilingz - sonic->height;
	}

	if (sonic->flags2 & MF2_OBJECTFLIP)
		tails->flags2 |= MF2_OBJECTFLIP;
	if (sonic->flags2 & MF2_TWOD)
		tails->flags2 |= MF2_TWOD;
	if (sonic->eflags & MFE_UNDERWATER)
		tails->eflags |= MFE_UNDERWATER;
	player->powers[pw_underwater] = sonic->player->powers[pw_underwater];
	player->powers[pw_spacetime] = sonic->player->powers[pw_spacetime];
	player->powers[pw_gravityboots] = sonic->player->powers[pw_gravityboots];
	player->powers[pw_nocontrol] = sonic->player->powers[pw_nocontrol];
	player->acceleration = sonic->player->acceleration;
	player->accelstart = sonic->player->accelstart;
	player->thrustfactor = sonic->player->thrustfactor;
	player->normalspeed = sonic->player->normalspeed;
	player->pflags |= PF_AUTOBRAKE|(sonic->player->pflags & PF_DIRECTIONCHAR);

	P_TeleportMove(tails, x, y, z);
	if (player->charability == CA_FLY)
	{
		P_SetPlayerMobjState(tails, S_PLAY_FLY);
		tails->player->powers[pw_tailsfly] = (UINT16)-1;
	}
	else
		P_SetPlayerMobjState(tails, S_PLAY_FALL);
	P_SetScale(tails, sonic->scale);
	tails->destscale = sonic->destscale;
}
