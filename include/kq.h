#pragma once

/*! \file
 * \brief Main include file for KQ
 * \author JB
 * \date ??????
 */

/* Have to undef some stuff because Allegro defines it - thanks guys
*/
#ifdef HAVE_CONFIG_H
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_BUGREPORT
#include "config.h"
#endif

#ifdef __GNUC__
#define NORETURN __attribute__((noreturn))
#else
#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#else
#define NORETURN
#endif /* MSVC */
#endif /* GNUC */

#include <stdint.h>
#include <string>
using std::string;

#include "gettext.h"
#define _(s) gettext(s)

#include "constants.h"
#include "entity.h"
#include "enums.h"
#include "fighter.h"
#include "heroc.h"
#include "maps.h"
#include "structs.h"

class Raster;

class KGame
{
public:
	KGame();

	void change_map(const string &, int, int, int, int);
	void change_mapm(const string &, const string &, int, int);
	void calc_viewport(int);
	void zone_check(void);
	void warp(int, int, int);
	void do_check_animation(void);
	void activate(void);
	void unpress(void);
	void wait_enter(void);
	void klog(const char *);
	void init_players(void);
	void kwait(int);
	NORETURN void program_death(const char *);
	size_t in_party(ePIDX);
	void wait_for_entity(size_t, size_t);
	char *get_timer_event(void);
	int add_timer_event(const char *, int);
	void reset_timer_events(void);
	void reset_world(void);

	/*! Yield processor to other tasks */
	void kq_yield(void);

	Raster *alloc_bmp(int bitmap_width, int bitmap_height, const char *bitmap_name);

	void startup(void);
	void deallocate_stuff(void);

	void allocate_stuff(void);
	void load_heroes(void);
	void prepare_map(int, int, int, int);
	void data_dump(void);

	string GetCurmap() { return m_curmap; }
	void SetCurmap(string curmap) { m_curmap = curmap; }

	bool IsOverworldMap() { return m_curmap == WORLD_MAP; }

public:
	const string WORLD_MAP;
	/*! The number of frames per second */
	const int32_t KQ_TICKS;

protected:
	/*! Name of the current map */
	string m_curmap;
};

extern int vx, vy, mx, my, steps, lastm[PSIZE];

extern Raster *double_buffer, *fx_buffer;
extern Raster *map_icons[MAX_TILES];

extern Raster *back, *tc, *tc2, *bub[8], *b_shield, *b_shell, *b_repulse, *b_mp;
extern Raster *cframes[NUM_FIGHTERS][MAXCFRAMES], *tcframes[NUM_FIGHTERS][MAXCFRAMES], *frames[MAXCHRS][MAXFRAMES];
extern Raster *eframes[MAXE][MAXEFRAMES], *pgb[9], *sfonts[5], *bord[8];
extern Raster *menuptr, *mptr, *sptr, *stspics, *sicons, *bptr, *missbmp, *noway, *upptr, *dnptr;
extern Raster *shadow[MAX_SHADOWS];
extern unsigned short *map_seg;
extern unsigned short *b_seg, *f_seg;
extern unsigned char *z_seg, *s_seg, *o_seg;
extern unsigned char progress[SIZE_PROGRESS];
extern unsigned char treasure[SIZE_TREASURE];
extern unsigned char save_spells[SIZE_SAVE_SPELL];
extern Raster *kfonts;
extern s_map g_map;
extern KQEntity g_ent[MAX_ENTITIES];
extern s_anim tanim[MAX_TILESETS][MAX_ANIM];
extern s_anim adata[MAX_ANIM];
extern uint32_t numchrs;
extern int gp, xofs, yofs, gsvol, gmvol;
extern uint32_t noe;
extern ePIDX pidx[MAXCHRS];
extern uint8_t autoparty, alldead, is_sound, deadeffect, vfollow, use_sstone, sound_avail;
extern const uint8_t kq_version;
extern uint8_t hold_fade, cansave, skip_intro, wait_retrace, windowed, stretch_view, cpu_usage;
extern uint16_t tilex[MAX_TILES], adelay[MAX_ANIM];
extern char *strbuf, *savedir;
extern s_player party[MAXCHRS];
extern s_heroinfo players[MAXCHRS];
extern bool display_attack_string;
extern string shop_name;
extern char attack_string[39];
extern volatile int timer, ksec, kmin, khr, animation_count, timer_count;
extern COLOR_MAP cmap;
extern uint8_t can_run, display_desc;
extern uint8_t draw_background, draw_middle, draw_foreground, draw_shadow;
extern s_inventory g_inv[MAX_INV];
extern s_special_item special_items[MAX_SPECIAL_ITEMS];
extern short player_special_items[MAX_SPECIAL_ITEMS];
extern int view_x1, view_y1, view_x2, view_y2, view_on, in_combat;
extern int show_frate, use_joy;

/*! Variables used with KQ_CHEATS */
extern int cheat;
extern int no_random_encounters;
extern int every_hit_999;
extern int no_monsters;

#ifdef DEBUGMODE
extern Raster *obj_mesh;
#endif

extern KGame Game;

#ifndef TRACE
extern void TRACE(const char *message, ...);
#endif
