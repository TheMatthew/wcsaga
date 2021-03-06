/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/





#include "object/object.h"
#include "ship/ship.h"
#include "globalincs/linklist.h"
#include "hud/hud.h"
#include "hud/hudmessage.h"
#include "hud/hudtargetbox.h"
#include "hud/hudescort.h"
#include "hud/hudshield.h"
#include "gamesnd/gamesnd.h"
#include "graphics/font.h"
#include "io/timer.h"
#include "weapon/emp.h"
#include "globalincs/alphacolors.h"
#include "globalincs/systemvars.h"
#include "playerman/player.h"
#include "hud/hudparse.h"
#include "network/multi.h"
#include "network/multiutil.h"
#include "iff_defs/iff_defs.h"
#include "parse/parselo.h"


int Show_escort_view;

typedef struct escort_info
{
	int objnum;
	int obj_signature;	// so we are sure we have a valid objnum
	int priority;		// higher priority is higher in the list
	short np_id;			// netplayer id (for multiplayer dogfight mode)
	shield_hit_info hit_info;
} escort_info;

escort_info Escort_ships[MAX_COMPLETE_ESCORT_LIST];
int Num_escort_ships;
int Max_escort_ships = 3;




hud_frames Escort_gauges[NUM_ESCORT_FRAMES];

//int Escort_gauge_y[MAX_ESCORT_SHIPS] = { 219, 230, 241 };

/*
int Escort_gauge_text_coords[GR_NUM_RESOLUTIONS][MAX_ESCORT_SHIPS][4][2] =
{
	{ // GR_640
		{
			{489,219}, 
			{599,212}, 
			{604,219}, 
			{474,219}
		},
		{
			{489,230}, 
			{599,223},
			{604,230},
			{474,230}
		},
		{
			{489,241}, 
			{599,234}, 
			{604,241},
			{474,241} 
		},
	}, 
	{ // GR_1024
		{
			{869,343},
			{973,338}, 
			{981,343}, 
			{854,343} 
		},
		{
			{869,354}, 
			{973,349},
			{981,354},
			{854,354}
		},
		{
			{869,365}, 
			{973,360},
			{981,365},
			{854,365}
		},
	}
};

// escort gauge coords
int Escort_coords[GR_NUM_RESOLUTIONS][4][2] = {
	{ // GR_640
		{486, 206},
		{486, 219},
		{486, 230},
		{486, 241}
	},
	{ // GR_1024
		{865, 330},
		{865, 343},
		{865, 354},
		{865, 365}
	}
};

// monitoring text coords
int Monitoring_coords[GR_NUM_RESOLUTIONS][2] = {
	{ // GR_640
		489, 208
	},
	{ // GR_1024
		869, 331
	}
};
	
char *Escort_gauge_filenames[GR_NUM_RESOLUTIONS][MAX_ESCORT_SHIPS] = 
{
//XSTR:OFF
	{ // GR_640
		"escort1",
		"escort2",
		"escort3"
	}, 
	{ // GR_1024
		"escort1",
		"escort2",
		"escort3"
	}
//XSTR:ON
};*/

static int Last_target_index;	// index into Escort_gauges for last targeted via 'Next Escort Target' key

// data needed from HUDshield.cpp
extern hud_frames Shield_mini_gauge;

// called from HUD init, loads the bitmap data in once, and resets any data for each level
void hud_escort_init()
{
	int i;

	for (i = 0; i < NUM_ESCORT_FRAMES; i++)
	{
		if (strlen(current_hud->Escort_filename[i]))
		{
			Escort_gauges[i].first_frame = bm_load_animation(current_hud->Escort_filename[i], &Escort_gauges[i].
				num_frames);

			if (Escort_gauges[i].first_frame == -1)
			{
				Warning(LOCATION, "Could not load in ani: %s\n", current_hud->Escort_filename[i]);
				return;
			}
		}
		else
		{
			Escort_gauges[i].first_frame = -1;
		}
	}

	Last_target_index = -1;

	if (Max_escort_ships > MAX_COMPLETE_ESCORT_LIST)
	{
		Max_escort_ships = MAX_COMPLETE_ESCORT_LIST;
	}
}

