/**
 * Entity functions
 */

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "combat.h"
#include "entity.h"
#include "enums.h"
#include "input.h"
#include "intrface.h"
#include "itemdefs.h"
#include "kq.h"
#include "menu.h"
#include "random.h"
#include "setup.h"

/*  internal functions  */
namespace KqFork
{
/**
 * Chase player
 *
 * Chase after the main player #0, if he/she is near. Speed up until at maximum. If the
 * player goes out of range, wander for a bit.
 *
 * @param target_entity Index of entity
 */
void chase(t_entity target_entity);

/**
 * Check proximity
 *
 * Check to see if the target is within "rad" squares.
 * Test area is a square box rather than a circle
 * target entity needs to be within the view area
 * to be visible
 * (PH) this implementation is really odd :?
 *
 * @param eno Entity under consideration
 * @param tgt Entity to test
 * @param rad Radius to test within
 * @return 1 if near, 0 otherwise
 */
int entity_near(t_entity eno, t_entity tgt, int rad);

/**
 * Run script
 *
 * This executes script commands.  This is from Verge1.
 *
 * @param target_entity Entity to process
 */
void entscript(t_entity target_entity);

/**
 * Party following leader
 *
 * This makes any characters (after the first) follow the leader.
 */
void follow(int tile_x, int tile_y);

/**
 * Read a command and parameter from a script
 *
 * This processes entity commands from the movement script.
 * This is from Verge1.
 *
 * Script commands are:
 * - U,R,D,L + param:  move up, right, down, left by param spaces
 * - W+param: wait param frames
 * - B: start script again
 * - X+param: move to x-coord param
 * - Y+param: move to y-coord param
 * - F+param: face direction param (0=S, 1=N, 2=W, 3=E)
 * - K: kill (remove) entity
 *
 * @param target_entity Entity to process
 */
void getcommand(t_entity target_entity);

/**
 * Generic movement
 *
 * Set up the entity vars to move in the given direction
 *
 * @param target_entity Index of entity to move
 * @param dx tiles to move in x direction
 * @param dy tiles to move in y direction
 */
int move(t_entity target_entity, int dx, int dy);

/**
 * Check for obstruction
 *
 * Check for any map-based obstructions in the specified co-ordinates.
 *
 * @param origin_x Original x-coord position
 * @param origin_y Original y-coord position
 * @param move_x Amount to move -1..+1
 * @param move_y Amount to move -1..+1
 * @param check_entity Whether to return 1 if an entity is at the target
 * @return 1 if path is obstructed, 0 otherwise
 */
int obstruction(int origin_x, int origin_y, int move_x, int move_y, int check_entity);

/**
 * Read an int from a script
 *
 * This parses the movement script for a value that relates
 * to a command.  This is from Verge1.
 *
 * @param target_entity Entity to process
 */
void parsems(t_entity target_entity);

/**
 * Process movement for player
 *
 * This is the replacement for process_controls that used to be in kq.c
 * I realized that all the work in process_controls was already being
 * done in process_entity... I just had to make this exception for the
 * player-controlled dude.
 */
void player_move(void);

/**
 * Actions for one entity
 *
 * Process an individual active entity. If the entity in question is #0 (main character)
 * and the party is not automated, then allow for player input.
 *
 * @param target_entity Index of entity
 */
void process_entity(t_entity target_entity);

/**
 * Adjust movement speed
 *
 * This has to adjust for each entity's speed.
 * 'Normal' speed appears to be 4.
 *
 * @param target_entity Index of entity
 */
void speed_adjust(t_entity target_entity);

/**
 * Move entity towards target
 *
 * When entity is in target mode (MM_TARGET) move towards the goal.  This is
 * fairly simple; it doesn't do clever obstacle avoidance.  It simply moves
 * either horizontally or vertically, preferring the _closer_ one. In other
 * words, it will try to get on a vertical or horizontal line with its target.
 *
 * @param target_entity Index of entity
 */
void target(t_entity target_entity);

/**
 * Move randomly
 *
 * Choose a random direction for the entity to walk in and set up the vars to do so.
 *
 * @param target_entity Index of entity to move
 */
void wander(t_entity target_entity);
}

