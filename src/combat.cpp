/*! License
   KQ is Copyright (C) 2002 by Josh Bolduc

   This file is part of KQ... a freeware RPG.

   KQ is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   KQ is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with KQ; see the file COPYING.  If not, write to
   the Free Software Foundation,
       675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*! \file
 * \brief Combat mode
 *
 * This is the main file where combat is initiated.
 * \author JB
 * \date ????????
 */

#include <memory>
#include <stdio.h>
#include <string.h>

#include "combat.h"
#include "constants.h"
#include "draw.h"
#include "effects.h"
#include "enemyc.h"
#include "enums.h"
#include "fade.h"
#include "fighter.h"
#include "gfx.h"
#include "heroc.h"
#include "imgcache.h"
#include "input.h"
#include "itemmenu.h"
#include "kq.h"
#include "magic.h"
#include "masmenu.h"
#include "menu.h"
#include "music.h"
#include "platform.h"
#include "player.h"
#include "random.h"
#include "res.h"
#include "setup.h"
#include "structs.h"
#include "timing.h"

/*! \name global variables  */

uint32_t combatend;
int cact[NUM_FIGHTERS];
int curx;
int cury;
uint32_t num_enemies;
int ta[NUM_FIGHTERS];
int deffect[NUM_FIGHTERS];
int RemainingBattleCounter;
uint8_t vspell;
uint8_t ms;
Raster *backart;

/* Internal variables */
static int curw;
static int nspeed[NUM_FIGHTERS];
static int bspeed[NUM_FIGHTERS];
static uint8_t hs;

enum eAttackResult { ATTACK_MISS, ATTACK_SUCCESS, ATTACK_CRITICAL };

/* Internal prototypes */
static eAttackResult attack_result(int, int);
static int check_end(void);
static void do_action(size_t);
static int do_combat(char *, char *, int);
static void do_round(void);
static void enemies_win(void);
static void heroes_win(void);
static void init_fighters(void);
static void roll_initiative(void);
static void snap_togrid(void);

/*! \brief Attack all enemies at once
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * This does the actual attack calculation. The damage done to
 * the target is kept in the ta[] array.
 *
 * \param   ar Attacker
 * \param   dr Defender
 * \returns ATTACK_MISS if attack was a miss,
 *          ATTACK_SUCCESS if attack was successful,
 *          ATTACK_CRITICAL if attack was a critical hit.
 */
eAttackResult attack_result(int ar, int dr)
{
	int c;
	int check_for_critical_hit;
	int attacker_critical_status = 0;
	int crit_hit = 0;
	int base;
	int to_hit;
	int mult;
	int dmg; /* extra */
	int attacker_attack;
	int attacker_hit;
	size_t attacker_weapon_element;
	int defender_defense;
	int defender_evade;

	attacker_attack = tempa.stats[A_ATT];
	attacker_hit = tempa.stats[A_HIT];
	attacker_weapon_element = tempa.welem;
	defender_defense = tempd.stats[A_DEF];
	defender_evade = tempd.stats[A_EVD];

	/*  JB: check to see if the attacker is in critical status...  */
	/*      increases chance for a critical hit                    */
	if (tempa.mhp > 250)
	{
		if (tempa.hp <= 50)
		{
			attacker_critical_status = 1;
		}
	}
	else
	{
		if (tempa.hp <= (tempa.mhp / 5))
		{
			attacker_critical_status = 1;
		}
	}

	/*  JB: check to see if the defender is 'defending'  */
	if (tempd.defend == 1)
	{
		defender_defense = (defender_defense * 3) / 2;
	}

	/*  JB: if the attacker is empowered by trueshot  */
	if (tempa.IsTrueshot())
	{
		fighter[ar].SetTrueshot(false);
		defender_evade = 0;
	}

	attacker_attack += (tempa.stats[tempa.bstat] * tempa.bonus / 100);
	if (attacker_attack < DMG_RND_MIN * 5)
	{
		base = kqrandom->random_range_exclusive(0, DMG_RND_MIN);
	}
	else
	{
		base = kqrandom->random_range_exclusive(0, attacker_attack / 5);
	}

	base += attacker_attack - defender_defense;
	if (base < 1)
	{
		base = 1;
	}

	mult = 0;
	to_hit = attacker_hit + defender_evade;
	if (to_hit < 1)
	{
		to_hit = 1;
	}

	if (kqrandom->random_range_exclusive(0, to_hit) < attacker_hit)
	{
		mult++;
	}

	/*  JB: If the defender is etherealized, set mult to 0  */
	if (tempd.IsEther())
	{
		mult = 0;
	}

	if (mult > 0)
	{
		if (tempd.crit == 1)
		{
			check_for_critical_hit = 1;
			if (attacker_critical_status == 1)
			{
				check_for_critical_hit = 2;
			}

			/* PH I _think_ this makes Sensar 2* as likely to make a critical hit */
			if (pidx[ar] == SENSAR)
			{
				check_for_critical_hit = check_for_critical_hit * 2;
			}

			check_for_critical_hit = (20 - check_for_critical_hit);
			if (kqrandom->random_range_exclusive(0, 20) >= check_for_critical_hit)
			{
				crit_hit = 1;
				base = ((int)base * 3) / 2;
			}
		}

		/*  JB: if affected by a NAUSEA/MALISON spell, the defender  */
		/*      takes more damage than normal                        */
		if (tempd.IsMalison())
		{
			base *= (int)5 / 4;
		}

		/*  JB: check for elemental/status weapons  */
		if (base < 1)
		{
			base = 1;
		}

		c = attacker_weapon_element - 1;
		if ((c >= R_EARTH) && (c <= R_ICE))
		{
			base = Magic.res_adjust(dr, c, base);
		}

		if ((c >= R_POISON) && (c <= R_SLEEP))
		{
			bool isAffected = false;
			switch (c)
			{
			case R_POISON:
				isAffected = !fighter[dr].IsPoisoned();
				break;
			case R_BLIND:
				isAffected = !fighter[dr].IsBlind();
				break;
			case R_CHARM:
				isAffected = !fighter[dr].IsCharmed();
				break;
			case R_PARALYZE:
				isAffected = !fighter[dr].IsStopped();
				break;
			case R_PETRIFY:
				isAffected = !fighter[dr].IsStone();
				break;
			case R_SILENCE:
				isAffected = !fighter[dr].IsMute();
				break;
			case R_SLEEP:
				isAffected = !fighter[dr].IsAsleep();
				break;
			default:
				isAffected = false;
			}

			if ((Magic.res_throw(dr, c) == 0) && isAffected)
			{
				if (Magic.non_dmg_save(dr, 50) == 0)
				{
					uint8_t timeEffectShouldLast = kqrandom->random_range_exclusive(2, 5);
					if (c == R_POISON || c == R_PETRIFY || c == R_SILENCE)
					{
						timeEffectShouldLast = 1;
					}

					switch (c)
					{
					case R_POISON:
						fighter[dr].SetPoisoned(timeEffectShouldLast);
						break;
					case R_BLIND:
						fighter[dr].SetBlind(timeEffectShouldLast != 0);
						break;
					case R_CHARM:
						fighter[dr].SetCharmed(timeEffectShouldLast);
						break;
					case R_PARALYZE:
						fighter[dr].SetStopped(timeEffectShouldLast);
						break;
					case R_PETRIFY:
						fighter[dr].SetStone(timeEffectShouldLast);
						break;
					case R_SILENCE:
						fighter[dr].SetMute(timeEffectShouldLast != 0);
						break;
					case R_SLEEP:
						fighter[dr].SetSleep(timeEffectShouldLast);
						break;
					}
				}
			}
		}
	}

	/*  JB: Apply the damage multiplier  */
	/*  RB FIXME: check if the changes I made here didn't break something  */
	/* TT TODO:
	 * If magic, attacks, etc. are zero, they should return as a miss.
	 * For some reason, this isn't properly being reported.
	 */

#ifdef KQ_CHEATS
	if (cheat && every_hit_999)
	{
		ta[dr] = -999;
		return ATTACK_SUCCESS;
	}
#endif

	dmg = mult * base;
	if (dmg == 0)
	{
		dmg = MISS;
		ta[dr] = dmg;
		return ATTACK_MISS;
	}

	ta[dr] = 0 - dmg;
	return crit_hit == 1 ? ATTACK_CRITICAL : ATTACK_SUCCESS;
}