// ----------------------------------------------------------------------
// hud_escort_clear_all()
//
void hud_escort_clear_all()
{
	int i;

	Num_escort_ships = 0;
	for (i = 0; i < Max_escort_ships; i++)
	{
		Escort_ships[i].obj_signature = -99;
		Escort_ships[i].np_id = -1;
		shield_info_reset(&Escort_ships[i].hit_info);
	}
}

// internal helper function for sort.
// sorts first by priority number and then alphabetically
int escort_compare_func(const void* e1, const void* e2)
{
	escort_info* escort1, *escort2;
	int diff;
	int ret;

	escort1 = (escort_info*)e1;
	escort2 = (escort_info*)e2;

	// multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		int n1, n2;

		n1 = find_player_id(escort1->np_id);
		n2 = find_player_id(escort2->np_id);
		if ((n1 < 0) || (n2 < 0) || (Net_players[n1].m_player == NULL) || (Net_players[n2].m_player == NULL))
		{
			ret = 0;
		}
		else
		{
			// player 1 is higher than player 2
			if (Net_players[n1].m_player->stats.m_kill_count_ok >= Net_players[n2].m_player->stats.m_kill_count_ok)
			{
				ret = -1;
			}
			else
			{
				ret = 1;
			}
		}
	}
	else
	{
		diff = escort2->priority - escort1->priority;

		if (diff != 0)
		{
			ret = diff;
		}
		else
		{
			char* name1, *name2;
			name1 = Ships[Objects[escort1->objnum].instance].ship_name;
			name2 = Ships[Objects[escort2->objnum].instance].ship_name;

			ret = stricmp(name1, name2);
		}
	}

	return ret;
}

// create complete priority sorted escort list for all active ships
// escorts - array of escort info
// num_escorts - number of escorts requests in field of active ships
//	  This will be culled to MAX_ESCORTS, selecting the top set from escorts
void hud_create_complete_escort_list(escort_info* escorts, int* num_escorts)
{
	ship_obj* so;
	object* objp;

	// start with none on list
	*num_escorts = 0;

	int idx;

	// multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		for (idx = 0; idx < MAX_PLAYERS; idx++)
		{
			// break out of the loop when we have reached our max
			if (*num_escorts == MAX_COMPLETE_ESCORT_LIST)
			{
				mprintf(("exceeded max ships in big escort list\n"));
				break;
			}

			// is this a valid player			
			if (MULTI_CONNECTED(Net_players[idx]) && !MULTI_OBSERVER(Net_players[idx]) &&
				!MULTI_STANDALONE(Net_players[idx]))
			{
				// add the ship
				escorts[*num_escorts].objnum = -1;
				escorts[*num_escorts].obj_signature = -1;
				escorts[*num_escorts].priority = -1;
				escorts[*num_escorts].np_id = Net_players[idx].player_id;
				(*num_escorts)++;
			}
		}
	}
		// all others 
	else
	{
		for (so = GET_FIRST(&Ship_obj_list); so != END_OF_LIST(&Ship_obj_list); so = GET_NEXT(so))
		{
			Assert(so->objnum >= 0 && so->objnum < MAX_OBJECTS);
			if ((so->objnum < 0) || (so->objnum >= MAX_OBJECTS))
			{
				continue;
			}
			objp = &Objects[so->objnum];
			Assert(objp->type == OBJ_SHIP);
			if (objp->type != OBJ_SHIP)
			{
				continue;
			}

			// break out of the loop when we have reached our max
			if (*num_escorts == MAX_COMPLETE_ESCORT_LIST)
			{
				mprintf(("exceeded max ships in big escort list\n"));
				break;
			}

			// only process ships that might be on the list
			if (!(Ships[objp->instance].flags & SF_ESCORT))
			{
				continue;
			}

			// only process ships that can be seen by sensors
			if ((Ships[objp->instance].flags & SF_HIDDEN_FROM_SENSORS))
			{
				continue;
			}

			// don't process most stealth ships
			if ((Ships[objp->instance].flags2 & SF2_STEALTH))
			{
				if (Ships[objp->instance].team == Player_ship->team)
				{
					// friendly stealths are only not seen when explicitly specified
					if (Ships[objp->instance].flags2 & SF2_FRIENDLY_STEALTH_INVIS)
					{
						continue;
					}
				}
					// non-friendly stealths are never seen
				else
				{
					continue;
				}
			}

			// don't process ships that are dying, or objects that should be dead
			if ((Ships[objp->instance].flags & (SF_DYING | SF_DEPARTING)) || (objp->flags & OF_SHOULD_BE_DEAD))
			{
				continue;
			}

			// add the ship
			escorts[*num_escorts].objnum = so->objnum;
			escorts[*num_escorts].obj_signature = objp->signature;
			escorts[*num_escorts].priority = Ships[objp->instance].escort_priority;
			escorts[*num_escorts].np_id = -1;
			(*num_escorts)++;
		}
	}
}