void KqFork::chase(t_entity target_entity)
{
	int emoved = 0;

	if (g_ent[target_entity].chasing == 0)
	{
		if (KqFork::entity_near(target_entity, 0, 3) == 1 && kqrandom->random_range_exclusive(0, 100) <= g_ent[target_entity].extra)
		{
			g_ent[target_entity].chasing = 1;
			if (g_ent[target_entity].speed < 7)
			{
				g_ent[target_entity].speed++;
			}
			g_ent[target_entity].delay = 0;
		}
		else
		{
			KqFork::wander(target_entity);
		}
	}
	if (g_ent[target_entity].chasing == 1)
	{
		if (KqFork::entity_near(target_entity, 0, 4) == 1)
		{
			if (g_ent[0].tilex > g_ent[target_entity].tilex)
			{
				emoved = KqFork::move(target_entity, 1, 0);
			}
			if (g_ent[0].tilex < g_ent[target_entity].tilex && !emoved)
			{
				emoved = KqFork::move(target_entity, -1, 0);
			}
			if (g_ent[0].tiley > g_ent[target_entity].tiley && !emoved)
			{
				emoved = KqFork::move(target_entity, 0, 1);
			}
			if (g_ent[0].tiley < g_ent[target_entity].tiley && !emoved)
			{
				emoved = KqFork::move(target_entity, 0, -1);
			}
			if (!emoved)
			{
				KqFork::wander(target_entity);
			}
		}
		else
		{
			g_ent[target_entity].chasing = 0;
			if (g_ent[target_entity].speed > 1)
			{
				g_ent[target_entity].speed--;
			}
			g_ent[target_entity].delay = kqrandom->random_range_exclusive(25, 50);
			KqFork::wander(target_entity);
		}
	}
}

/**
 * Count active entities
 *
 * Force calculation of the 'noe' variable.
 * This actually calculates the last index of any active entity plus one,
 * so if there are entities present, but not active, they may be counted.
 */
void count_entities(void)
{
	size_t entity_index;

	noe = 0;
	for (entity_index = 0; entity_index < MAX_ENTITIES; entity_index++)
	{
		if (g_ent[entity_index].active == 1)
		{
			noe = entity_index + 1;
		}
	}
}

int KqFork::entity_near(t_entity eno, t_entity tgt, int rad)
{
	int ax, ay, ex, ey, b;

	b = 0 - rad;
	ex = g_ent[eno].tilex;
	ey = g_ent[eno].tiley;
	for (ay = b; ay <= rad; ay++)
	{
		for (ax = b; ax <= rad; ax++)
		{
			if (ex + ax >= view_x1 && ax + ax <= view_x2 && ey + ay >= view_y1 && ey + ay <= view_y2)
			{
				if (ex + ax == g_ent[tgt].tilex && ey + ay == g_ent[tgt].tiley)
				{
					return 1;
				}
			}
		}
	}
	return 0;
}

int entityat(int ox, int oy, t_entity who)
{
	for (t_entity i = 0; i < MAX_ENTITIES; i++)
	{
		if (g_ent[i].active && ox == g_ent[i].tilex && oy == g_ent[i].tiley)
		{
			if (who >= PSIZE)
			{
				if (g_ent[who].eid == ID_ENEMY && i < PSIZE)
				{
					if (combat(0) == 1)
					{
						g_ent[who].active = 0;
					}
					return 0;
				}
				return i + 1;
			}
			else
			{
				if (g_ent[i].eid == ID_ENEMY)
				{
					if (combat(0) == 1)
					{
						g_ent[i].active = 0;
					}
					return 0;
				}
				if (i >= PSIZE)
				{
					return i + 1;
				}
			}
		}
	}
	return 0;
}

