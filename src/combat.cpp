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
int rcount;
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
eAttackResult attack_result(int ar, int dr) {
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
  if (tempa.fighterMaxHealth > 250) {
    if (tempa.fighterHealth <= 50) {
      attacker_critical_status = 1;
    }
  } else {
    if (tempa.fighterHealth <= (tempa.fighterMaxHealth / 5)) {
      attacker_critical_status = 1;
    }
  }

  /*  JB: check to see if the defender is 'defending'  */
  if (tempd.defend == 1) {
    defender_defense = (defender_defense * 3) / 2;
  }

  /*  JB: if the attacker is empowered by trueshot  */
  if (tempa.sts[S_TRUESHOT] > 0) {
    fighter[ar].sts[S_TRUESHOT] = 0;
    defender_evade = 0;
  }

  attacker_attack += (tempa.stats[tempa.bstat] * tempa.bonus / 100);
  if (attacker_attack < DMG_RND_MIN * 5) {
    base = kqrandom->random_range_exclusive(0, DMG_RND_MIN);
  } else {
    base = kqrandom->random_range_exclusive(0, attacker_attack / 5);
  }

  base += attacker_attack - defender_defense;
  if (base < 1) {
    base = 1;
  }

  mult = 0;
  to_hit = attacker_hit + defender_evade;
  if (to_hit < 1) {
    to_hit = 1;
  }

  if (kqrandom->random_range_exclusive(0, to_hit) < attacker_hit) {
    mult++;
  }

  /*  JB: If the defender is etherealized, set mult to 0  */
  if (tempd.sts[S_ETHER] > 0) {
    mult = 0;
  }

  if (mult > 0) {
    if (tempd.crit == 1) {
      check_for_critical_hit = 1;
      if (attacker_critical_status == 1) {
        check_for_critical_hit = 2;
      }

      /* PH I _think_ this makes Sensar 2* as likely to make a critical hit */
      if (pidx[ar] == SENSAR) {
        check_for_critical_hit = check_for_critical_hit * 2;
      }

      check_for_critical_hit = (20 - check_for_critical_hit);
      if (kqrandom->random_range_exclusive(0, 20) >= check_for_critical_hit) {
        crit_hit = 1;
        base = ((int)base * 3) / 2;
      }
    }

    /*  JB: if affected by a NAUSEA/MALISON spell, the defender  */
    /*      takes more damage than normal                        */
    if (tempd.sts[S_MALISON] > 0) {
      base *= (int)5 / 4;
    }

    /*  JB: check for elemental/status weapons  */
    if (base < 1) {
      base = 1;
    }

    c = attacker_weapon_element - 1;
    if ((c >= R_EARTH) && (c <= R_ICE)) {
      base = res_adjust(dr, c, base);
    }

    if ((c >= R_POISON) && (c <= R_SLEEP)) {
      if ((res_throw(dr, c) == 0) && (fighter[dr].sts[c - R_POISON] == 0)) {
        if (non_dmg_save(dr, 50) == 0) {
          if ((c == R_POISON) || (c == R_PETRIFY) || (c == R_SILENCE)) {
            tempd.sts[c - R_POISON] = 1;
          } else {
            tempd.sts[c - R_POISON] = kqrandom->random_range_exclusive(2, 5);
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
  if (cheat && every_hit_999) {
    ta[dr] = -999;
    return ATTACK_SUCCESS;
  }
#endif

  dmg = mult * base;
  if (dmg == 0) {
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
 * \param   sall Select all
 */
void battle_render(signed int plyr, size_t hl, int sall) {
  int a = 0;
  int b = 0;
  int sz;
  int t;
  size_t z;
  size_t current_fighter_index = 0;
  size_t fighter_index = 0;

  if (plyr > 0) {
    current_fighter_index = plyr - 1;
    curw = fighter[current_fighter_index].fighterImageDatafileWidth;
    curx = fighter[current_fighter_index].fighterImageDatafileX;
    cury = fighter[current_fighter_index].fighterImageDatafileY;
  } else {
    curx = -1;
    cury = -1;
  }

  clear_bitmap(double_buffer);
  blit(backart, double_buffer, 0, 0, 0, 0, KQ_SCREEN_W, KQ_SCREEN_H);

  if ((sall == 0) && (curx > -1) && (cury > -1)) {
    draw_sprite(double_buffer, bptr, curx + (curw / 2) - 8, cury - 8);
    if (current_fighter_index >= PSIZE) {
      current_fighter_index = plyr - 1;
      t = curx + (curw / 2);
      t -= (fighter[current_fighter_index].fighterName.length() * 4);
      z = (fighter[current_fighter_index].fighterImageDatafileY < 32
		  ? fighter[current_fighter_index].fighterImageDatafileY + fighter[current_fighter_index].fighterImageDatafileHeight
		  : fighter[current_fighter_index].fighterImageDatafileY - 32);

      menubox(double_buffer, t - 8, z, fighter[current_fighter_index].fighterName.length(), 1, BLUE);
      print_font(double_buffer, t, z + 8, fighter[current_fighter_index].fighterName.c_str(), FNORMAL);
    }
  }

  auto x_offset = 216;
  for (z = 0; z < numchrs; z++) {
    b = z * x_offset;

    if (fighter[z].sts[S_DEAD] == 0) {
      draw_fighter(z, (sall == 1));
    } else {
      fighter[z].aframe = 3;
      draw_fighter(z, 0);
    }

    menubox(double_buffer, b, 184, 11, 5, BLUE);
    if (fighter[z].sts[S_DEAD] == 0) {
      sz = bspeed[z] * 88 / ROUND_MAX;
      if (sz > 88) {
        sz = 88;
      }

      a = 116;
      if (fighter[z].sts[S_TIME] == 1) {
        a = 83;
      }
      else if (fighter[z].sts[S_TIME] == 2) {
        a = 36;
      }

      a += (sz / 11);
      hline(double_buffer, b + 8, 229, b + sz + 8, a + 1);
      hline(double_buffer, b + 8, 230, b + sz + 8, a);
      hline(double_buffer, b + 8, 231, b + sz + 8, a - 1);
    }

    print_font(double_buffer, b + 8, 192, fighter[z].fighterName.c_str(), (hl == z + 1) ? FGOLD : FNORMAL);

    sprintf(strbuf, _("HP: %3d/%3d"), fighter[z].fighterHealth, fighter[z].fighterMaxHealth);
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

    print_font(double_buffer, b + 8, 208, strbuf, (fighter[z].fighterHealth < (fighter[z].fighterMaxHealth / 5)) ? FRED : FNORMAL);

    hline(double_buffer, b + 8, 216, b + 95, 21);
    sz = (fighter[z].fighterHealth > 0) ? fighter[z].fighterHealth * 88 / fighter[z].fighterMaxHealth : 88;

    hline(double_buffer, b + 8, 216, b + 8 + sz, 12);
    sprintf(strbuf, _("MP: %3d/%3d"), fighter[z].fighterMagic, fighter[z].fighterMaxMagic);

    /*  RB IDEA: Same suggestion as with health, just above.  */
    print_font(double_buffer, b + 8, 218, strbuf, (fighter[z].fighterMagic < (fighter[z].fighterMaxMagic / 5)) ? FRED : FNORMAL);
    hline(double_buffer, b + 8, 226, b + 95, 21);
    sz = (fighter[z].fighterMagic > 0) ? fighter[z].fighterMagic * 88 / fighter[z].fighterMaxMagic : 88;
    hline(double_buffer, b + 8, 226, b + 8 + sz, 12);
    draw_stsicon(double_buffer, 1, z, 17, b + 8, 200);
  }

  for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies;
       fighter_index++) {
    if (fighter[fighter_index].sts[S_DEAD] == 0) {
      draw_fighter(fighter_index, (sall == 2));
    }
  }

  if (display_attack_string == 1) {
    size_t ctext_length = strlen(attack_string) * 4;
    menubox(double_buffer, 152 - ctext_length, 8, strlen(attack_string), 1, BLUE);
    print_font(double_buffer, 160 - ctext_length, 16, attack_string, FNORMAL);
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
static int check_end(void) {
  size_t fighter_index;
  int alive = 0;

  /*  RB: count the number of heroes alive. If there is none, the   */
  /*      enemies won the battle.                                   */
  for (fighter_index = 0; fighter_index < numchrs; fighter_index++) {
    if (fighter[fighter_index].sts[S_DEAD] == 0) {
      alive++;
    }
  }

  if (alive == 0) {
    enemies_win();
    return 1;
  }

  /*  RB: count the number of enemies alive. If there is none, the  */
  /*      heroes won the battle.                                    */
  alive = 0;
  for (fighter_index = 0; fighter_index < num_enemies; fighter_index++) {
    if (fighter[fighter_index + PSIZE].sts[S_DEAD] == 0) {
      alive++;
    }
  }

  if (alive == 0) {
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
int combat(int bno) {
  int hero_level;
  int encounter;
  int lc;

#ifdef KQ_CHEATS
  if (cheat && no_monsters) {
    return 0;
  }
#endif

  /* PH: some checking! */
  if (bno < 0 || bno >= NUM_BATTLES) {
    sprintf(strbuf, _("Combat: battle %d does not exist."), bno);
    return 1;
    // program_death(strbuf);
  }

  /* TT: no battles during scripted/target movements */
  if (g_ent[0].movemode != MM_STAND) {
    return 0;
  }

  hero_level = party[pidx[0]].lvl;
  encounter = select_encounter(battles[bno].etnum, battles[bno].eidx);

  /*  RB: check if we had had a random encounter  */
  if (battles[bno].enc > 1) {
#ifdef KQ_CHEATS
    /* skip battle if no_random_encouters cheat is set */
    if (cheat && no_random_encounters) {
      return 0;
    }
#endif

    /* skip battle if haven't moved enough steps since last battle,
     * or if it's just not time for one yet */
    if ((steps < STEPS_NEEDED) || (kqrandom->random_range_exclusive(0, battles[bno].enc) > 0)) {
      return 0;
    }

    /* Likely (not always) skip random battle if repulse is active */
    if (save_spells[P_REPULSE] > 0) {
      lc = (hero_level - erows[encounter].lvl) * 20;
      if (lc < 5) {
        lc = 5;
      }

      /* Although Repulse is active, there's still a chance of battle */
      if (kqrandom->random_range_exclusive(0, 100) < lc) {
        return 0;
      }
    }
  }

  if (hero_level >= erows[encounter].lvl + 5 && battles[bno].eidx == 99) {
    lc = (hero_level - erows[encounter].lvl) * 5;

    /* TT: This will skip battles based on a random number from hero's
     *     level minus enemy's level.
     */
    if (kqrandom->random_range_exclusive(0, 100) < lc) {
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
static void do_action(size_t fighter_index) {
  size_t imb_index;
  uint8_t imbued_item;
  uint8_t spell_type_status;

  for (imb_index = 0; imb_index < 2; imb_index++) {
    imbued_item = fighter[fighter_index].imb[imb_index];
    if (imbued_item > 0) {
      cast_imbued_spell(fighter_index, imbued_item, 1, TGT_CASTER);
    }
  }

  spell_type_status = fighter[fighter_index].sts[S_MALISON];
  if (spell_type_status > 0) {
    if (kqrandom->random_range_exclusive(0, 100) < spell_type_status * 5) {
      cact[fighter_index] = 0;
    }
  }

  spell_type_status = fighter[fighter_index].sts[S_CHARM];
  if (spell_type_status > 0) {
    fighter[fighter_index].sts[S_CHARM]--;
    spell_type_status = fighter[fighter_index].sts[S_CHARM];

    if (fighter_index < PSIZE) {
      auto_herochooseact(fighter_index);
    } else {
      enemy_charmaction(fighter_index);
    }
  }

  if (cact[fighter_index] != 0) {
    revert_cframes(fighter_index, 0);
    if (fighter_index < PSIZE) {
      if (spell_type_status == 0) {
        hero_choose_action(fighter_index);
      }
    } else {
      enemy_chooseaction(fighter_index);
    }
  }

  cact[fighter_index] = 0;
  if (check_end() == 1) {
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
static int do_combat(char *bg, char *mus, int is_rnd) {
  int zoom_step;

  in_combat = 1;
  backart = get_cached_image(bg);
  if (is_rnd) {
    if ((numchrs == 1) && (pidx[0] == AYLA)) {
      hs = kqrandom->random_range_exclusive(1, 101);
      ms = kqrandom->random_range_exclusive(1, 4);
    } else {
      if (numchrs > 1 && (Game.in_party(AYLA) < MAXCHRS)) {
        hs = kqrandom->random_range_exclusive(1, 21);
        ms = kqrandom->random_range_exclusive(1, 6);
      } else {
        hs = kqrandom->random_range_exclusive(1, 11);
        ms = kqrandom->random_range_exclusive(1, 11);
      }
    }
  } else {
    hs = 10;
    ms = 10;
  }

  /*  RB: do the zoom at the beginning of the combat.  */
  Music.pause_music();
  Music.set_music_volume((gmvol / 250.0) * 0.75);
  Music.play_music(mus, 0);
  if (stretch_view == 2) {
    do_transition(TRANS_FADE_OUT, 2);
    clear_bitmap(double_buffer);
    do_transition(TRANS_FADE_IN, 64);
  } else {
    /* TT TODO:
     * Change this so when we zoom into the battle, it won't just zoom into the
     * middle
     * of the screen.  Instead, it's going to zoom into the location where the
     * player
     * is, so if he's on the side of the map somewhere...
     */
    std::unique_ptr<Raster> temp(copy_bitmap(nullptr, double_buffer));
    for (zoom_step = 0; zoom_step < 9; zoom_step++) {
      Music.poll_music();
      stretch_blit(temp.get(), double_buffer,
        zoom_step * (KQ_SCREEN_W / 20) + xofs,
        zoom_step * (KQ_SCREEN_H / 20) + yofs,
        KQ_SCREEN_W - (zoom_step * (KQ_SCREEN_W / 10)),
        KQ_SCREEN_H - (zoom_step * (KQ_SCREEN_H / 10)),
        0, 0,
        KQ_SCREEN_W, KQ_SCREEN_H);
      blit2screen(xofs, yofs);
    }
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
  if (alldead) {
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
static void do_round(void) {
  size_t a;
  size_t fighter_index;

  timer_count = 0;
  while (!combatend) {
    if (timer_count >= 10) {
      rcount += BATTLE_INC;

      if (rcount >= ROUND_MAX) {
        rcount = 0;
      }

      for (fighter_index = 0; fighter_index < PSIZE + num_enemies;
           fighter_index++) {
        if ((fighter_index < numchrs) || (fighter_index >= PSIZE)) {
          if (((fighter[fighter_index].sts[S_POISON] - 1) == rcount) && fighter[fighter_index].fighterHealth > 1) {
            a = kqrandom->random_range_exclusive(0, fighter[fighter_index].fighterMaxHealth / 20) + 1;

            if (a < 2) {
              a = 2;
            }

            if ((fighter[fighter_index].fighterHealth - a) < 1) {
              a = fighter[fighter_index].fighterHealth - 1;
            }

            ta[fighter_index] = a;
            display_amount(fighter_index, FONT_WHITE, 0);
            fighter[fighter_index].fighterHealth -= a;
          }

          /*  RB: the character is regenerating? when needed, get a  */
          /*      random value (never lower than 5), and increase    */
          /*      the character's health by that amount.             */
          if ((fighter[fighter_index].sts[S_REGEN] - 1) == rcount) {
            a = kqrandom->random_range_exclusive(0, 5) + (fighter[fighter_index].fighterMaxHealth / 10);

            if (a < 5) {
              a = 5;
            }

            ta[fighter_index] = a;
            display_amount(fighter_index, FONT_YELLOW, 0);
            adjust_hp(fighter_index, a);
          }

          /*  RB: the character has ether actived?  */
          cact[fighter_index] = 1;
          if ((fighter[fighter_index].sts[S_ETHER] > 0) && (rcount == 0)) {
            fighter[fighter_index].sts[S_ETHER]--;
          }

          /*  RB: the character is stopped?  */
          if (fighter[fighter_index].sts[S_STOP] > 0) {
            if (pidx[fighter_index] == TEMMIN) {
              fighter[fighter_index].aux = 0;
            }

            if (rcount == 0) {
              fighter[fighter_index].sts[S_STOP]--;
            }

            cact[fighter_index] = 0;
          }

          /*  RB: the character is sleeping?  */
          if (fighter[fighter_index].sts[S_SLEEP] > 0) {
            if (pidx[fighter_index] == TEMMIN) {
              fighter[fighter_index].aux = 0;
            }

            if (rcount == 0) {
              fighter[fighter_index].sts[S_SLEEP]--;
            }

            cact[fighter_index] = 0;
          }

          /*  RB: the character is petrified?  */
          if (fighter[fighter_index].sts[S_STONE] > 0) {
            if (pidx[fighter_index] == TEMMIN) {
              fighter[fighter_index].aux = 0;
            }

            if (rcount == 0) {
              fighter[fighter_index].sts[S_STONE]--;
            }

            cact[fighter_index] = 0;
          }

          if (fighter[fighter_index].sts[S_DEAD] != 0 || fighter[fighter_index].fighterMaxHealth <= 0) {
            if (pidx[fighter_index] == TEMMIN) {
              fighter[fighter_index].aux = 0;
            }

            bspeed[fighter_index] = 0;
            cact[fighter_index] = 0;
          }

          if (cact[fighter_index] > 0) {
            if (fighter[fighter_index].sts[S_TIME] == 0) {
              bspeed[fighter_index] += nspeed[fighter_index];
            } else {
              if (fighter[fighter_index].sts[S_TIME] == 1) {
                bspeed[fighter_index] += (nspeed[fighter_index] / 2 + 1);
              } else {
                bspeed[fighter_index] += (nspeed[fighter_index] * 2);
              }
            }
          }
        } else {
          cact[fighter_index] = 0;
        }
      }

      PlayerInput.readcontrols();
      battle_render(0, 0, 0);
      blit2screen(0, 0);

      for (fighter_index = 0; fighter_index < (PSIZE + num_enemies);
           fighter_index++) {
        if ((bspeed[fighter_index] >= ROUND_MAX) && (cact[fighter_index] > 0)) {
          do_action(fighter_index);
          fighter[fighter_index].ctmem = 0;
          fighter[fighter_index].csmem = 0;
          cact[fighter_index] = 1;
          bspeed[fighter_index] = 0;
        }

        if (combatend) {
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
	s_fighter* fr = &fighter[fighter_index];

	xx = fr->fighterImageDatafileX;
	yy = fr->fighterImageDatafileY;

	ff = (!fr->aframe) ? fr->facing : fr->aframe;

	if (fr->sts[S_STONE] > 0) {
		convert_cframes(fighter_index, 2, 12, 0);
	}

	if (fr->sts[S_ETHER] > 0) {
		draw_trans_sprite(double_buffer, cframes[fighter_index][ff], xx, yy);
	}
	else {
		if (fighter_index < PSIZE) {
			// Your party
			Raster *shad = new Raster(cframes[fighter_index][ff]->width * 2 / 3, cframes[fighter_index][ff]->height / 4);

			clear_bitmap(shad);
			ellipsefill(shad, shad->width / 2, shad->height / 2, shad->width / 2, shad->height / 2, makecol(128, 128, 128));
			draw_trans_sprite(double_buffer, shad, xx + (shad->width / 3) - 2, yy + cframes[fighter_index][ff]->height - shad->height / 2);
			delete shad;
		}
		else {
			// Enemy
			Raster *shad = new Raster(cframes[fighter_index][ff]->width, cframes[fighter_index][ff]->height / 4);

			clear_bitmap(shad);
			ellipsefill(shad, shad->width / 2, shad->height / 2, shad->width / 2, shad->height / 2, makecol(128, 128, 128));
			draw_trans_sprite(double_buffer, shad, xx, yy + cframes[fighter_index][ff]->height - shad->height / 2);
			delete (shad);
		}

		draw_sprite(double_buffer, cframes[fighter_index][ff], xx, yy);
	}

	if (dcur == 1) {
		draw_sprite(double_buffer, bptr, xx + (fr->fighterImageDatafileWidth / 2) - 8, yy - 8);
	}

	if ((vspell == 1) && (fighter_index >= PSIZE)) {
		ff = fr->fighterHealth * 30 / fr->fighterMaxHealth;
		if ((fr->fighterHealth > 0) && (ff < 1)) {
			ff = 1;
		}

		xx += fr->fighterImageDatafileWidth / 2;
		rect(double_buffer, xx - 16, yy + fr->fighterImageDatafileHeight + 2, xx + 15, yy + fr->fighterImageDatafileHeight + 5, 0);
		if (ff > AUGMENT_STRONGEST) {
			rectfill(double_buffer, xx - 15, yy + fr->fighterImageDatafileHeight + 3, xx - 15 + ff - 1, yy + fr->fighterImageDatafileHeight + 4, 40);
		}

		else if ((ff <= AUGMENT_STRONGEST) && (ff > AUGMENT_STRONG)) {
			rectfill(double_buffer, xx - 15, yy + fr->fighterImageDatafileHeight + 3, xx - 15 + ff - 1, yy + fr->fighterImageDatafileHeight + 4, 104);
		}

		else if ((ff <= AUGMENT_STRONG) && (ff > AUGMENT_NORMAL)) {
			rectfill(double_buffer, xx - 15, yy + fr->fighterImageDatafileHeight + 3, xx - 15 + ff - 1, yy + fr->fighterImageDatafileHeight + 4, 24);
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
static void enemies_win(void) {
  Music.play_music("rain.s3m", 0);
  battle_render(0, 0, 0);
  /*  RB FIXME: rest()?  */
  blit2screen(0, 0);
  kq_wait(1000);
  sprintf(strbuf, _("%s was defeated!"), party[pidx[0]].name);
  menubox(double_buffer, 152 - (strlen(strbuf) * 4), 48, strlen(strbuf), 1, BLUE);
  print_font(double_buffer, 160 - (strlen(strbuf) * 4), 56, strbuf, FNORMAL);
  blit2screen(0, 0);
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
int fight(size_t attack_fighter_index, size_t defend_fighter_index, int sk) {
  int a;
  int tx = -1;
  int ty = -1;
  uint32_t f;
  uint32_t ares;
  size_t fighter_index;
  size_t stats_index;

  for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++) {
    deffect[fighter_index] = 0;
    ta[fighter_index] = 0;
  }

  /*  check the 'sk' variable to see if we are over-riding the  */
  /*  attackers stats with temporary ones... used for skills    */
  /*  and such                                                  */
  if (sk == 0) {
    tempa = status_adjust(attack_fighter_index);
  }

  tempd = status_adjust(defend_fighter_index);
  ares = attack_result(attack_fighter_index, defend_fighter_index);
  for (stats_index = 0; stats_index < 24; stats_index++) {
    fighter[defend_fighter_index].sts[stats_index] = tempd.sts[stats_index];
  }

  /*  RB TODO: rest(20) or vsync() before the blit?  */
  if (ares == 2) {
    for (f = 0; f < 3; f++) {
      battle_render(defend_fighter_index + 1, 0, 0);
      blit2screen(0, 0);
      kq_wait(20);
      rectfill(double_buffer, 0, 0, KQ_SCREEN_W, KQ_SCREEN_H, 15);
      blit2screen(0, 0);
      kq_wait(20);
    }
  }

  if ((pidx[defend_fighter_index] == TEMMIN) &&
      (fighter[defend_fighter_index].aux == 2)) {
    fighter[defend_fighter_index].aux = 1;
    a = 1 - defend_fighter_index;
    tx = fighter[defend_fighter_index].fighterImageDatafileX;
    ty = fighter[defend_fighter_index].fighterImageDatafileY;
    fighter[defend_fighter_index].fighterImageDatafileX = fighter[a].fighterImageDatafileX;
    fighter[defend_fighter_index].fighterImageDatafileY = fighter[a].fighterImageDatafileY - 16;
  }

  if (attack_fighter_index < PSIZE) {
    fighter[attack_fighter_index].aframe = 7;
  } else {
    fighter[attack_fighter_index].fighterImageDatafileY += 10;
  }

  fight_animation(defend_fighter_index, attack_fighter_index, 0);
  if (attack_fighter_index < PSIZE) {
    fighter[attack_fighter_index].aframe = 0;
  } else {
    fighter[attack_fighter_index].fighterImageDatafileY -= 10;
  }

  if ((tx != -1) && (ty != -1)) {
    fighter[defend_fighter_index].fighterImageDatafileX = tx;
    fighter[defend_fighter_index].fighterImageDatafileY = ty;
  }

  if (ta[defend_fighter_index] != MISS) {
    ta[defend_fighter_index] =
        do_shield_check(defend_fighter_index, ta[defend_fighter_index]);
  }

  display_amount(defend_fighter_index, FONT_DECIDE, 0);
  if (ta[defend_fighter_index] != MISS) {
    fighter[defend_fighter_index].fighterHealth += ta[defend_fighter_index];
    if ((fighter[attack_fighter_index].imb_s > 0) && (kqrandom->random_range_exclusive(0, 5) == 0)) {
      cast_imbued_spell(attack_fighter_index, fighter[attack_fighter_index].imb_s, fighter[attack_fighter_index].imb_a, defend_fighter_index);
    }

    if ((fighter[defend_fighter_index].fighterHealth <= 0) && (fighter[defend_fighter_index].sts[S_DEAD] == 0)) {
      fkill(defend_fighter_index);
      death_animation(defend_fighter_index, 0);
    }

    if (fighter[defend_fighter_index].fighterHealth > fighter[defend_fighter_index].fighterMaxHealth) {
      fighter[defend_fighter_index].fighterHealth = fighter[defend_fighter_index].fighterMaxHealth;
    }

    if (fighter[defend_fighter_index].sts[S_SLEEP] > 0) {
      fighter[defend_fighter_index].sts[S_SLEEP] = 0;
    }

    if ((fighter[defend_fighter_index].sts[S_CHARM] > 0) &&
        (attack_fighter_index == defend_fighter_index)) {
      fighter[defend_fighter_index].sts[S_CHARM] = 0;
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
void fkill(size_t fighter_index) {
  size_t spell_index;

#ifdef KQ_CHEATS
  /* PH Combat cheat - when a hero dies s/he is mysteriously boosted back
   * to full HP.
   */
  if (cheat && fighter_index < PSIZE) {
    fighter[fighter_index].fighterHealth = fighter[fighter_index].fighterMaxHealth;
    return;
  }
#endif

  for (spell_index = 0; spell_index < 24; spell_index++) {
    fighter[fighter_index].sts[spell_index] = 0;
  }

  fighter[fighter_index].sts[S_DEAD] = 1;
  fighter[fighter_index].fighterHealth = 0;
  if (fighter_index < PSIZE) {
    fighter[fighter_index].fighterDefeatItemCommon = 0;
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
static void heroes_win(void) {
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
	s_fighter t1;
	s_fighter t2;

	Music.play_music("rend5.s3m", 0);
	kq_wait(500);
	revert_equipstats();
	for (fighter_index = 0; fighter_index < numchrs; fighter_index++) {
		fighter[fighter_index].aframe = 4;
	}

	battle_render(0, 0, 0);
	blit2screen(0, 0);
	kq_wait(250);
	for (fighter_index = 0; fighter_index < numchrs; fighter_index++) {
		if (fighter[fighter_index].sts[S_STONE] == 0 &&
			fighter[fighter_index].sts[S_DEAD] == 0) {
			nc++;
		}

		ta[fighter_index] = 0;
	}

	for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies;
		fighter_index++) {
		txp += fighter[fighter_index].fighterExperience;
		tgp += fighter[fighter_index].fighterMoney;
	}

	/*  JB: nc should never be zero if we won, but whatever  */
	if (nc > 0) {
		txp /= nc;
	}

	gp += tgp;
	if (tgp > 0) {
		sprintf(strbuf, _("Gained %d xp and found %d gp."), txp, tgp);
	}
	else {
		sprintf(strbuf, _("Gained %d xp."), txp);
	}

	menubox(double_buffer, 152 - (strlen(strbuf) * 4), 8, strlen(strbuf), 1, BLUE);
	print_font(double_buffer, 160 - (strlen(strbuf) * 4), 16, strbuf, FNORMAL);
	blit2screen(0, 0);
	fullblit(double_buffer, back);
	for (fighter_index = 0; fighter_index < num_enemies; fighter_index++) {
		/* PH bug: (?) should found_item be reset to zero at the start of this loop?
		 * If you defeat 2 enemies, you should (possibly) get 2 items, right?
		 */
		if (kqrandom->random_range_exclusive(0, 100) < fighter[fighter_index + PSIZE].fighterDefeatItemProbability) {
			if (fighter[fighter_index + PSIZE].fighterDefeatItemCommon > 0) {
				found_item = fighter[fighter_index + PSIZE].fighterDefeatItemCommon;
			}

			if (fighter[fighter_index + PSIZE].fighterDefeatItemRare > 0) {
				if (kqrandom->random_range_exclusive(0, 100) < 5) {
					found_item = fighter[fighter_index + PSIZE].fighterDefeatItemRare;
				}
			}

			if (found_item > 0) {
				if (check_inventory(found_item, 1) != 0) {
					sprintf(strbuf, _("%s found!"), items[found_item].name);
					menubox(double_buffer, 148 - (strlen(strbuf) * 4), nr * 24 + 48, strlen(strbuf) + 1, 1, BLUE);
					draw_icon(double_buffer, items[found_item].icon, 156 - (strlen(strbuf) * 4), nr * 24 + 56);
					print_font(double_buffer, 164 - (strlen(strbuf) * 4), nr * 24 + 56, strbuf, FNORMAL);
					nr++;
				}
			}
		}
	}

	if (nr > 0) {
		blit2screen(0, 0);
		Game.wait_enter();
		fullblit(back, double_buffer);
	}

	nr = 0;
	for (pidx_index = 0; pidx_index < numchrs; pidx_index++) {
		if (party[pidx[pidx_index]].sts[S_STONE] == 0 &&
			party[pidx[pidx_index]].sts[S_DEAD] == 0) {
			b = pidx_index * 160;
			player2fighter(pidx[pidx_index], &t1);
			if (give_xp(pidx[pidx_index], txp, 0) == 1) {
				menubox(double_buffer, b, 40, 18, 9, BLUE);
				player2fighter(pidx[pidx_index], &t2);
				print_font(double_buffer, b + 8, 48, _("Level up!"), FGOLD);
				print_font(double_buffer, b + 8, 56, _("Max HP"), FNORMAL);
				print_font(double_buffer, b + 8, 64, _("Max MP"), FNORMAL);
				print_font(double_buffer, b + 8, 72, _("Strength"), FNORMAL);
				print_font(double_buffer, b + 8, 80, _("Agility"), FNORMAL);
				print_font(double_buffer, b + 8, 88, _("Vitality"), FNORMAL);
				print_font(double_buffer, b + 8, 96, _("Intellect"), FNORMAL);
				print_font(double_buffer, b + 8, 104, _("Sagacity"), FNORMAL);
				sprintf(strbuf, "%3d>", t1.fighterMaxHealth);
				print_font(double_buffer, b + 96, 56, strbuf, FNORMAL);
				sprintf(strbuf, "%3d", t2.fighterMaxHealth);
				print_font(double_buffer, b + 128, 56, strbuf, FGREEN);
				sprintf(strbuf, "%3d>", t1.fighterMaxMagic);
				print_font(double_buffer, b + 96, 64, strbuf, FNORMAL);
				sprintf(strbuf, "%3d", t2.fighterMaxMagic);
				print_font(double_buffer, b + 128, 64, strbuf, FGREEN);

				for (z = 0; z < 5; z++) {
					sprintf(strbuf, "%3d>", t1.stats[z]);
					print_font(double_buffer, b + 96, z * 8 + 72, strbuf, FNORMAL);
					sprintf(strbuf, "%3d", t2.stats[z]);
					if (t2.stats[z] > t1.stats[z]) {
						print_font(double_buffer, b + 128, z * 8 + 72, strbuf, FGREEN);
					}
					else {
						print_font(double_buffer, b + 128, z * 8 + 72, strbuf, FNORMAL);
					}
				}

				nr++;
			}
			else {
				menubox(double_buffer, b, 104, 18, 1, BLUE);
			}

			sprintf(strbuf, _("Next level %7d"), party[pidx[pidx_index]].next - party[pidx[pidx_index]].xp);
			print_font(double_buffer, b + 8, 112, strbuf, FGOLD);
		}
	}

	blit2screen(0, 0);
	for (pidx_index = 0; pidx_index < numchrs; pidx_index++) {
		if (party[pidx[pidx_index]].sts[S_STONE] == 0 &&
			party[pidx[pidx_index]].sts[S_DEAD] == 0) {
			ent += learn_new_spells(pidx[pidx_index]);
		}
	}

	if (ent == 0) {
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
static void init_fighters(void) {
  size_t fighter_index;

  for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++) {
    deffect[fighter_index] = 0;
    fighter[fighter_index].fighterMaxHealth = 0;
    fighter[fighter_index].aux = 0;
    /* .defend was not initialized; patch supplied by Sam H */
    fighter[fighter_index].defend = 0;
  }

  /* TT: These two are only called once in the game.
   *     Should we move them here?
   */
  hero_init();
  enemy_init();
  for (fighter_index = 0; fighter_index < (PSIZE + num_enemies);
       fighter_index++) {
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
void multi_fight(size_t attack_fighter_index) {
  size_t fighter_index;
  size_t spell_index;
  size_t start_fighter_index;
  size_t end_fighter_index;
  uint32_t deadcount = 0;
  uint32_t killed_warrior[NUM_FIGHTERS];
  // uint32_t ares[NUM_FIGHTERS];

  for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++) {
    deffect[fighter_index] = 0;
    ta[fighter_index] = 0;
    killed_warrior[fighter_index] = 0;
  }

  if (attack_fighter_index < PSIZE) {
    // if the attacker is you, target enemies
    start_fighter_index = PSIZE;
    end_fighter_index = num_enemies;
  } else {
    // if the attacker is enemy, target your party
    start_fighter_index = 0;
    end_fighter_index = numchrs;
  }

  for (fighter_index = start_fighter_index;
       fighter_index < start_fighter_index + end_fighter_index;
       fighter_index++) {
    tempd = status_adjust(fighter_index);
    if ((fighter[fighter_index].sts[S_DEAD] == 0) && (fighter[fighter_index].fighterMaxHealth > 0)) {
      // ares[fighter_index] = attack_result(attack_fighter_index,
      // fighter_index);
      for (spell_index = 0; spell_index < 24; spell_index++) {
        fighter[fighter_index].sts[spell_index] = tempd.sts[spell_index];
      }
    }

    if (ta[fighter_index] != MISS) {
      if (ta[fighter_index] != MISS) {
        ta[fighter_index] = do_shield_check(fighter_index, ta[fighter_index]);
      }

      fighter[fighter_index].fighterHealth += ta[fighter_index];
      if ((fighter[fighter_index].fighterHealth <= 0) && (fighter[fighter_index].sts[S_DEAD] == 0)) {
        fighter[fighter_index].fighterHealth = 0;
        killed_warrior[fighter_index] = 1;
      }

      /*  RB: check we always have less health points than the maximum  */
      if (fighter[fighter_index].fighterHealth > fighter[fighter_index].fighterMaxHealth) {
        fighter[fighter_index].fighterHealth = fighter[fighter_index].fighterMaxHealth;
      }

      /*  RB: if sleeping, a good hit wakes him/her up  */
      if (fighter[fighter_index].sts[S_SLEEP] > 0) {
        fighter[fighter_index].sts[S_SLEEP] = 0;
      }

      /*  RB: if charmed, a good hit wakes him/her up  */
      if (fighter[fighter_index].sts[S_CHARM] > 0 && ta[fighter_index] > 0 &&
          attack_fighter_index == fighter_index) {
        fighter[fighter_index].sts[S_CHARM] = 0;
      }
    }
  }

  if (attack_fighter_index < PSIZE) {
    fighter[attack_fighter_index].aframe = 7;
  } else {
    fighter[attack_fighter_index].fighterImageDatafileY += 10;
  }

  fight_animation(start_fighter_index, attack_fighter_index, 1);
  if (attack_fighter_index < PSIZE) {
    fighter[attack_fighter_index].aframe = 0;
  } else {
    fighter[attack_fighter_index].fighterImageDatafileY -= 10;
  }

  display_amount(start_fighter_index, FONT_DECIDE, 1);
  for (fighter_index = start_fighter_index;
       fighter_index < start_fighter_index + end_fighter_index;
       fighter_index++) {
    if (killed_warrior[fighter_index] != 0) {
      fkill(fighter_index);
      deadcount++;
    }
  }

  if (deadcount > 0) {
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
static void roll_initiative(void) {
  size_t fighter_index, j;

  if (hs == 1 && ms == 1) {
    hs = 10;
    ms = 10;
  }

  for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++) {
    fighter[fighter_index].csmem = 0;
    fighter[fighter_index].ctmem = 0;
    cact[fighter_index] = 1;
    j = ROUND_MAX * 66 / 100;
    if (j < 1) {
      j = 1;
    }

    bspeed[fighter_index] = kqrandom->random_range_exclusive(0, j);
  }

  for (fighter_index = 0; fighter_index < numchrs; fighter_index++) {
    if (ms == 1) {
      bspeed[fighter_index] = ROUND_MAX;
    } else if (hs == 1) {
      bspeed[fighter_index] = 0;
    }
  }

  for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies;
       fighter_index++) {
    if (hs == 1) {
      bspeed[fighter_index] = ROUND_MAX;
    } else if (ms == 1) {
      bspeed[fighter_index] = 0;
    }
  }

  rcount = 0;

  /* PH: This should be ok */
  for (fighter_index = 0; fighter_index < NUM_FIGHTERS; fighter_index++) {
    if (fighter_index < numchrs ||
        (fighter_index >= PSIZE && fighter_index < (PSIZE + num_enemies))) {
      for (j = 0; j < 2; j++) {
        if (fighter[fighter_index].imb[j] > 0) {
          cast_imbued_spell(fighter_index, fighter[fighter_index].imb[j], 1, TGT_CASTER);
        }
      }
    }
  }

  battle_render(-1, -1, 0);
  blit2screen(0, 0);
  if ((hs == 1) && (ms > 1)) {
    message(_("You have been ambushed!"), 255, 1500, 0, 0);
  }

  if ((hs > 1) && (ms == 1)) {
    message(_("You've surprised the enemy!"), 255, 1500, 0, 0);
  }
}

/*! \brief Fighter on-screen locations in battle
 * \author Josh Bolduc
 * \date Created ????????
 * \date Updated
 *
 * Calculate where the fighters should be drawn.
 */
static void snap_togrid(void) {
  size_t fighter_index;
  int hf = 0;
  int mf = 1;
  int a;

  if (hs == 1) {
    hf = 1;
  }

  if (ms == 1) {
    mf = 0;
  }

  for (fighter_index = 0; fighter_index < numchrs; fighter_index++) {
    fighter[fighter_index].facing = hf;
  }

  for (fighter_index = PSIZE; fighter_index < (PSIZE + num_enemies);
       fighter_index++) {
    fighter[fighter_index].facing = mf;
  }

  hf = 170 - (numchrs * 24);
  for (fighter_index = 0; fighter_index < numchrs; fighter_index++) {
    fighter[fighter_index].fighterImageDatafileX = fighter_index * 48 + hf;
    fighter[fighter_index].fighterImageDatafileY = 128;
  }

  a = fighter[PSIZE].fighterImageDatafileWidth + 16;
  mf = 170 - (num_enemies * a / 2);
  for (fighter_index = PSIZE; fighter_index < PSIZE + num_enemies;
       fighter_index++) {
    fighter[fighter_index].fighterImageDatafileX = (fighter_index - PSIZE) * a + mf;

    if (fighter[fighter_index].fighterImageDatafileHeight < 104) {
      fighter[fighter_index].fighterImageDatafileY = 104 - fighter[fighter_index].fighterImageDatafileHeight;
    } else {
      fighter[fighter_index].fighterImageDatafileY = 8;
    }
  }
}