// ----------------------------------------------------------------------
// hud_init_escort_info()
//
// Set up the escort list
//
void hud_setup_escort_list(int level)
{
	int num_escorts, num_complete_escorts;
	escort_info complete_escorts[MAX_COMPLETE_ESCORT_LIST];

	hud_escort_clear_all();

	// get complete escort list
	hud_create_complete_escort_list(complete_escorts, &num_complete_escorts);

	// sort escort list by priority
	insertion_sort(complete_escorts, num_complete_escorts, sizeof(escort_info), escort_compare_func);

	// set number in escort list
	num_escorts = num_complete_escorts;
	if (num_escorts > Max_escort_ships)
	{
		num_escorts = Max_escort_ships;
	}

	// add ships to escort list
	for (Num_escort_ships = 0; Num_escort_ships < num_escorts; Num_escort_ships++)
	{
		Escort_ships[Num_escort_ships].obj_signature = complete_escorts[Num_escort_ships].obj_signature;
		Escort_ships[Num_escort_ships].priority = complete_escorts[Num_escort_ships].priority;
		Escort_ships[Num_escort_ships].objnum = complete_escorts[Num_escort_ships].objnum;
		Escort_ships[Num_escort_ships].np_id = complete_escorts[Num_escort_ships].np_id;
	}

	if (level)
	{
		Show_escort_view = 1;
	}
}


// combine complete escort list with Escort_ships, keeping valid hit info
void merge_escort_lists(escort_info* complete_escorts, int num_complete_escorts)
{
	int i, j, top_complete_escorts;
	int valid_hit_info[MAX_COMPLETE_ESCORT_LIST];

	// may be > 1 ship change to list (ie, 2 or 3 culled during same frame)
	// set Num_escort_ships and cap
	Num_escort_ships = num_complete_escorts;
	if (Num_escort_ships > Max_escort_ships)
	{
		Num_escort_ships = Max_escort_ships;
	}

	// nothing to do
	if (Num_escort_ships == 0)
	{
		return;
	}

	// check used as a flag whether top slots in complete_escorts were copied
	// this is important re. hit info
	for (i = 0; i < Max_escort_ships; i++)
	{
		valid_hit_info[i] = 0;
	}

	// get the top slots in complete escort list that will be copied onto Escort_ships
	top_complete_escorts = num_complete_escorts;
	if (top_complete_escorts > Max_escort_ships)
	{
		top_complete_escorts = Max_escort_ships;
	}

	// copy for Escort_ships to complete_escorts to retain hit_info
	if (!((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT)))
	{
		for (i = 0; i < top_complete_escorts; i++)
		{
			for (j = 0; j < Num_escort_ships; j++)
			{
				if (Escort_ships[j].obj_signature == complete_escorts[i].obj_signature)
				{
					complete_escorts[i] = Escort_ships[j];
					valid_hit_info[i] = 1;
					break;
				}
			}
		}

		// copy top slots to Escort_ships
		for (i = 0; i < top_complete_escorts; i++)
		{
			Escort_ships[i] = complete_escorts[i];
			// check all ships are valid
			int objnum = Escort_ships[i].objnum;
			Assert(objnum >= 0 && objnum < MAX_OBJECTS);
			if ((objnum < 0) || (objnum >= MAX_OBJECTS))
			{
				continue;
			}
			if (!valid_hit_info[i])
			{
				shield_info_reset(&Escort_ships[i].hit_info);
			}
		}
	}

	// reset Num_escort_ships
	Num_escort_ships = top_complete_escorts;
}