/*! \brief Draw the battle screen
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated 20020914 - 16:16 (RB)
 *
 * Draw the battle screen.
 *
 * \param   plyr Player: -1 means "no one is selected" (roll_initiative()), else
 * index of fighter
 * \param   hl Highlighted
 * \param   SelectAll Select all
 */
void battle_render(signed int plyr, size_t hl, int SelectAll)
{
	int a = 0;
	int b = 0;
	int sz;
	int t;
	size_t z;
	size_t current_fighter_index = 0;
	size_t fighter_index = 0;

	if (plyr > 0)
	{
		current_fighter_index = plyr - 1;
		curw = fighter[current_fighter_index].cw;
		curx = fighter[current_fighter_index].cx;
		cury = fighter[current_fighter_index].cy;
	}
	else
	{
		curx = -1;
		cury = -1;
	}

	clear_bitmap(double_buffer);
	blit(backart, double_buffer, 0, 0, 0, 0, KQ_SCREEN_W, KQ_SCREEN_H);

	if ((SelectAll == 0) && (curx > -1) && (cury > -1))
	{
		draw_sprite(double_buffer, bptr, curx + (curw / 2) - 8, cury - 8);
		if (current_fighter_index >= PSIZE)
		{
			current_fighter_index = plyr - 1;
			t = curx + (curw / 2);
			t -= (fighter[current_fighter_index].name.length() * 4);
			z = (fighter[current_fighter_index].cy < 32
				? fighter[current_fighter_index].cy + fighter[current_fighter_index].cl
				: fighter[current_fighter_index].cy - 32);

			Draw.menubox(double_buffer, t - 8, z, fighter[current_fighter_index].name.length(), 1, BLUE);
			Draw.print_font(double_buffer, t, z + 8, fighter[current_fighter_index].name, FNORMAL);
		}
	}

	auto x_offset = 216;
	for (z = 0; z < numchrs; z++)
	{
		b = z * x_offset;

		if (fighter[z].IsAlive())
		{
			draw_fighter(z, (SelectAll == 1));
		}
		else
		{
			fighter[z].aframe = 3;
			draw_fighter(z, 0);
		}

		Draw.menubox(double_buffer, b, 184, 11, 5, BLUE);
		if (fighter[z].IsAlive())
		{
			sz = bspeed[z] * 88 / ROUND_MAX;
			if (sz > 88)
			{
				sz = 88;
			}

			a = 116;
			if (fighter[z].GetRemainingTime() == 1)
			{
				a = 83;
			}
			else if (fighter[z].GetRemainingTime() == 2)
			{
				a = 36;
			}

			a += (sz / 11);
			hline(double_buffer, b + 8, 229, b + sz + 8, a + 1);
			hline(double_buffer, b + 8, 230, b + sz + 8, a);
			hline(double_buffer, b + 8, 231, b + sz + 8, a - 1);
		}

		Draw.print_font(double_buffer, b + 8, 192, fighter[z].name, (hl == z + 1) ? FGOLD : FNORMAL);

		sprintf(strbuf, _("HP: %3d/%3d"), fighter[z].hp, fighter[z].mhp);
		/*  RB IDEA: If the character has less than 1/5 of his/her max    */
		/*           health points, it shows the amount with red (the     */
		/*           character is in danger). I suggest setting that '5'  */
		/*           as a '#define WARNING_LEVEL 5' or something like     */
		/*           that, so we can change it easily (maybe we can let   */
		/*           the player choose when it should be turned red).     */
		/*  TT TODO: I like this idea; maybe somewhere in the Options     */
		/*           menu?  I find that when the bar flashes red/yellow   */
		/*           to warn the player, it's much more eye-pleasing than */
		/*           just a solid color (and not too hard to implement).  */

		Draw.print_font(double_buffer, b + 8, 208, strbuf, (fighter[z].hp < (fighter[z].mhp / 5)) ? FRED : FNORMAL);

		hline(double_buffer, b + 8, 216, b + 95, 21);
		sz = (fighter[z].hp > 0) ? fighter[z].hp * 88 / fighter[z].mhp : 88;

		hline(double_buffer, b + 8, 216, b + 8 + sz, 12);
		sprintf(strbuf, _("MP: %3d/%3d"), fighter[z].mp, fighter[z].mmp);

		/*  RB IDEA: Same suggestion as with health, just above.  */
		Draw.print_font(double_buffer, b + 8, 218, strbuf, (fighter[z].mp < (fighter[z].mmp / 5)) ? FRED : FNORMAL);
		hline(double_buffer, b + 8, 226, b + 95, 21);
		sz = (fighter[z].mp > 0) ? fighter[z].mp * 88 / fighter[z].mmp : 88;
		hline(double_buffer, b + 8, 226, b + 8 + sz, 12);
		Draw.draw_stsicon(double_buffer, 1, z, eSpellType::S_INFUSE, b + 8, 200);
	}

	for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies; fighter_index++)
	{
		if (fighter[fighter_index].IsAlive())
		{
			draw_fighter(fighter_index, (SelectAll == 2));
		}
	}

	if (display_attack_string == 1)
	{
		size_t ctext_length = strlen(attack_string) * 4;
		Draw.menubox(double_buffer, 152 - ctext_length, 8, strlen(attack_string), 1, BLUE);
		Draw.print_font(double_buffer, 160 - ctext_length, 16, attack_string, FNORMAL);
	}
}