void KqFork::entscript(t_entity target_entity)
{
	if (g_ent[target_entity].active == 0)
	{
		return;
	}
	if (g_ent[target_entity].cmd == 0)
	{
		KqFork::getcommand(target_entity);
	}
	switch (g_ent[target_entity].cmd)
	{
	case COMMAND_MOVE_UP:
		if (KqFork::move(target_entity, 0, -1))
		{
			g_ent[target_entity].cmdnum--;
		}
		break;
	case COMMAND_MOVE_DOWN:
		if (KqFork::move(target_entity, 0, 1))
		{
			g_ent[target_entity].cmdnum--;
		}
		break;
	case COMMAND_MOVE_LEFT:
		if (KqFork::move(target_entity, -1, 0))
		{
			g_ent[target_entity].cmdnum--;
		}
		break;
	case COMMAND_MOVE_RIGHT:
		if (KqFork::move(target_entity, 1, 0))
		{
			g_ent[target_entity].cmdnum--;
		}
		break;
	case COMMAND_WAIT:
		g_ent[target_entity].cmdnum--;
		break;
	case COMMAND_FINISH_COMMANDS:
		return;
	case COMMAND_REPEAT:
		g_ent[target_entity].sidx = 0;
		g_ent[target_entity].cmdnum = 0;
		break;
	case COMMAND_MOVETO_X:
		if (g_ent[target_entity].tilex < g_ent[target_entity].cmdnum)
		{
			KqFork::move(target_entity, 1, 0);
		}
		if (g_ent[target_entity].tilex > g_ent[target_entity].cmdnum)
		{
			KqFork::move(target_entity, -1, 0);
		}
		if (g_ent[target_entity].tilex == g_ent[target_entity].cmdnum)
		{
			g_ent[target_entity].cmdnum = 0;
		}
		break;
	case COMMAND_MOVETO_Y:
		if (g_ent[target_entity].tiley < g_ent[target_entity].cmdnum)
		{
			KqFork::move(target_entity, 0, 1);
		}
		if (g_ent[target_entity].tiley > g_ent[target_entity].cmdnum)
		{
			KqFork::move(target_entity, 0, -1);
		}
		if (g_ent[target_entity].tiley == g_ent[target_entity].cmdnum)
		{
			g_ent[target_entity].cmdnum = 0;
		}
		break;
	case COMMAND_FACE:
		g_ent[target_entity].facing = g_ent[target_entity].cmdnum;
		g_ent[target_entity].cmdnum = 0;
		break;
	}
	if (g_ent[target_entity].cmdnum == 0)
	{
		g_ent[target_entity].cmd = 0;
	}
}

void KqFork::follow(int tile_x, int tile_y)
{
	t_entity i;

	if (numchrs == 1)
	{
		return;
	}
	for (i = numchrs - 1; i > 0; --i)
	{
		if (i == 1)
		{
			KqFork::move(i, tile_x - g_ent[i].tilex, tile_y - g_ent[i].tiley);
		}
		else
		{
			KqFork::move(i, g_ent[i - 1].tilex - g_ent[i].tilex, g_ent[i - 1].tiley - g_ent[i].tiley);
		}
	}
}