// ----------------------------------------------------------------------
// hud_remove_ship_from_escort_index()
//
// Take a ship out of the escort list
void hud_remove_ship_from_escort_index(int dead_index, int objnum)
{
	int i, count, num_complete_escorts;
	escort_info bakup_arr[MAX_COMPLETE_ESCORT_LIST], complete_escorts[MAX_COMPLETE_ESCORT_LIST];

	// remove him from escort list
	if ((objnum >= 0) && (Objects[objnum].type == OBJ_SHIP) && (Objects[objnum].instance >= 0))
	{
		Ships[Objects[objnum].instance].flags &= ~SF_ESCORT;
	}

	count = 0;
	for (i = 0; i < Num_escort_ships; i++)
	{
		if (i != dead_index)
		{
			bakup_arr[count++] = Escort_ships[i];
		}
	}

	for (i = 0; i < count; i++)
	{
		Escort_ships[i] = bakup_arr[i];
	}

	Num_escort_ships--;
	Assert(Num_escort_ships >= 0);

	// get complete escort list
	hud_create_complete_escort_list(complete_escorts, &num_complete_escorts);

	// sort escort list by priority
	insertion_sort(complete_escorts, num_complete_escorts, sizeof(escort_info), escort_compare_func);

	// merge list
	merge_escort_lists(complete_escorts, num_complete_escorts);

	hud_gauge_popup_start(HUD_ESCORT_VIEW);

}

// called once per frame to remove dead or departed ships from the escort list
void hud_escort_cull_list()
{
	int i, objnum;

	int np_index;

	// multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		for (i = 0; i < Num_escort_ships; i++)
		{
			np_index = find_player_id(Escort_ships[i].np_id);

			// maybe remove him if he left
			if (np_index < 0)
			{
				hud_remove_ship_from_escort_index(i, -1);
				i--;
			}
		}
	}
		// everything else
	else
	{
		for (i = 0; i < Num_escort_ships; i++)
		{
			objnum = Escort_ships[i].objnum;
			Assert(objnum >= 0 && objnum < MAX_OBJECTS);
			if (Objects[objnum].flags & OF_SHOULD_BE_DEAD || Ships[Objects[objnum].instance].flags &
				SF_HIDDEN_FROM_SENSORS)
			{
				hud_remove_ship_from_escort_index(i, objnum);
				i--;
			}
		}
	}
}