/*! \brief Check if all heroes/enemies dead.
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Just check to see if all the enemies or heroes are dead.
 *
 * \returns 1 if the battle ended (either the heroes or the enemies won),
 *          0 otherwise.
 */
static int check_end(void)
{
	size_t fighter_index;
	int alive = 0;

	/*  RB: count the number of heroes alive. If there is none, the   */
	/*      enemies won the battle.                                   */
	for (fighter_index = 0; fighter_index < numchrs; fighter_index++)
	{
		if (fighter[fighter_index].IsAlive())
		{
			alive++;
		}
	}

	if (alive == 0)
	{
		enemies_win();
		return 1;
	}

	/*  RB: count the number of enemies alive. If there is none, the  */
	/*      heroes won the battle.                                    */
	alive = 0;
	for (fighter_index = 0; fighter_index < num_enemies; fighter_index++)
	{
		if (fighter[fighter_index + PSIZE].IsAlive())
		{
			alive++;
		}
	}

	if (alive == 0)
	{
		heroes_win();
		return 1;
	}

	return 0;
}

/*! \brief Main combat function
 *
 * The big one... I say that because the game is mostly combat :p
 * First, check to see if a random encounter has occured. The check is skipped
 * if it's a scripted battle.  Then call all the helper and setup functions
 * and start the combat by calling do_round.
 *
 * \param   bno Combat identifier (index into battles[])
 * \returns 0 if no combat, 1 otherwise
 */
int combat(int bno)
{
	int hero_level;
	int encounter;
	int lc;

#ifdef KQ_CHEATS
	if (cheat && no_monsters)
	{
		return 0;
	}
#endif

	/* PH: some checking! */
	if (bno < 0 || bno >= NUM_BATTLES)
	{
		sprintf(strbuf, _("Combat: battle %d does not exist."), bno);
		return 1;
		// program_death(strbuf);
	}

	/* TT: no battles during scripted/target movements */
	if (g_ent[0].movemode != MM_STAND)
	{
		return 0;
	}

	hero_level = party[pidx[0]].lvl;
	encounter = Enemy.SelectEncounter(battles[bno].etnum, battles[bno].eidx);

	/*  RB: check if we had had a random encounter  */
	if (battles[bno].enc > 1)
	{
#ifdef KQ_CHEATS
		/* skip battle if no_random_encouters cheat is set */
		if (cheat && no_random_encounters)
		{
			return 0;
		}
#endif

		/* skip battle if haven't moved enough steps since last battle,
		 * or if it's just not time for one yet */
		if ((steps < STEPS_NEEDED) || (kqrandom->random_range_exclusive(0, battles[bno].enc) > 0))
		{
			return 0;
		}

		/* Likely (not always) skip random battle if repulse is active */
		if (save_spells[P_REPULSE] > 0)
		{
			lc = (hero_level - erows[encounter].lvl) * 20;
			if (lc < 5)
			{
				lc = 5;
			}

			/* Although Repulse is active, there's still a chance of battle */
			if (kqrandom->random_range_exclusive(0, 100) < lc)
			{
				return 0;
			}
		}
	}

	if (hero_level >= erows[encounter].lvl + 5 && battles[bno].eidx == 99)
	{
		lc = (hero_level - erows[encounter].lvl) * 5;

		/* TT: This will skip battles based on a random number from hero's
		 *     level minus enemy's level.
		 */
		if (kqrandom->random_range_exclusive(0, 100) < lc)
		{
			return 0;
		}
	}

	/* Player is about to do battle. */

	steps = 0;
	init_fighters();
	return do_combat(battles[bno].backimg, battles[bno].bmusic, battles[bno].eidx == 99);
}

/*! \brief Choose an action
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Choose a fighter action.
 */
static void do_action(size_t fighter_index)
{
	size_t imb_index;
	uint8_t imbued_item;
	uint8_t spell_type_status;

	for (imb_index = 0; imb_index < 2; imb_index++)
	{
		imbued_item = fighter[fighter_index].imb[imb_index];
		if (imbued_item > 0)
		{
			Magic.cast_imbued_spell(fighter_index, imbued_item, 1, TGT_CASTER);
		}
	}

	spell_type_status = fighter[fighter_index].GetRemainingMalison();
	if (spell_type_status > 0)
	{
		if (kqrandom->random_range_exclusive(0, 100) < spell_type_status * 5)
		{
			cact[fighter_index] = 0;
		}
	}

	spell_type_status = fighter[fighter_index].GetRemainingCharm();
	if (spell_type_status > 0)
	{
		fighter[fighter_index].AddCharm(-1);
		spell_type_status = fighter[fighter_index].GetRemainingCharm();

		if (fighter_index < PSIZE)
		{
			auto_herochooseact(fighter_index);
		}
		else
		{
			Enemy.CharmAction(fighter_index);
		}
	}

	if (cact[fighter_index] != 0)
	{
		Draw.revert_cframes(fighter_index, 0);
		if (fighter_index < PSIZE)
		{
			if (spell_type_status == 0)
			{
				hero_choose_action(fighter_index);
			}
		}
		else
		{
			Enemy.ChooseAction(fighter_index);
		}
	}

	cact[fighter_index] = 0;
	if (check_end() == 1)
	{
		combatend = 1;
	}
}

/*! \brief Really do combat once fighters have been inited
 *
 * \param   bg Background image
 * \param   mus Music
 * \param   is_rnd If !=0 then this is a random combat
 * \returns 1 if battle occurred
 */