void KqFork::getcommand(t_entity target_entity)
{
	char s;

	/* PH FIXME: prevented from running off end of string */
	if (g_ent[target_entity].sidx < sizeof(g_ent[target_entity].script))
	{
		s = g_ent[target_entity].script[g_ent[target_entity].sidx++];
	}
	else
	{
		s = '\0';
	}
	switch (s)
	{
	case 'u':
	case 'U':
		g_ent[target_entity].cmd = COMMAND_MOVE_UP;
		KqFork::parsems(target_entity);
		break;
	case 'd':
	case 'D':
		g_ent[target_entity].cmd = COMMAND_MOVE_DOWN;
		KqFork::parsems(target_entity);
		break;
	case 'l':
	case 'L':
		g_ent[target_entity].cmd = COMMAND_MOVE_LEFT;
		KqFork::parsems(target_entity);
		break;
	case 'r':
	case 'R':
		g_ent[target_entity].cmd = COMMAND_MOVE_RIGHT;
		KqFork::parsems(target_entity);
		break;
	case 'w':
	case 'W':
		g_ent[target_entity].cmd = COMMAND_WAIT;
		KqFork::parsems(target_entity);
		break;
	case '\0':
		g_ent[target_entity].cmd = COMMAND_FINISH_COMMANDS;
		g_ent[target_entity].movemode = MM_STAND;
		g_ent[target_entity].cmdnum = 0;
		g_ent[target_entity].sidx = 0;
		break;
	case 'b':
	case 'B':
		g_ent[target_entity].cmd = COMMAND_REPEAT;
		break;
	case 'x':
	case 'X':
		g_ent[target_entity].cmd = COMMAND_MOVETO_X;
		KqFork::parsems(target_entity);
		break;
	case 'y':
	case 'Y':
		g_ent[target_entity].cmd = COMMAND_MOVETO_Y;
		KqFork::parsems(target_entity);
		break;
	case 'f':
	case 'F':
		g_ent[target_entity].cmd = COMMAND_FACE;
		KqFork::parsems(target_entity);
		break;
	case 'k':
	case 'K':
		/* PH add: command K makes the ent disappear */
		g_ent[target_entity].cmd = COMMAND_KILL;
		g_ent[target_entity].active = 0;
		break;
	default:
#ifdef DEBUGMODE
		if (debugging > 0)
		{
			sprintf(strbuf, _("Invalid entity command (%c) at position %d for ent %d"), s, g_ent[target_entity].sidx, target_entity);
			Game.program_death(strbuf);
		}
#endif
		break;
	}
}

int KqFork::move(t_entity target_entity, int dx, int dy)
{
	int tile_x, tile_y, source_tile, oldfacing;
	KQEntity* ent = &g_ent[target_entity];

	if (dx == 0 && dy == 0) // Speed optimization.
	{
		return 0;
	}

	tile_x = ent->x / TILE_W;
	tile_y = ent->y / TILE_H;
	oldfacing = ent->facing;
	if (dx < 0)
	{
		ent->facing = FACE_LEFT;
	}
	else if (dx > 0)
	{
		ent->facing = FACE_RIGHT;
	}
	else if (dy > 0)
	{
		ent->facing = FACE_DOWN;
	}
	else if (dy < 0)
	{
		ent->facing = FACE_UP;
	}
	if (tile_x + dx < 0 || tile_x + dx >= (int)g_map.xsize ||
		tile_y + dy < 0 || tile_y + dy >= (int)g_map.ysize)
	{
		return 0;
	}
	if (ent->obsmode == 1)
	{
		// Try to automatically walk/run around obstacle.
		if (dx && KqFork::obstruction(tile_x, tile_y, dx, 0, FALSE))
		{
			if (dy != -1 && oldfacing == ent->facing &&
				!KqFork::obstruction(tile_x, tile_y + 1, dx, 0, TRUE) &&
				!KqFork::obstruction(tile_x, tile_y, 0, 1, TRUE))
			{
				dy = 1;
			}
			else if (dy != 1 && oldfacing == ent->facing &&
				!KqFork::obstruction(tile_x, tile_y - 1, dx, 0, TRUE) &&
				!KqFork::obstruction(tile_x, tile_y, 0, -1, TRUE))
			{
				dy = -1;
			}
			else
			{
				dx = 0;
			}
		}
		if (dy && KqFork::obstruction(tile_x, tile_y, 0, dy, FALSE))
		{
			if (dx != -1 && oldfacing == ent->facing &&
				!KqFork::obstruction(tile_x + 1, tile_y, 0, dy, TRUE) &&
				!KqFork::obstruction(tile_x, tile_y, 1, 0, TRUE))
			{
				dx = 1;
			}
			else if (dx != 1 && oldfacing == ent->facing &&
				!KqFork::obstruction(tile_x - 1, tile_y, 0, dy, TRUE) &&
				!KqFork::obstruction(tile_x, tile_y, -1, 0, TRUE))
			{
				dx = -1;
			}
			else
			{
				dy = 0;
			}
		}
		if ((dx || dy) && KqFork::obstruction(tile_x, tile_y, dx, dy, FALSE))
		{
			dx = dy = 0;
		}
	}

	if (!dx && !dy && oldfacing == ent->facing)
	{
		return 0;
	}

	if (ent->obsmode == 1 && entityat(tile_x + dx, tile_y + dy, target_entity))
	{
		return 0;
	}

	// Make sure that the player can't avoid special zones by moving diagonally.
	if (dx && dy)
	{
		source_tile = tile_y * g_map.xsize + tile_x;
		if (z_seg[source_tile] != z_seg[source_tile + dx] ||
			z_seg[source_tile] != z_seg[source_tile + dy * g_map.xsize])
		{
			if (ent->facing == FACE_LEFT || ent->facing == FACE_RIGHT)
			{
				if (!KqFork::obstruction(tile_x, tile_y, dx, 0, TRUE))
				{
					dy = 0;
				}
				else
				{
					dx = 0;
				}
			}
			else   // They are facing up or down.
			{
				if (!KqFork::obstruction(tile_x, tile_y, 0, dy, TRUE))
				{
					dx = 0;
				}
				else
				{
					dy = 0;
				}
			}
		}
	}

	// Make sure player can't walk diagonally between active entities.
	if (dx && dy)
	{
		if (KqFork::obstruction(tile_x, tile_y, dx, 0, TRUE) &&
			KqFork::obstruction(tile_x, tile_y, 0, dy, TRUE))
		{
			return 0;
		}
	}

	ent->tilex = tile_x + dx;
	ent->tiley = tile_y + dy;
	ent->y += dy;
	ent->x += dx;
	ent->moving = 1;
	ent->movcnt = 15;
	return 1;
}