// Set the color for the text to be displayed
// Goober5000 - changed second parameter from "friendly" (true/false) to "team";
// also changed from hud color or red to team color
int hud_escort_set_gauge_color(int index, int team)
{
	int is_flashing = 0;
	int is_bright = 0;
	int seen_from_team = (Player_ship != NULL) ? Player_ship->team : -1;
	shield_hit_info* shi;

	shi = &Escort_ships[index].hit_info;

	// multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		hud_set_gauge_color(HUD_ESCORT_VIEW);
		return 0;
	}

	// set flashing color
	if (!timestamp_elapsed(shi->shield_hit_timers[HULL_HIT_OFFSET]))
	{
		if (timestamp_elapsed(shi->shield_hit_next_flash[HULL_HIT_OFFSET]))
		{
			shi->shield_hit_next_flash[HULL_HIT_OFFSET] = timestamp(SHIELD_FLASH_INTERVAL);
			shi->shield_show_bright ^= (1 << HULL_HIT_OFFSET);	// toggle between default and bright frames
		}

		is_flashing = 1;
		if (shi->shield_show_bright & (1 << HULL_HIT_OFFSET))
		{
			is_bright = 1;
		}
	}


	// Goober5000 - now base this on team color
	gr_set_color_fast(iff_get_color_by_team_and_object(team, seen_from_team,
		is_bright, &Objects[Escort_ships[index].objnum]));


	// Goober5000 - an alternative; same as original but incorporating teams for non-friendlies
	/*
	if ((seen_from_team == team) || (seen_from_team < 0))	// :V: sez assume friendly if Player_ship is NULL
		hud_set_gauge_color(HUD_ESCORT_VIEW, is_bright ? HUD_C_BRIGHT : HUD_C_DIM);
	else
		gr_set_color_fast(iff_get_color_by_team(team, seen_from_team, is_bright));
	*/


	// Goober5000 - original color logic
	/*
	if ((seen_from_team == team) || (seen_from_team < 0))	// :V: sez assume friendly if Player_ship is NULL
		hud_set_gauge_color(HUD_ESCORT_VIEW, is_bright ? HUD_C_BRIGHT : HUD_C_DIM);
	else
		gr_set_color_fast(is_bright ? &Color_bright_red : &Color_red);
	*/


	return is_flashing;
}

// multiplayer dogfight
void hud_escort_show_icon_dogfight(int x, int y, int index)
{
	int hull_integrity = 100;
	char buf[255];
	int np_index;
	object* objp;
	ship_info* sip;

	int stat_shift = 40;

	// always use the standard color to avoid confusion
	hud_set_gauge_color(HUD_ESCORT_VIEW);

	// netplayer index
	np_index = find_player_id(Escort_ships[index].np_id);
	if ((np_index < 0) || (np_index >= MAX_PLAYERS) || (Net_players[np_index].m_player == NULL))
	{
		return;
	}

	// print out player name
	strcpy_s(buf, Net_players[np_index].m_player->callsign);
	gr_force_fit_string(buf, 255, 100 - stat_shift);
	emp_hud_string(x + current_hud->Escort_name[0], y + current_hud->Escort_name[1], EG_ESCORT1 + index, buf);

	// can we get the player object?
	objp = NULL;
	if ((Net_players[np_index].m_player->objnum >= 0) && (Net_players[np_index].m_player->objnum < MAX_OBJECTS) &&
		(Objects[Net_players[np_index].m_player->objnum].type == OBJ_SHIP))
	{
		objp = &Objects[Net_players[np_index].m_player->objnum];
		if ((objp->instance >= 0) && (objp->instance < MAX_SHIPS) && (Ships[objp->instance].ship_info_index >= 0) &&
			(Ships[objp->instance].ship_info_index < MAX_SHIPS))
		{
			sip = &Ship_info[Ships[objp->instance].ship_info_index];
		}
		else
		{
			return;
		}

		hull_integrity = (int)(((float)objp->hull_strength / (float)Ships[objp->instance].ship_max_hull_strength) *
							   100.0f);
		if (hull_integrity < 0)
		{
			hull_integrity = 0;
		}
	}

	// show ship integrity
	if (objp == NULL)
	{
		emp_hud_printf(x + current_hud->Escort_integrity[0] - stat_shift, y + current_hud->Escort_integrity[1],
			EG_NULL, "%d", Net_players[np_index].m_player->stats.m_kill_count_ok);
	}
	else
	{
		emp_hud_printf(x + current_hud->Escort_integrity[0] - stat_shift, y + current_hud->Escort_integrity[1],
			EG_NULL, "(%d%%) %d", hull_integrity, Net_players[np_index].m_player->stats.m_kill_count_ok);
	}
}