static int do_combat(char *bg, char *mus, int is_rnd)
{
	int zoom_step;

	in_combat = 1;
	backart = get_cached_image(bg);
	if (is_rnd)
	{
		if ((numchrs == 1) && (pidx[0] == AYLA))
		{
			hs = kqrandom->random_range_exclusive(1, 101);
			ms = kqrandom->random_range_exclusive(1, 4);
		}
		else
		{
			if (numchrs > 1 && (Game.in_party(AYLA) < MAXCHRS))
			{
				hs = kqrandom->random_range_exclusive(1, 21);
				ms = kqrandom->random_range_exclusive(1, 6);
			}
			else
			{
				hs = kqrandom->random_range_exclusive(1, 11);
				ms = kqrandom->random_range_exclusive(1, 11);
			}
		}
	}
	else
	{
		hs = 10;
		ms = 10;
	}

	/*  RB: do the zoom at the beginning of the combat.  */
	Music.pause_music();
	Music.set_music_volume((gmvol / 250.0) * 0.75);
	Music.play_music(mus, 0);

	/* TT TODO:
	 * Change this so when we zoom into the battle, it won't just zoom into the middle
	 * of the screen.  Instead, it's going to zoom into the location where the player
	 * is, so if he's on the side of the map somewhere...
	 */
	std::unique_ptr<Raster> temp(Draw.copy_bitmap(nullptr, double_buffer));
	for (zoom_step = 0; zoom_step < 9; zoom_step++)
	{
		Music.poll_music();
		stretch_blit(temp.get(), double_buffer,
			zoom_step * (KQ_SCREEN_W / 20) + xofs,
			zoom_step * (KQ_SCREEN_H / 20) + yofs,
			KQ_SCREEN_W - (zoom_step * (KQ_SCREEN_W / 10)),
			KQ_SCREEN_H - (zoom_step * (KQ_SCREEN_H / 10)),
			0, 0,
			KQ_SCREEN_W, KQ_SCREEN_H);
		Draw.blit2screen(xofs, yofs);
	}

	snap_togrid();
	roll_initiative();
	curx = 0;
	cury = 0;
	vspell = 0;
	combatend = 0;

	/*  RB: execute combat  */
	do_round();
	Music.set_music_volume(gmvol / 250.0);
	Music.resume_music();
	if (alldead)
	{
		Music.stop_music();
	}

	steps = 0;
	in_combat = 0;
	timer_count = 0;
	return (1);
}

/*! \brief Battle gauge, action controls
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated 20020914 - 16:16 (RB)
 *
 * This function controls the battle gauges and calls for action
 * when necessary. This is also where things like poison, sleep,
 * and what-not are checked.
 */
static void do_round(void)
{
	size_t a;
	size_t fighter_index;

	timer_count = 0;
	while (!combatend)
	{
		if (timer_count >= 10)
		{
			RemainingBattleCounter += BATTLE_INC;

			if (RemainingBattleCounter >= ROUND_MAX)
			{
				RemainingBattleCounter = 0;
			}

			for (fighter_index = 0; fighter_index < PSIZE + num_enemies; fighter_index++)
			{
				if ((fighter_index < numchrs) || (fighter_index >= PSIZE))
				{
					if (((fighter[fighter_index].GetRemainingPoison() - 1) == RemainingBattleCounter) && fighter[fighter_index].hp > 1)
					{
						a = kqrandom->random_range_exclusive(0, fighter[fighter_index].mhp / 20) + 1;

						if (a < 2)
						{
							a = 2;
						}

						if ((fighter[fighter_index].hp - a) < 1)
						{
							a = fighter[fighter_index].hp - 1;
						}

						ta[fighter_index] = a;
						display_amount(fighter_index, FONT_WHITE, 0);
						fighter[fighter_index].hp -= a;
					}

					/*  RB: the character is regenerating? when needed, get a  */
					/*      random value (never lower than 5), and increase    */
					/*      the character's health by that amount.             */
					if ((fighter[fighter_index].GetRemainingRegen() - 1) == RemainingBattleCounter)
					{
						a = kqrandom->random_range_exclusive(0, 5) + (fighter[fighter_index].mhp / 10);

						if (a < 5)
						{
							a = 5;
						}

						ta[fighter_index] = a;
						display_amount(fighter_index, FONT_YELLOW, 0);
						Magic.adjust_hp(fighter_index, a);
					}

					/*  RB: the character has ether active?  */
					cact[fighter_index] = 1;
					if ((fighter[fighter_index].IsEther()) && (RemainingBattleCounter == 0))
					{
						fighter[fighter_index].AddEther(-1);
					}

					/*  RB: the character is stopped?  */
					if (fighter[fighter_index].IsStopped())
					{
						if (pidx[fighter_index] == TEMMIN)
						{
							fighter[fighter_index].aux = 0;
						}

						if (RemainingBattleCounter == 0)
						{
							fighter[fighter_index].AddStopped(-1);
						}

						cact[fighter_index] = 0;
					}

					/*  RB: the character is sleeping?  */
					if (fighter[fighter_index].IsAsleep())
					{
						if (pidx[fighter_index] == TEMMIN)
						{
							fighter[fighter_index].aux = 0;
						}

						if (RemainingBattleCounter == 0)
						{
							fighter[fighter_index].AddSleep(-1);
						}

						cact[fighter_index] = 0;
					}

					/*  RB: the character is petrified?  */
					if (fighter[fighter_index].IsStone())
					{
						if (pidx[fighter_index] == TEMMIN)
						{
							fighter[fighter_index].aux = 0;
						}

						if (RemainingBattleCounter == 0)
						{
							fighter[fighter_index].AddStone(-1);
						}

						cact[fighter_index] = 0;
					}

					if (fighter[fighter_index].IsDead() ||
						fighter[fighter_index].mhp <= 0)
					{
						if (pidx[fighter_index] == TEMMIN)
						{
							fighter[fighter_index].aux = 0;
						}

						bspeed[fighter_index] = 0;
						cact[fighter_index] = 0;
					}

					if (cact[fighter_index] > 0)
					{
						if (!fighter[fighter_index].IsTime())
						{
							bspeed[fighter_index] += nspeed[fighter_index];
						}
						else
						{
							if (fighter[fighter_index].GetRemainingTime() == 1)
							{
								bspeed[fighter_index] += (nspeed[fighter_index] / 2 + 1);
							}
							else
							{
								bspeed[fighter_index] += (nspeed[fighter_index] * 2);
							}
						}
					}
				}
				else
				{
					cact[fighter_index] = 0;
				}
			}

			PlayerInput.readcontrols();
			battle_render(0, 0, 0);
			Draw.blit2screen(0, 0);

			for (fighter_index = 0; fighter_index < (PSIZE + num_enemies);
				fighter_index++)
			{
				if ((bspeed[fighter_index] >= ROUND_MAX) && (cact[fighter_index] > 0))
				{
					do_action(fighter_index);
					fighter[fighter_index].ctmem = 0;
					fighter[fighter_index].csmem = 0;
					cact[fighter_index] = 1;
					bspeed[fighter_index] = 0;
				}

				if (combatend)
				{
					return;
				}
			}

			timer_count = 0;
		}

		Game.kq_yield();
	}
}

/*! \brief Display one fighter on the screen
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated 20020914 - 16:37 (RB)
 * \date Updated 20031009 PH (put fr-> instead of fighter[fighter_index]. every
 * time)
 *
 * Display a single fighter on the screen. Checks for dead and
 * stone, and if the fighter is selected. Also displays 'Vision'
 * spell information.
 */