int KqFork::obstruction(int origin_x, int origin_y, int move_x, int move_y, int check_entity)
{
	int current_tile; // obstrution for current tile
	int target_tile;  // obstruction for destination tile
	int dest_x;       // destination tile, x-coord
	int dest_y;       // destination tile, y-coord
	t_entity i;

	// Block entity if it tries to walk off the map
	if ((origin_x == 0 && move_x < 0) ||
		(origin_y == 0 && move_y < 0) ||
		(origin_x == (int)g_map.xsize - 1 && move_x > 0) ||
		(origin_y == (int)g_map.ysize - 1 && move_y > 0))
	{
		return 1;
	}

	dest_x = origin_x + move_x;
	dest_y = origin_y + move_y;

	// Check the current and target tiles' obstacles
	current_tile = o_seg[(origin_y * g_map.xsize) + origin_x];
	target_tile = o_seg[(dest_y * g_map.xsize) + dest_x];

	// Return early if the destination tile is an obstruction
	if (target_tile == BLOCK_ALL)
	{
		return 1;
	}

	// Check whether the current OR target tiles block movement
	if (move_y == -1)
	{
		if (current_tile == BLOCK_U || target_tile == BLOCK_D)
		{
			return 1;
		}
	}
	if (move_y == 1)
	{
		if (current_tile == BLOCK_D || target_tile == BLOCK_U)
		{
			return 1;
		}
	}
	if (move_x == -1)
	{
		if (current_tile == BLOCK_L || target_tile == BLOCK_R)
		{
			return 1;
		}
	}
	if (move_x == 1)
	{
		if (current_tile == BLOCK_R || target_tile == BLOCK_L)
		{
			return 1;
		}
	}

	// Another entity blocks movement as well
	if (check_entity)
	{
		for (i = 0; i < MAX_ENTITIES; i++)
		{
			if (g_ent[i].active && dest_x == g_ent[i].tilex && dest_y == g_ent[i].tiley)
			{
				return 1;
			}
		}
	}

	// No obstacles found
	return 0;
}

void KqFork::parsems(t_entity target_entity)
{
	uint32_t p = 0;
	char tok[10];
	char s;

	s = g_ent[target_entity].script[g_ent[target_entity].sidx];

	// 48..57 are '0'..'9' ASCII
	while (s >= '0' && s <= '9')
	{
		tok[p] = s;
		p++;

		g_ent[target_entity].sidx++;
		s = g_ent[target_entity].script[g_ent[target_entity].sidx];
	}
	tok[p] = 0;
	g_ent[target_entity].cmdnum = atoi(tok);
}