// draw the shield icon and integrity for the escort ship
void hud_escort_show_icon(int x, int y, int index)
{
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT) && index <= 2)
	{
		hud_escort_show_icon_dogfight(x, y, index);
		return;
	}

	float shields, integrity;
	int screen_integrity, offset;
	char buf[255];

	object* objp = &Objects[Escort_ships[index].objnum];
	ship* sp = &Ships[objp->instance];

	// determine if its "friendly" or not	
	// Goober5000 - changed in favor of just passing the team
	hud_escort_set_gauge_color(index, sp->team);
	/*
	if(Player_ship != NULL){
		hud_escort_set_gauge_color(index, (sp->team == Player_ship->team) ? 1 : 0);
	} else {
		hud_escort_set_gauge_color(index, 1);
	}
	*/

	// draw a 'D' if a ship is disabled
	if ((sp->flags & SF_DISABLED) || (ship_subsys_disrupted(sp, SUBSYSTEM_ENGINE)))
	{
		emp_hud_string(x + current_hud->Escort_status[0], y + current_hud->Escort_status[1], EG_NULL, XSTR("D", 284));
	}

	// print out ship name
	if (sp->callsign_index >= 0)
	{
		hud_stuff_ship_callsign(buf, sp);
	}
	else if (((Iff_info[sp->team].flags & IFFF_WING_NAME_HIDDEN) && (sp->wingnum != -1)) || (sp->flags2 &
																							 SF2_HIDE_SHIP_NAME))
	{
		hud_stuff_ship_class(buf, sp);
	}
	else
	{
		hud_stuff_ship_name(buf, sp);
	}

	gr_force_fit_string(buf, 255, 100);

	emp_hud_string(x + current_hud->Escort_name[0], y + current_hud->Escort_name[1], EG_ESCORT1 + index, buf);

	// show ship integrity
	hud_get_target_strength(objp, &shields, &integrity);
	screen_integrity = fl2i(integrity * 100 + 0.5f);
	offset = 0;
	if (screen_integrity < 100)
	{
		offset = 2;
		if (screen_integrity == 0)
		{
			if (integrity > 0)
			{
				screen_integrity = 1;
			}
		}
	}
	emp_hud_printf(x + current_hud->Escort_integrity[0] + offset, y + current_hud->Escort_integrity[1], EG_NULL, "%d",
		screen_integrity);

	//Let's be nice.
	hud_set_gauge_color(HUD_ESCORT_VIEW);
}


// ----------------------------------------------------------------------
// hud_display_escort()
//
// Display the data on ships in the escort list
void hud_display_escort()
{
	int i = 0;

	if (!Show_escort_view)
	{
		return;
	}

	if (!Num_escort_ships)
	{
		return;
	}

	// hud_set_default_color();
	hud_set_gauge_color(HUD_ESCORT_VIEW);

	// draw the top of the escort view
	hud_aabitmap(Escort_gauges[0].first_frame, current_hud->Escort_coords[0], current_hud->Escort_coords[1]);
	gr_string(current_hud->Escort_htext_coords[0], current_hud->Escort_htext_coords[1], current_hud->Escort_htext);

	int x = current_hud->Escort_coords[0] + current_hud->Escort_list[0];
	int y = current_hud->Escort_coords[1] + current_hud->Escort_list[1];

	//This is temporary
	Num_escort_ships--;
	i = 0;

	if (Num_escort_ships)
	{
		for (; i < Num_escort_ships; i++)
		{
			if (i != 0)
			{
				x += current_hud->Escort_entry[0];
				y += current_hud->Escort_entry[1];
			}
			hud_aabitmap(Escort_gauges[1].first_frame, x, y);

			//Now we just show the ships info
			hud_escort_show_icon(x, y, i);
		}

		//Increment for last entry
		x += current_hud->Escort_entry_last[0];
		y += current_hud->Escort_entry_last[1];
	}

	//Back to right #
	Num_escort_ships++;

	//Show the last escort entry
	hud_aabitmap(Escort_gauges[2].first_frame, x, y);
	hud_escort_show_icon(x, y, i);
}