void draw_fighter(size_t fighter_index, size_t dcur)
{
	static const int AUGMENT_STRONGEST = 20;
	static const int AUGMENT_STRONG = 10;
	static const int AUGMENT_NORMAL = 0;

	int xx;
	int yy;
	int ff;
	KFighter *fr = &fighter[fighter_index];

	xx = fr->cx;
	yy = fr->cy;

	ff = (!fr->aframe) ? fr->facing : fr->aframe;

	if (fr->IsStone())
	{
		// Green, for sickness
		Draw.convert_cframes(fighter_index, 2, 12, 0);
	}

	if (fr->IsEther())
	{
		draw_trans_sprite(double_buffer, cframes[fighter_index][ff], xx, yy);
	}
	else
	{
		if (fighter_index < PSIZE)
		{
			// Your party
			Raster *shad = new Raster(cframes[fighter_index][ff]->width * 2 / 3, cframes[fighter_index][ff]->height / 4);

			clear_bitmap(shad);
			ellipsefill(shad, shad->width / 2, shad->height / 2, shad->width / 2, shad->height / 2, makecol(128, 128, 128));
			draw_trans_sprite(double_buffer, shad, xx + (shad->width / 3) - 2, yy + cframes[fighter_index][ff]->height - shad->height / 2);
			delete shad;
		}
		else
		{
			// Enemy
			Raster *shad = new Raster(cframes[fighter_index][ff]->width, cframes[fighter_index][ff]->height / 4);

			clear_bitmap(shad);
			ellipsefill(shad, shad->width / 2, shad->height / 2, shad->width / 2, shad->height / 2, makecol(128, 128, 128));
			draw_trans_sprite(double_buffer, shad, xx, yy + cframes[fighter_index][ff]->height - shad->height / 2);
			delete (shad);
		}

		draw_sprite(double_buffer, cframes[fighter_index][ff], xx, yy);
	}

	if (dcur == 1)
	{
		draw_sprite(double_buffer, bptr, xx + (fr->cw / 2) - 8, yy - 8);
	}

	if ((vspell == 1) && (fighter_index >= PSIZE))
	{
		ff = fr->hp * 30 / fr->mhp;
		if ((fr->hp > 0) && (ff < 1))
		{
			ff = 1;
		}

		xx += fr->cw / 2;
		rect(double_buffer, xx - 16, yy + fr->cl + 2, xx + 15, yy + fr->cl + 5, 0);
		if (ff > AUGMENT_STRONGEST)
		{
			rectfill(double_buffer, xx - 15, yy + fr->cl + 3, xx - 15 + ff - 1, yy + fr->cl + 4, 40);
		}

		else if ((ff <= AUGMENT_STRONGEST) && (ff > AUGMENT_STRONG))
		{
			rectfill(double_buffer, xx - 15, yy + fr->cl + 3, xx - 15 + ff - 1, yy + fr->cl + 4, 104);
		}

		else if ((ff <= AUGMENT_STRONG) && (ff > AUGMENT_NORMAL))
		{
			rectfill(double_buffer, xx - 15, yy + fr->cl + 3, xx - 15 + ff - 1, yy + fr->cl + 4, 24);
		}
	}
}

/*! \brief Enemies defeated the player
 * \author Josh Bolduc
 * \date created ????????
 * \date updated
 *
 * Play some sad music and set the dead flag so that the game
 * will return to the main menu.
 */
static void enemies_win(void)
{
	Music.play_music("rain.s3m", 0);
	battle_render(0, 0, 0);
	/*  RB FIXME: rest()?  */
	Draw.blit2screen(0, 0);
	kq_wait(1000);
	sprintf(strbuf, _("%s was defeated!"), party[pidx[0]].name.c_str());
	Draw.menubox(double_buffer, 152 - (strlen(strbuf) * 4), 48, strlen(strbuf), 1, BLUE);
	Draw.print_font(double_buffer, 160 - (strlen(strbuf) * 4), 56, strbuf, FNORMAL);
	Draw.blit2screen(0, 0);
	Game.wait_enter();
	do_transition(TRANS_FADE_OUT, 4);
	alldead = 1;
}

/*! \brief Main fighting routine
 *
 * I don't really need to describe this :p
 *
 * \author Josh Bolduc
 * \date created ????????
 * \date updated

 * \param   attack_fighter_index Attacker ID
 * \param   defend_fighter_index Defender ID
 * \param   sk If non-zero, override the attacker's stats.
 * \returns 1 if damage done, 0 otherwise
 */