/**
 * Set position
 *
 * Position an entity manually.
 *
 * @param en Entity to position
 * @param ex x-coord
 * @param ey y-coord
 */
void place_ent(t_entity en, int ex, int ey)
{
	g_ent[en].tilex = ex;
	g_ent[en].tiley = ey;
	g_ent[en].x = g_ent[en].tilex * TILE_W;
	g_ent[en].y = g_ent[en].tiley * TILE_H;
}

void KqFork::player_move(void)
{
	int oldx = g_ent[0].tilex;
	int oldy = g_ent[0].tiley;

	PlayerInput.readcontrols();

	if (PlayerInput.balt)
	{
		Game.activate();
	}
	if (PlayerInput.benter)
	{
		menu();
	}
#ifdef KQ_CHEATS
	if (PlayerInput.bcheat)
	{
		do_luacheat();
	}
#endif

	KqFork::move(0, PlayerInput.right ? 1 : PlayerInput.left ? -1 : 0, PlayerInput.down ? 1 : PlayerInput.up ? -1 : 0);
	if (g_ent[0].moving)
	{
		KqFork::follow(oldx, oldy);
	}
}

/**
 * Main entity routine
 *
 * The main routine that loops through the entity list and processes each
 * one.
 */
void process_entities(void)
{
	for (t_entity i = 0; i < MAX_ENTITIES; i++)
	{
		if (g_ent[i].active == 1)
		{
			KqFork::speed_adjust(i);
		}
	}

	/* Do timers */
	char* t_evt = Game.get_timer_event();
	if (t_evt)
	{
		do_timefunc(t_evt);
	}
}

void KqFork::process_entity(t_entity target_entity)
{
	KQEntity* ent = &g_ent[target_entity];
	s_player* player = 0;

	ent->scount = 0;

	if (!ent->active)
	{
		return;
	}

	if (!ent->moving)
	{
		if (target_entity == 0 && !autoparty)
		{
			KqFork::player_move();
			if (ent->moving && display_desc == 1)
			{
				display_desc = 0;
			}
			return;
		}
		switch (ent->movemode)
		{
		case MM_STAND:
			return;
		case MM_WANDER:
			KqFork::wander(target_entity);
			break;
		case MM_SCRIPT:
			KqFork::entscript(target_entity);
			break;
		case MM_CHASE:
			KqFork::chase(target_entity);
			break;
		case MM_TARGET:
			KqFork::target(target_entity);
			break;
		}
	}
	else   /* if (.moving==0) */
	{
		if (ent->tilex * TILE_W > ent->x)
		{
			++ent->x;
		}
		if (ent->tilex * TILE_W < ent->x)
		{
			--ent->x;
		}
		if (ent->tiley * TILE_H > ent->y)
		{
			++ent->y;
		}
		if (ent->tiley * TILE_H < ent->y)
		{
			--ent->y;
		}
		ent->movcnt--;

		if (ent->framectr < 20)
		{
			ent->framectr++;
		}
		else
		{
			ent->framectr = 0;
		}

		if (ent->movcnt == 0)
		{
			ent->moving = 0;
			if (target_entity < PSIZE)
			{
				player = &party[pidx[target_entity]];
				if (steps < STEPS_NEEDED)
				{
					steps++;
				}
				if (player->sts[S_POISON] > 0)
				{
					if (player->hp > 1)
					{
						player->hp--;
					}
					play_effect(21, 128);
				}
				if (player->eqp[EQP_SPECIAL] == I_REGENERATOR)
				{
					if (player->hp < player->mhp)
					{
						player->hp++;
					}
				}
			}
			if (target_entity == 0)
			{
				Game.zone_check();
			}
		}

		if (target_entity == 0 && vfollow == 1)
		{
			Game.calc_viewport(0);
		}
	}
}

/**
 * Initialize script
 *
 * This is used to set up an entity with a movement script so that
 * it can be automatically controlled.
 *
 * @param target_entity Entity to process
 * @param movestring The script
 */