// ----------------------------------------------------------------------
// hud_escort_view_toggle()
//
void hud_escort_view_toggle()
{
	Show_escort_view ^= 1;
	if (Show_escort_view)
	{
		HUD_sourced_printf(HUD_SOURCE_HIDDEN, XSTR("Escort view enabled", 286));
	}
	else
	{
		HUD_sourced_printf(HUD_SOURCE_HIDDEN, XSTR("Escort view disabled", 287));
	}
}

// try to add a ship to the escort list, if slot available
void hud_add_ship_to_escort(int objnum, int supress_feedback)
{
	escort_info complete_escorts[MAX_COMPLETE_ESCORT_LIST];
	int num_complete_escorts, idx, found;

	// get complete escort list
	hud_create_complete_escort_list(complete_escorts, &num_complete_escorts);

	// ensure the complete escort list is not full already
	if (num_complete_escorts == MAX_COMPLETE_ESCORT_LIST)
	{
		return;
	}

	// check if ship is already on complete escort list
	found = 0;
	for (idx = 0; idx < num_complete_escorts; idx++)
	{
		if (complete_escorts[idx].obj_signature == Objects[objnum].signature)
		{
			found = 1;
			break;
		}
	}

	// add new ship into complete list
	if (!found)
	{
		complete_escorts[num_complete_escorts].objnum = objnum;
		complete_escorts[num_complete_escorts].obj_signature = Objects[objnum].signature;
		complete_escorts[num_complete_escorts].priority = Ships[Objects[objnum].instance].escort_priority;

		// remove him from escort list
		Ships[Objects[objnum].instance].flags |= SF_ESCORT;

		num_complete_escorts++;
	}

	// sort escort list by priority
	insertion_sort(complete_escorts, num_complete_escorts, sizeof(escort_info), escort_compare_func);

	// merge list
	merge_escort_lists(complete_escorts, num_complete_escorts);

	// maybe do feedback
	if ((Num_escort_ships == Max_escort_ships) && !supress_feedback)
	{
		found = 0;
		// search thru list for objnum
		for (idx = 0; idx < Num_escort_ships; idx++)
		{
			if (Escort_ships[idx].objnum == objnum)
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			HUD_sourced_printf(HUD_SOURCE_HIDDEN, XSTR("Escort list is full with %d ships", 288), Num_escort_ships);
			snd_play(&Snds[SND_TARGET_FAIL]);
		}
	}

	hud_gauge_popup_start(HUD_ESCORT_VIEW);
}


// ----------------------------------------------------------------------
// hud_add_remove_ship_escort()
//
void hud_add_remove_ship_escort(int objnum, int supress_feedback)
{
	int in_escort, i;

	// no ships on the escort list in multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		return;
	}

	if (objnum < 0)
	{
		Int3();
		return;
	}

	if (Objects[objnum].type != OBJ_SHIP)
	{
		if (!supress_feedback)
		{
			snd_play(&Snds[SND_TARGET_FAIL]);
		}
		return;
	}

	in_escort = 0;
	for (i = 0; i < Num_escort_ships; i++)
	{
		if (Escort_ships[i].obj_signature == Objects[objnum].signature)
		{
			in_escort = 1;
			break;
		}
	}

	if (in_escort)
	{
		hud_remove_ship_from_escort_index(i, objnum);
		return;
	}

	hud_add_ship_to_escort(objnum, supress_feedback);
}