int fight(size_t attack_fighter_index, size_t defend_fighter_index, int sk)
{
	int a;
	int tx = -1;
	int ty = -1;
	uint32_t f;
	uint32_t ares;
	size_t fighter_index;

	for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++)
	{
		deffect[fighter_index] = 0;
		ta[fighter_index] = 0;
	}

	/*  check the 'sk' variable to see if we are over-riding the  */
	/*  attackers stats with temporary ones... used for skills    */
	/*  and such                                                  */
	if (sk == 0)
	{
		tempa = Magic.status_adjust(attack_fighter_index);
	}

	tempd = Magic.status_adjust(defend_fighter_index);
	ares = attack_result(attack_fighter_index, defend_fighter_index);
	fighter[defend_fighter_index].SetPoisoned(tempd.IsPoisoned());
	fighter[defend_fighter_index].SetBlind(tempd.IsBlind());
	fighter[defend_fighter_index].SetCharmed(tempd.IsCharmed());
	fighter[defend_fighter_index].SetStopped(tempd.IsStopped());
	fighter[defend_fighter_index].SetStone(tempd.IsStone());
	fighter[defend_fighter_index].SetMute(tempd.IsMute());
	fighter[defend_fighter_index].SetSleep(tempd.IsAsleep());
	fighter[defend_fighter_index].SetDead(tempd.IsDead());
	fighter[defend_fighter_index].SetMalison(tempd.IsMalison());
	fighter[defend_fighter_index].SetResist(tempd.IsResist());
	fighter[defend_fighter_index].SetTime(tempd.IsTime());
	fighter[defend_fighter_index].SetShield(tempd.IsShield());
	fighter[defend_fighter_index].SetBless(tempd.IsBless());
	fighter[defend_fighter_index].SetStrength(tempd.IsStrength());
	fighter[defend_fighter_index].SetEther(tempd.IsEther());
	fighter[defend_fighter_index].SetTrueshot(tempd.IsTrueshot());
	fighter[defend_fighter_index].SetRegen(tempd.IsRegen());
	fighter[defend_fighter_index].SetInfuse(tempd.IsInfuse());

	/*  RB TODO: rest(20) or vsync() before the blit?  */
	if (ares == 2)
	{
		for (f = 0; f < 3; f++)
		{
			battle_render(defend_fighter_index + 1, 0, 0);
			Draw.blit2screen(0, 0);
			kq_wait(20);
			rectfill(double_buffer, 0, 0, KQ_SCREEN_W, KQ_SCREEN_H, 15);
			Draw.blit2screen(0, 0);
			kq_wait(20);
		}
	}

	if ((pidx[defend_fighter_index] == TEMMIN) &&
		(fighter[defend_fighter_index].aux == 2))
	{
		fighter[defend_fighter_index].aux = 1;
		a = 1 - defend_fighter_index;
		tx = fighter[defend_fighter_index].cx;
		ty = fighter[defend_fighter_index].cy;
		fighter[defend_fighter_index].cx = fighter[a].cx;
		fighter[defend_fighter_index].cy = fighter[a].cy - 16;
	}

	if (attack_fighter_index < PSIZE)
	{
		fighter[attack_fighter_index].aframe = 7;
	}
	else
	{
		fighter[attack_fighter_index].cy += 10;
	}

	fight_animation(defend_fighter_index, attack_fighter_index, 0);
	if (attack_fighter_index < PSIZE)
	{
		fighter[attack_fighter_index].aframe = 0;
	}
	else
	{
		fighter[attack_fighter_index].cy -= 10;
	}

	if ((tx != -1) && (ty != -1))
	{
		fighter[defend_fighter_index].cx = tx;
		fighter[defend_fighter_index].cy = ty;
	}

	if (ta[defend_fighter_index] != MISS)
	{
		ta[defend_fighter_index] = Magic.do_shield_check(defend_fighter_index, ta[defend_fighter_index]);
	}

	display_amount(defend_fighter_index, FONT_DECIDE, 0);
	if (ta[defend_fighter_index] != MISS)
	{
		fighter[defend_fighter_index].hp += ta[defend_fighter_index];
		if ((fighter[attack_fighter_index].imb_s > 0) && (kqrandom->random_range_exclusive(0, 5) == 0))
		{
			Magic.cast_imbued_spell(
				attack_fighter_index, fighter[attack_fighter_index].imb_s,
				fighter[attack_fighter_index].imb_a, defend_fighter_index);
		}

		if ((fighter[defend_fighter_index].hp <= 0) &&
			(fighter[defend_fighter_index].IsAlive()))
		{
			fkill(defend_fighter_index);
			death_animation(defend_fighter_index, 0);
		}

		if (fighter[defend_fighter_index].hp > fighter[defend_fighter_index].mhp)
		{
			fighter[defend_fighter_index].hp = fighter[defend_fighter_index].mhp;
		}

		if (fighter[defend_fighter_index].IsAsleep())
		{
			fighter[defend_fighter_index].SetSleep(0);
		}

		if ((fighter[defend_fighter_index].IsCharmed()) &&
			(attack_fighter_index == defend_fighter_index))
		{
			fighter[defend_fighter_index].SetCharmed(0);
		}

		return 1;
	}

	return 0;
}

/*! \brief Kill a fighter
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated 20020917 (PH) -- added cheat mode
 *
 * Do what it takes to put a fighter out of commission.
 *
 * \param   fighter_index The one who will die
 */
void fkill(size_t fighter_index)
{
#ifdef KQ_CHEATS
	/* PH Combat cheat - when a hero dies s/he is mysteriously boosted back
	 * to full HP.
	 */
	if (cheat && fighter_index < PSIZE)
	{
		fighter[fighter_index].hp = fighter[fighter_index].mhp;
		return;
	}
#endif

	fighter[fighter_index].SetPoisoned(0);
	fighter[fighter_index].SetBlind(0);
	fighter[fighter_index].SetCharmed(0);
	fighter[fighter_index].SetStopped(0);
	fighter[fighter_index].SetStone(0);
	fighter[fighter_index].SetMute(0);
	fighter[fighter_index].SetSleep(0);
	fighter[fighter_index].SetDead(true);
	fighter[fighter_index].SetMalison(0);
	fighter[fighter_index].SetResist(0);
	fighter[fighter_index].SetTime(0);
	fighter[fighter_index].SetShield(0);
	fighter[fighter_index].SetBless(0);
	fighter[fighter_index].SetStrength(0);
	fighter[fighter_index].SetEther(0);
	fighter[fighter_index].SetTrueshot(0);
	fighter[fighter_index].SetRegen(0);
	fighter[fighter_index].SetInfuse(0);

	fighter[fighter_index].hp = 0;
	if (fighter_index < PSIZE)
	{
		fighter[fighter_index].defeat_item_common = 0;
	}

	deffect[fighter_index] = 1;
	cact[fighter_index] = 0;
}

/*! \brief Player defeated the enemies
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Distribute the booty!
 */