void set_script(t_entity target_entity, const char* movestring)
{
	g_ent[target_entity].moving = 0; // Stop entity from moving
	g_ent[target_entity].movcnt = 0; // Reset the move counter to 0
	g_ent[target_entity].cmd = COMMAND_NONE;
	g_ent[target_entity].sidx = 0;   // Reset script command index
	g_ent[target_entity].cmdnum = 0; // There are no scripted commands
	g_ent[target_entity].movemode = MM_SCRIPT; // Force the entity to follow the script
	strncpy(g_ent[target_entity].script, movestring, sizeof(g_ent[target_entity].script));
}

void KqFork::speed_adjust(t_entity target_entity)
{
	if (g_ent[target_entity].speed < 4)
	{
		switch (g_ent[target_entity].speed)
		{
		case 1:
			if (g_ent[target_entity].scount < 3)
			{
				g_ent[target_entity].scount++;
				return;
			}
			break;
		case 2:
			if (g_ent[target_entity].scount < 2)
			{
				g_ent[target_entity].scount++;
				return;
			}
			break;
		case 3:
			if (g_ent[target_entity].scount < 1)
			{
				g_ent[target_entity].scount++;
				return;
			}
			break;
		}
	}
	if (g_ent[target_entity].speed < 5)
	{
		KqFork::process_entity(target_entity);
	}
	switch (g_ent[target_entity].speed)
	{
	case 5:
		KqFork::process_entity(target_entity);
		KqFork::process_entity(target_entity);
		break;
	case 6:
		KqFork::process_entity(target_entity);
		KqFork::process_entity(target_entity);
		KqFork::process_entity(target_entity);
		break;
	case 7:
		KqFork::process_entity(target_entity);
		KqFork::process_entity(target_entity);
		KqFork::process_entity(target_entity);
		KqFork::process_entity(target_entity);
		break;
	}
	/* TT: This is to see if the player is "running" */
	if (key[PlayerInput.kctrl] && target_entity < PSIZE)
	{
		KqFork::process_entity(target_entity);
	}
}

void KqFork::target(t_entity target_entity)
{
	int dx, dy, ax, ay, emoved = 0;
	KQEntity* ent = &g_ent[target_entity];

	ax = dx = ent->target_x - ent->tilex;
	ay = dy = ent->target_y - ent->tiley;
	if (ax < 0)
	{
		ax = -ax;
	}
	if (ay < 0)
	{
		ay = -ay;
	}
	if (ax < ay)
	{
		/* Try to move horizontally */
		if (dx < 0)
		{
			emoved = KqFork::move(target_entity, -1, 0);
		}
		if (dx > 0)
		{
			emoved = KqFork::move(target_entity, 1, 0);
		}
		/* Didn't move so try vertically */
		if (!emoved)
		{
			if (dy < 0)
			{
				KqFork::move(target_entity, 0, -1);
			}
			if (dy > 0)
			{
				KqFork::move(target_entity, 0, 1);
			}
		}
	}
	else
	{
		/* Try to move vertically */
		if (dy < 0)
		{
			emoved = KqFork::move(target_entity, 0, -1);
		}
		if (dy > 0)
		{
			emoved = KqFork::move(target_entity, 0, 1);
		}
		/* Didn't move so try horizontally */
		if (!emoved)
		{
			if (dx < 0)
			{
				KqFork::move(target_entity, -1, 0);
			}
			if (dx > 0)
			{
				KqFork::move(target_entity, 1, 0);
			}
		}
	}
	if (dx == 0 && dy == 0)
	{
		/* Got there */
		ent->movemode = MM_STAND;
	}
}

void KqFork::wander(t_entity target_entity)
{
	if (g_ent[target_entity].delayctr < g_ent[target_entity].delay)
	{
		g_ent[target_entity].delayctr++;
		return;
	}
	g_ent[target_entity].delayctr = 0;
	switch (kqrandom->random_range_exclusive(0, 8))
	{
	case 0:
		KqFork::move(target_entity, 0, -1);
		break;
	case 1:
		KqFork::move(target_entity, 0, 1);
		break;
	case 2:
		KqFork::move(target_entity, -1, 0);
		break;
	case 3:
		KqFork::move(target_entity, 1, 0);
		break;
	}
}