void hud_remove_ship_from_escort(int objnum)
{
	int in_escort, i;

	// no ships on the escort list in multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		return;
	}

	if (objnum < 0)
	{
		Int3();
		return;
	}

	in_escort = 0;
	for (i = 0; i < Num_escort_ships; i++)
	{
		if (Escort_ships[i].obj_signature == Objects[objnum].signature)
		{
			in_escort = 1;
			break;
		}
	}

	if (in_escort)
	{
		hud_remove_ship_from_escort_index(i, objnum);
		return;
	}
}

// Called whenever a ship is hit to determine if that ship is in the escort list.  If it
// is, then start timers to flash the name hull/shield icon for that ship.
void hud_escort_ship_hit(object* objp, int quadrant)
{
	int num, i;
	shield_hit_info* shi;

	// no ships on the escort list in multiplayer dogfight
	if ((Game_mode & GM_MULTIPLAYER) && (Netgame.type_flags & NG_TYPE_DOGFIGHT))
	{
		return;
	}

	for (i = 0; i < Num_escort_ships; i++)
	{
		if (Escort_ships[i].objnum == OBJ_INDEX(objp))
		{
			shi = &Escort_ships[i].hit_info;
			num = Quadrant_xlate[quadrant];
			hud_gauge_popup_start(HUD_ESCORT_VIEW);
			if (quadrant >= 0)
			{
				shi->shield_hit_timers[num] = timestamp(SHIELD_HIT_DURATION);
			}
			else
			{
				shi->shield_hit_timers[HULL_HIT_OFFSET] = timestamp(SHIELD_HIT_DURATION);
			}
		}
	}
}

// target the next ship in the escort list
void hud_escort_target_next()
{
	int objnum;

	if (Num_escort_ships == 0)
	{
		snd_play(&Snds[SND_TARGET_FAIL], 0.0f);
		return;
	}

	Last_target_index++;
	if (Last_target_index >= Num_escort_ships)
	{
		Last_target_index = 0;
	}

	objnum = Escort_ships[Last_target_index].objnum;
	set_target_objnum(Player_ai, objnum);
	hud_restore_subsystem_target(&Ships[Objects[objnum].instance]);
}

// return the number of ships currently on the escort list
int hud_escort_num_ships_on_list()
{
	return Num_escort_ships;
}

// Return the object number for the ship at index position in the escort list
int hud_escort_return_objnum(int index)
{
	int escort_objnum, escort_sig;
	if (index >= Num_escort_ships)
	{
		return -1;
	}

	escort_objnum = Escort_ships[index].objnum;
	escort_sig = Escort_ships[index].obj_signature;

	if (escort_objnum < 0)
	{
		return -1;
	}

	// ensure this is still a valid index
	if (Objects[escort_objnum].signature != escort_sig)
	{
		return -1;
	}

	return Escort_ships[index].objnum;
}


void hudescort_page_in()
{
	int i;

	for (i = 0; i < NUM_ESCORT_FRAMES; i++)
	{
		bm_page_in_aabitmap(Escort_gauges[i].first_frame, Escort_gauges[i].num_frames);
	}
}

void hud_escort_add_player(short id)
{
	Assert(Game_mode & GM_MULTIPLAYER);
	if (!(Game_mode & GM_MULTIPLAYER))
	{
		return;
	}

	int idx;

	// just go through and add as long as its not a duplicate
	for (idx = 0; idx < Num_escort_ships; idx++)
	{
		if (Escort_ships[idx].np_id == id)
		{
			return;
		}
	}

	// re-setup the escort list
	hud_setup_escort_list(0);
}

void hud_escort_remove_player(short id)
{
	Assert(Game_mode & GM_MULTIPLAYER);
	if (!(Game_mode & GM_MULTIPLAYER))
	{
		return;
	}

	int idx;

	// find the instance and remove it if possible
	for (idx = 0; idx < Num_escort_ships; idx++)
	{
		if (Escort_ships[idx].np_id == id)
		{
			hud_remove_ship_from_escort_index(idx, -1);
			return;
		}
	}
}