static void heroes_win(void)
{
	int tgp = 0;
	size_t fighter_index;
	int b;
	size_t pidx_index;
	int z;
	int nc = 0;
	int txp = 0;
	int found_item = 0;
	int nr = 0;
	int ent = 0;
	KFighter t1;
	KFighter t2;

	Music.play_music("rend5.s3m", 0);
	kq_wait(500);
	kmenu.revert_equipstats();
	for (fighter_index = 0; fighter_index < numchrs; fighter_index++)
	{
		fighter[fighter_index].aframe = 4;
	}

	battle_render(0, 0, 0);
	Draw.blit2screen(0, 0);
	kq_wait(250);
	for (fighter_index = 0; fighter_index < numchrs; fighter_index++)
	{
		if (!fighter[fighter_index].IsStone() &&
			fighter[fighter_index].IsAlive())
		{
			nc++;
		}

		ta[fighter_index] = 0;
	}

	for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies; fighter_index++)
	{
		txp += fighter[fighter_index].xp;
		tgp += fighter[fighter_index].gp;
	}

	/*  JB: nc should never be zero if we won, but whatever  */
	if (nc > 0)
	{
		txp /= nc;
	}

	Game.AddGold(tgp);
	if (tgp > 0)
	{
		sprintf(strbuf, _("Gained %d xp and found %d gp."), txp, tgp);
	}
	else
	{
		sprintf(strbuf, _("Gained %d xp."), txp);
	}

	Draw.menubox(double_buffer, 152 - (strlen(strbuf) * 4), 8, strlen(strbuf), 1, BLUE);
	Draw.print_font(double_buffer, 160 - (strlen(strbuf) * 4), 16, strbuf, FNORMAL);
	Draw.blit2screen(0, 0);
	fullblit(double_buffer, back);
	for (fighter_index = 0; fighter_index < num_enemies; fighter_index++)
	{
		/* PH bug: (?) should found_item be reset to zero at the start of this loop?
		 * If you defeat 2 enemies, you should (possibly) get 2 items, right?
		 */
		if (kqrandom->random_range_exclusive(0, 100) < fighter[fighter_index + PSIZE].dip)
		{
			if (fighter[fighter_index + PSIZE].defeat_item_common > 0)
			{
				found_item = fighter[fighter_index + PSIZE].defeat_item_common;
			}

			if (fighter[fighter_index + PSIZE].defeat_item_rare > 0)
			{
				if (kqrandom->random_range_exclusive(0, 100) < 5)
				{
					found_item = fighter[fighter_index + PSIZE].defeat_item_rare;
				}
			}

			if (found_item > 0)
			{
				if (check_inventory(found_item, 1) != 0)
				{
					sprintf(strbuf, _("%s found!"), items[found_item].name);
					Draw.menubox(double_buffer, 148 - (strlen(strbuf) * 4), nr * 24 + 48, strlen(strbuf) + 1, 1, BLUE);
					Draw.draw_icon(double_buffer, items[found_item].icon, 156 - (strlen(strbuf) * 4), nr * 24 + 56);
					Draw.print_font(double_buffer, 164 - (strlen(strbuf) * 4), nr * 24 + 56, strbuf, FNORMAL);
					nr++;
				}
			}
		}
	}

	if (nr > 0)
	{
		Draw.blit2screen(0, 0);
		Game.wait_enter();
		fullblit(back, double_buffer);
	}

	nr = 0;
	for (pidx_index = 0; pidx_index < numchrs; pidx_index++)
	{
		if (!party[pidx[pidx_index]].IsStone() &&
			party[pidx[pidx_index]].IsAlive())
		{
			b = pidx_index * 160;
			player2fighter(pidx[pidx_index], &t1);
			if (kmenu.give_xp(pidx[pidx_index], txp, 0))
			{
				Draw.menubox(double_buffer, b, 40, 18, 9, BLUE);
				player2fighter(pidx[pidx_index], &t2);
				Draw.print_font(double_buffer, b + 8, 48, _("Level up!"), FGOLD);
				Draw.print_font(double_buffer, b + 8, 56, _("Max HP"), FNORMAL);
				Draw.print_font(double_buffer, b + 8, 64, _("Max MP"), FNORMAL);
				Draw.print_font(double_buffer, b + 8, 72, _("Strength"), FNORMAL);
				Draw.print_font(double_buffer, b + 8, 80, _("Agility"), FNORMAL);
				Draw.print_font(double_buffer, b + 8, 88, _("Vitality"), FNORMAL);
				Draw.print_font(double_buffer, b + 8, 96, _("Intellect"), FNORMAL);
				Draw.print_font(double_buffer, b + 8, 104, _("Sagacity"), FNORMAL);
				sprintf(strbuf, "%3d>", t1.mhp);
				Draw.print_font(double_buffer, b + 96, 56, strbuf, FNORMAL);
				sprintf(strbuf, "%3d", t2.mhp);
				Draw.print_font(double_buffer, b + 128, 56, strbuf, FGREEN);
				sprintf(strbuf, "%3d>", t1.mmp);
				Draw.print_font(double_buffer, b + 96, 64, strbuf, FNORMAL);
				sprintf(strbuf, "%3d", t2.mmp);
				Draw.print_font(double_buffer, b + 128, 64, strbuf, FGREEN);

				for (z = 0; z < 5; z++)
				{
					sprintf(strbuf, "%3d>", t1.stats[z]);
					Draw.print_font(double_buffer, b + 96, z * 8 + 72, strbuf, FNORMAL);
					sprintf(strbuf, "%3d", t2.stats[z]);
					if (t2.stats[z] > t1.stats[z])
					{
						Draw.print_font(double_buffer, b + 128, z * 8 + 72, strbuf, FGREEN);
					}
					else
					{
						Draw.print_font(double_buffer, b + 128, z * 8 + 72, strbuf, FNORMAL);
					}
				}

				nr++;
			}
			else
			{
				Draw.menubox(double_buffer, b, 104, 18, 1, BLUE);
			}

			sprintf(strbuf, _("Next level %7d"), party[pidx[pidx_index]].next - party[pidx[pidx_index]].xp);
			Draw.print_font(double_buffer, b + 8, 112, strbuf, FGOLD);
		}
	}

	Draw.blit2screen(0, 0);
	for (pidx_index = 0; pidx_index < numchrs; pidx_index++)
	{
		if (!party[pidx[pidx_index]].IsStone() &&
			party[pidx[pidx_index]].IsAlive())
		{
			ent += learn_new_spells(pidx[pidx_index]);
		}
	}

	if (ent == 0)
	{
		Game.wait_enter();
	}
}

/*! \brief Initiate fighter structs and initial vars
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Pre-combat setup of fighter structures and initial vars.
 */
static void init_fighters(void)
{
	size_t fighter_index;

	for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++)
	{
		deffect[fighter_index] = 0;
		fighter[fighter_index].mhp = 0;
		fighter[fighter_index].aux = 0;
		/* .defend was not initialized; patch supplied by Sam H */
		fighter[fighter_index].defend = 0;
	}

	/* TT: These two are only called once in the game.
	 *     Should we move them here?
	 */
	hero_init();
	Enemy.Init();
	for (fighter_index = 0; fighter_index < (PSIZE + num_enemies);
		fighter_index++)
	{
		nspeed[fighter_index] = (fighter[fighter_index].stats[A_SPD] + 50) / 5;
	}
}

/*! \brief Attack all enemies at once
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * This is different than fight in that all enemies are attacked
 * simultaneously, once. As a note, the attackers stats are
 * always over-ridden in this function. As well, critical hits
 * are possible, but the screen doesn't flash.
 *
 * \param   attack_fighter_index Attacker
 */
void multi_fight(size_t attack_fighter_index)
{
	size_t fighter_index;
	size_t start_fighter_index;
	size_t end_fighter_index;
	uint32_t deadcount = 0;
	uint32_t killed_warrior[NUM_FIGHTERS];
	// uint32_t ares[NUM_FIGHTERS];

	for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++)
	{
		deffect[fighter_index] = 0;
		ta[fighter_index] = 0;
		killed_warrior[fighter_index] = 0;
	}

	if (attack_fighter_index < PSIZE)
	{
		// if the attacker is you, target enemies
		start_fighter_index = PSIZE;
		end_fighter_index = num_enemies;
	}
	else
	{
		// if the attacker is enemy, target your party
		start_fighter_index = 0;
		end_fighter_index = numchrs;
	}

	for (fighter_index = start_fighter_index; fighter_index < start_fighter_index + end_fighter_index; fighter_index++)
	{
		tempd = Magic.status_adjust(fighter_index);
		if ((fighter[fighter_index].IsAlive()) &&
			(fighter[fighter_index].mhp > 0))
		{
			// ares[fighter_index] = attack_result(attack_fighter_index, fighter_index);
			fighter[fighter_index].SetPoisoned(tempd.IsPoisoned());
			fighter[fighter_index].SetBlind(tempd.IsBlind());
			fighter[fighter_index].SetCharmed(tempd.IsCharmed());
			fighter[fighter_index].SetStopped(tempd.IsStopped());
			fighter[fighter_index].SetStone(tempd.IsStone());
			fighter[fighter_index].SetMute(tempd.IsMute());
			fighter[fighter_index].SetSleep(tempd.IsAsleep());
			fighter[fighter_index].SetDead(tempd.IsDead());
			fighter[fighter_index].SetMalison(tempd.IsMalison());
			fighter[fighter_index].SetResist(tempd.IsResist());
			fighter[fighter_index].SetTime(tempd.IsTime());
			fighter[fighter_index].SetShield(tempd.IsShield());
			fighter[fighter_index].SetBless(tempd.IsBless());
			fighter[fighter_index].SetStrength(tempd.IsStrength());
			fighter[fighter_index].SetEther(tempd.IsEther());
			fighter[fighter_index].SetTrueshot(tempd.IsTrueshot());
			fighter[fighter_index].SetRegen(tempd.IsRegen());
			fighter[fighter_index].SetInfuse(tempd.IsInfuse());
		}

		if (ta[fighter_index] != MISS)
		{
			if (ta[fighter_index] != MISS)
			{
				ta[fighter_index] = Magic.do_shield_check(fighter_index, ta[fighter_index]);
			}

			fighter[fighter_index].hp += ta[fighter_index];
			if ((fighter[fighter_index].hp <= 0) &&
				(fighter[fighter_index].IsAlive()))
			{
				fighter[fighter_index].hp = 0;
				killed_warrior[fighter_index] = 1;
			}

			/*  RB: check we always have less health points than the maximun  */
			if (fighter[fighter_index].hp > fighter[fighter_index].mhp)
			{
				fighter[fighter_index].hp = fighter[fighter_index].mhp;
			}

			/*  RB: if sleeping, a good hit wakes him/her up  */
			if (fighter[fighter_index].IsAsleep())
			{
				fighter[fighter_index].SetSleep(0);
			}

			/*  RB: if charmed, a good hit wakes him/her up  */
			if (fighter[fighter_index].IsCharmed() && ta[fighter_index] > 0 &&
				attack_fighter_index == fighter_index)
			{
				fighter[fighter_index].SetCharmed(0);
			}
		}
	}

	if (attack_fighter_index < PSIZE)
	{
		fighter[attack_fighter_index].aframe = 7;
	}
	else
	{
		fighter[attack_fighter_index].cy += 10;
	}

	fight_animation(start_fighter_index, attack_fighter_index, 1);
	if (attack_fighter_index < PSIZE)
	{
		fighter[attack_fighter_index].aframe = 0;
	}
	else
	{
		fighter[attack_fighter_index].cy -= 10;
	}

	display_amount(start_fighter_index, FONT_DECIDE, 1);
	for (fighter_index = start_fighter_index; fighter_index < start_fighter_index + end_fighter_index; fighter_index++)
	{
		if (killed_warrior[fighter_index] != 0)
		{
			fkill(fighter_index);
			deadcount++;
		}
	}

	if (deadcount > 0)
	{
		death_animation(start_fighter_index, 1);
	}
}

/*! \brief Choose who attacks first, speeds, etc.
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Set up surprise vars, speeds, act vars, etc.
 */
static void roll_initiative(void)
{
	size_t fighter_index, j;

	if (hs == 1 && ms == 1)
	{
		hs = 10;
		ms = 10;
	}

	for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++)
	{
		fighter[fighter_index].csmem = 0;
		fighter[fighter_index].ctmem = 0;
		cact[fighter_index] = 1;
		j = ROUND_MAX * 66 / 100;
		if (j < 1)
		{
			j = 1;
		}

		bspeed[fighter_index] = kqrandom->random_range_exclusive(0, j);
	}

	for (fighter_index = 0; fighter_index < numchrs; fighter_index++)
	{
		if (ms == 1)
		{
			bspeed[fighter_index] = ROUND_MAX;
		}
		else if (hs == 1)
		{
			bspeed[fighter_index] = 0;
		}
	}

	for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies; fighter_index++)
	{
		if (hs == 1)
		{
			bspeed[fighter_index] = ROUND_MAX;
		}
		else if (ms == 1)
		{
			bspeed[fighter_index] = 0;
		}
	}

	RemainingBattleCounter = 0;

	/* PH: This should be ok */
	for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++)
	{
		if (fighter_index < numchrs ||
			(fighter_index >= PSIZE && fighter_index < (PSIZE + num_enemies)))
		{
			for (j = 0; j < 2; j++)
			{
				if (fighter[fighter_index].imb[j] > 0)
				{
					Magic.cast_imbued_spell(fighter_index, fighter[fighter_index].imb[j], 1, TGT_CASTER);
				}
			}
		}
	}

	battle_render(-1, -1, 0);
	Draw.blit2screen(0, 0);
	if ((hs == 1) && (ms > 1))
	{
		Draw.message(_("You have been ambushed!"), 255, 1500, 0, 0);
	}

	if ((hs > 1) && (ms == 1))
	{
		Draw.message(_("You've surprised the enemy!"), 255, 1500, 0, 0);
	}
}

/*! \brief Fighter on-screen locations in battle
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Calculate where the fighters should be drawn.
 */
static void snap_togrid(void)
{
	size_t fighter_index;
	int hf = 0;
	int mf = 1;
	int a;

	if (hs == 1)
	{
		hf = 1;
	}

	if (ms == 1)
	{
		mf = 0;
	}

	for (fighter_index = 0; fighter_index < numchrs; fighter_index++)
	{
		fighter[fighter_index].facing = hf;
	}

	for (fighter_index = PSIZE; fighter_index < (PSIZE + num_enemies);
		fighter_index++)
	{
		fighter[fighter_index].facing = mf;
	}

	hf = 170 - (numchrs * 24);
	for (fighter_index = 0; fighter_index < numchrs; fighter_index++)
	{
		fighter[fighter_index].cx = fighter_index * 48 + hf;
		fighter[fighter_index].cy = 128;
	}

	a = fighter[PSIZE].cw + 16;
	mf = 170 - (num_enemies * a / 2);
	for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies; fighter_index++)
	{
		fighter[fighter_index].cx = (fighter_index - PSIZE) * a + mf;

		if (fighter[fighter_index].cl < 104)
		{
			fighter[fighter_index].cy = 104 - fighter[fighter_index].cl;
		}
		else
		{
			fighter[fighter_index].cy = 8;
		}
	}
}
