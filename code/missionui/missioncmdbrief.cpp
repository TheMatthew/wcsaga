/*
 * Copyright (C) Volition, Inc. 1999.  All rights reserved.
 *
 * All source code herein is the property of Volition, Inc. You may not sell 
 * or otherwise commercially exploit the source or things you created based on the 
 * source.
 *
*/



#include "missionui/missioncmdbrief.h"
#include "missionui/missionscreencommon.h"
#include "ui/uidefs.h"
#include "gamesnd/gamesnd.h"
#include "gamesequence/gamesequence.h"
#include "io/key.h"
#include "graphics/font.h"
#include "mission/missionbriefcommon.h"
#include "missionui/redalert.h"
#include "sound/audiostr.h"
#include "io/timer.h"
#include "gamesnd/eventmusic.h"
#include "playerman/player.h"
#include "gamehelp/contexthelp.h"
#include "globalincs/alphacolors.h"
#include "anim/packunpack.h"
#include "anim/animplay.h"
#include "sound/fsspeech.h"



#define NUM_CMD_SETTINGS	2

char* Cmd_brief_fname[NUM_CMD_SETTINGS][GR_NUM_RESOLUTIONS] =
{
	{
		"CommandBrief",
		"2_CommandBrief"
	},
	{
		"CommandBriefb",
		"2_CommandBriefb"
	}
};


char* Cmd_brief_mask[NUM_CMD_SETTINGS][GR_NUM_RESOLUTIONS] =
{
	{
		"CommandBrief-m",
		"2_CommandBrief-m"
	},
	{
		"CommandBrief-mb",
		"2_CommandBrief-mb"
	},
};


// lookups for coordinates
#define CMD_X_COORD 0
#define CMD_Y_COORD 1
#define CMD_W_COORD 2
#define CMD_H_COORD 3

int Cmd_text_wnd_coords[NUM_CMD_SETTINGS][GR_NUM_RESOLUTIONS][4] =
{
	// original
	{
		{
			17,
			109,
			606,
			108			// GR_640
		},
		{
			28,
			174,
			969,
			174			// GR_1024
		}
	},
	// buttons
	{
		{
			17,
			109,
			587,
			108			// GR_640
		},
		{
			28,
			174,
			939,
			174			// GR_1024
		}
	}
};

int Cmd_stage_y[GR_NUM_RESOLUTIONS] =
{
	90,		// GR_640
	145		// GR_1024
};

/*
int Cmd_image_wnd_coords[GR_NUM_RESOLUTIONS][4] =
{
	{
		26, 258, 441, 204		// GR_640
	},
	{
		155, 475, 706, 327		// GR_1024
	}
};
*/

// Goober5000 - center coordinates only
int Cmd_image_center_coords[GR_NUM_RESOLUTIONS][2] =
{
	{
		246,
		358				// GR_640
	},
	{
		394,
		573				// GR_1024
	}
};

int Top_cmd_brief_text_line;
int Cmd_brief_text_max_lines[GR_NUM_RESOLUTIONS] =
{
	10,
	17
};

#define MAX_CMD_BRIEF_BUTTONS	10
#define MIN_CMD_BRIEF_BUTTONS	8
#define NUM_CMD_BRIEF_BUTTONS	(Uses_scroll_buttons ? MAX_CMD_BRIEF_BUTTONS : MIN_CMD_BRIEF_BUTTONS)

#define CMD_BRIEF_BUTTON_FIRST_STAGE	0
#define CMD_BRIEF_BUTTON_PREV_STAGE		1
#define CMD_BRIEF_BUTTON_PAUSE			2
#define CMD_BRIEF_BUTTON_NEXT_STAGE		3
#define CMD_BRIEF_BUTTON_LAST_STAGE		4
#define CMD_BRIEF_BUTTON_HELP			5
#define CMD_BRIEF_BUTTON_OPTIONS		6
#define CMD_BRIEF_BUTTON_ACCEPT			7
#define CMD_BRIEF_BUTTON_SCROLL_UP		8
#define CMD_BRIEF_BUTTON_SCROLL_DOWN	9

// buttons
ui_button_info Cmd_brief_buttons[GR_NUM_RESOLUTIONS][MAX_CMD_BRIEF_BUTTONS] =
{
	{ // GR_640
		ui_button_info("CBB_00", 504, 221, -1, -1, 0),
		ui_button_info("CBB_01", 527, 221, -1, -1, 1),
		ui_button_info("CBB_02", 555, 221, -1, -1, 2),
		ui_button_info("CBB_03", 583, 221, -1, -1, 3),
		ui_button_info("CBB_04", 607, 221, -1, -1, 4),
		ui_button_info("CBB_05", 539, 431, -1, -1, 5),
		ui_button_info("CBB_06", 538, 455, -1, -1, 6),
		ui_button_info("CBB_07", 575, 432, -1, -1, 7),
		ui_button_info("CBB_08", 615, 144, -1, -1, 8),
		ui_button_info("CBB_09", 615, 186, -1, -1, 9),
	},
	{ // GR_1024
		ui_button_info("2_CBB_00", 806, 354, -1, -1, 0),
		ui_button_info("2_CBB_01", 844, 354, -1, -1, 1),
		ui_button_info("2_CBB_02", 888, 354, -1, -1, 2),
		ui_button_info("2_CBB_03", 933, 354, -1, -1, 3),
		ui_button_info("2_CBB_04", 971, 354, -1, -1, 4),
		ui_button_info("2_CBB_05", 854, 681, -1, -1, 5),
		ui_button_info("2_CBB_06", 861, 728, -1, -1, 6),
		ui_button_info("2_CBB_07", 920, 692, -1, -1, 7),
		ui_button_info("2_CBB_08", 985, 232, -1, -1, 8),
		ui_button_info("2_CBB_09", 985, 299, -1, -1, 9),
	}
};

// text
#define CMD_BRIEF_NUM_TEXT		3
UI_XSTR Cmd_brief_text[GR_NUM_RESOLUTIONS][CMD_BRIEF_NUM_TEXT] =
{
	{ // GR_640
		{
			"Help",
			928,
			500,
			440,
			UI_XSTR_COLOR_PINK,
			-1,
			&Cmd_brief_buttons[0][CMD_BRIEF_BUTTON_HELP].button
		},
		{
			"Options",
			1036,
			479,
			464,
			UI_XSTR_COLOR_PINK,
			-1,
			&Cmd_brief_buttons[0][CMD_BRIEF_BUTTON_OPTIONS].button
		},
		{
			"Continue",
			1069,
			564,
			413,
			UI_XSTR_COLOR_PINK,
			-1,
			&Cmd_brief_buttons[0][CMD_BRIEF_BUTTON_ACCEPT].button
		},
	},
	{ // GR_1024
		{
			"Help",
			928,
			822,
			704,
			UI_XSTR_COLOR_PINK,
			-1,
			&Cmd_brief_buttons[1][CMD_BRIEF_BUTTON_HELP].button
		},
		{
			"Options",
			1036,
			800,
			743,
			UI_XSTR_COLOR_PINK,
			-1,
			&Cmd_brief_buttons[1][CMD_BRIEF_BUTTON_OPTIONS].button
		},
		{
			"Continue",
			1069,
			941,
			671,
			UI_XSTR_COLOR_PINK,
			-1,
			&Cmd_brief_buttons[1][CMD_BRIEF_BUTTON_ACCEPT].button
		},
	}
};

static UI_WINDOW Ui_window;
static int Cmd_brief_background_bitmap;					// bitmap for the background of the cmd_briefing
static int Cur_stage;
static int Cmd_brief_inited = 0;
// static int Cmd_brief_ask_for_cd;
static int Voice_good_to_go = 0;
static int Voice_started_time = 0;
static int Voice_ended_time;
//static int Anim_playing_id = -1;
//static anim_instance *Cur_anim_instance = NULL;
static generic_anim Cur_Anim;
static char* Cur_anim_filename = "~~~~";
static int anim_done = 0;

static int Last_anim_frame_num;

static int Cmd_brief_last_voice;
static int Cmd_brief_paused = 0;
//static int Palette_bmp = -1;
static ubyte Palette[768];
//static char Palette_name[128];

static int Uses_scroll_buttons = 0;

void cmd_brief_init_voice()
{
	int i;

	Assert(Cur_cmd_brief);
	for (i = 0; i < Cur_cmd_brief->num_stages; i++)
	{
		Cur_cmd_brief->stage[i].wave = -1;
		if (stricmp(Cur_cmd_brief->stage[i].wave_filename, NOX("none")) && Cur_cmd_brief->stage[i].wave_filename[0])
		{
			Cur_cmd_brief->stage[i].wave = audiostream_open(Cur_cmd_brief->stage[i].wave_filename, ASF_VOICE);
			if (Cur_cmd_brief->stage[i].wave < 0)
			{
				nprintf(("General", "Failed to load \"%s\"\n", Cur_cmd_brief->stage[i].wave_filename));
			}
		}
	}

	Cmd_brief_last_voice = -1;
}

int cmd_brief_check_stage_done()
{
	if (!Voice_good_to_go)
		return 0;

	if (Cmd_brief_paused)
		return 0;

	if (Voice_ended_time && (timer_get_milliseconds() - Voice_ended_time >= 1000))
		return 1;

	if (Briefing_voice_enabled && (Cmd_brief_last_voice >= 0))
	{
		if (audiostream_is_playing(Cmd_brief_last_voice))
		{
			return 0;
		}

		if (!Voice_ended_time)
		{
			Voice_ended_time = timer_get_milliseconds();
		}

		return 0;
	}

	// if we get here, there is no voice, so we simulate the time it would take instead
	if (!Voice_ended_time)
		Voice_ended_time = Voice_started_time + MAX(5000, Num_brief_text_lines[0] * 3500);

	return 0;
}

// start playback of the voice for a particular briefing stage
void cmd_brief_voice_play(int stage_num)
{
	int voice = -1;

	if (!Voice_good_to_go)
	{
		Voice_started_time = 0;
		return;
	}

	if (!Voice_started_time)
	{
		Voice_started_time = timer_get_milliseconds();
		Voice_ended_time = 0;
	}

	if (!Briefing_voice_enabled)
	{
		return;
	}

	if (Cur_stage >= 0 && Cur_stage < Cur_cmd_brief->num_stages)
	{
		voice = Cur_cmd_brief->stage[stage_num].wave;
	}

	// are we still on same voice that is currently playing/played?
	if (Cmd_brief_last_voice == voice)
	{
		return;  // no changes, nothing to do.
	}

	// if previous wave is still playing, stop it first.
	if (Cmd_brief_last_voice >= 0)
	{
		audiostream_stop(Cmd_brief_last_voice, 1, 0);  // stream is automatically rewound
		Cmd_brief_last_voice = -1;
	}

	// ok, new wave needs playing, so we can start playing it now (and it becomes the current wave)
	Cmd_brief_last_voice = voice;
	if (voice >= 0)
	{
		audiostream_play(voice, Master_voice_volume, 0);
	}
}

// called to leave the command briefing screen
void cmd_brief_exit()
{
	// I know, going to red alert from cmd brief is stupid, but we have stupid fredders
	if (red_alert_mission())
	{
		gameseq_post_event(GS_EVENT_RED_ALERT);
	}
	else
	{
		gameseq_post_event(GS_EVENT_START_BRIEFING);
	}
}

//doesn't actually stop playing ANIs any more, just stops audio
void cmd_brief_stop_anim(int id)
{
	/*
	if (Cur_anim_instance && (id != Anim_playing_id)) {
		anim_stop_playing(Cur_anim_instance);
		Cur_anim_instance = NULL;
	}
	*/

	Voice_good_to_go = 0;
	if (Cmd_brief_last_voice >= 0)
	{
		audiostream_stop(Cmd_brief_last_voice, 1, 0);  // stream is automatically rewound
		Cmd_brief_last_voice = -1;
	}
}

void cmd_brief_new_stage(int stage)
{
	char* p;
	char filename[NAME_LENGTH];

	if (stage < 0)
	{
		cmd_brief_stop_anim(-1);
		Cur_stage = -1;
	}

	// If the briefing has no wave to play use simulated speach
	if (Cur_cmd_brief->stage[stage].wave <= 0)
	{
		fsspeech_play(FSSPEECH_FROM_BRIEFING, Cur_cmd_brief->stage[stage].text);
	}

	Cur_stage = stage;
	brief_color_text_init(Cur_cmd_brief->stage[stage].text, Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.
		res][CMD_W_COORD]);

	// load a new animation if it's different to what's already playing
	if (strcmp(Cur_anim_filename, Cur_cmd_brief->stage[stage].ani_filename) != 0)
	{
		// set stuff up
		int stream_result = -1;
		ubyte bg_type = bm_get_type(Cmd_brief_background_bitmap);
		anim_done = 0;

		// unload the previous anim
		if (Cur_Anim.num_frames > 0)
		{
			generic_anim_unload(&Cur_Anim);
		}

		// save new filename
		Cur_anim_filename = Cur_cmd_brief->stage[stage].ani_filename;

		// hi-res support
		if (gr_screen.res == GR_1024)
		{
			// attempt to load a hi-res animation
			memset(filename, 0, NAME_LENGTH);
			strcpy_s(filename, "2_");
			strncat(filename, Cur_anim_filename, NAME_LENGTH - 3);

			// remove extension
			p = strchr(filename, '.');
			if (p)
			{
				*p = '\0';
			}

			// attempt to stream the hi-res ani
			generic_anim_init(&Cur_Anim, filename);
			Cur_Anim.ani.bg_type = bg_type;
			stream_result = generic_anim_stream(&Cur_Anim);
		}

		// we failed to stream hi-res, or we aren't running in hi-res, so try low-res
		if (stream_result < 0)
		{
			strcpy_s(filename, Cur_anim_filename);

			// remove extension
			p = strchr(filename, '.');
			if (p)
			{
				*p = '\0';
			}

			// attempt to stream the low-res ani
			generic_anim_init(&Cur_Anim, filename);
			Cur_Anim.ani.bg_type = bg_type;
			stream_result = generic_anim_stream(&Cur_Anim);
		}

		// we've failed to load any animation
		if (stream_result < 0)
		{
			// load an image and treat it like a 1 frame animation
			Cur_Anim.first_frame = bm_load(Cur_cmd_brief->stage[stage].ani_filename);	//if we fail here, the value is still -1
			if (Cur_Anim.first_frame != -1)
			{
				Cur_Anim.num_frames = 1;
			}
		}
	}

	//resetting the audio here
	cmd_brief_stop_anim(-1);

	Top_cmd_brief_text_line = 0;
}

void cmd_brief_hold()
{
	cmd_brief_stop_anim(-1);
	//Anim_playing_id = -1;
}

void cmd_brief_unhold()
{
	cmd_brief_new_stage(Cur_stage);
}

extern int Briefing_music_handle;

void cmd_brief_pause()
{
	if (Cmd_brief_paused)
		return;

	Cmd_brief_paused = 1;

	if (Cmd_brief_last_voice >= 0)
	{
		audiostream_pause(Cmd_brief_last_voice);
	}

	if (Briefing_music_handle >= 0)
	{
		audiostream_pause(Briefing_music_handle);
	}

	fsspeech_pause(true);
}

void cmd_brief_unpause()
{
	if (!Cmd_brief_paused)
		return;

	Cmd_brief_paused = 0;

	if (Cmd_brief_last_voice >= 0)
	{
		audiostream_unpause(Cmd_brief_last_voice);
	}

	if (Briefing_music_handle >= 0)
	{
		audiostream_unpause(Briefing_music_handle);
	}

	fsspeech_pause(false);
}

void cmd_brief_button_pressed(int n)
{
	switch (n)
	{
	case CMD_BRIEF_BUTTON_HELP:
		launch_context_help();
		gamesnd_play_iface(SND_HELP_PRESSED);
		break;

	case CMD_BRIEF_BUTTON_OPTIONS:
		gamesnd_play_iface(SND_SWITCH_SCREENS);
		gameseq_post_event(GS_EVENT_OPTIONS_MENU);
		break;

	case CMD_BRIEF_BUTTON_FIRST_STAGE:
		if (Cur_stage)
		{
			cmd_brief_new_stage(0);
			gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
		}
		else
		{
			gamesnd_play_iface(SND_GENERAL_FAIL);
		}

		break;

	case CMD_BRIEF_BUTTON_PREV_STAGE:
		if (Cur_stage)
		{
			cmd_brief_new_stage(Cur_stage - 1);
			gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
		}
		else
		{
			gamesnd_play_iface(SND_GENERAL_FAIL);
		}

		break;

	case CMD_BRIEF_BUTTON_NEXT_STAGE:
		if (Cur_stage < Cur_cmd_brief->num_stages - 1)
		{
			cmd_brief_new_stage(Cur_stage + 1);
			gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
		}
		else
		{
			gamesnd_play_iface(SND_GENERAL_FAIL);
		}

		break;

	case CMD_BRIEF_BUTTON_LAST_STAGE:
		if (Cur_stage < Cur_cmd_brief->num_stages - 1)
		{
			cmd_brief_new_stage(Cur_cmd_brief->num_stages - 1);
			gamesnd_play_iface(SND_BRIEF_STAGE_CHG);
		}
		else
		{
			gamesnd_play_iface(SND_GENERAL_FAIL);
		}
		break;

	case CMD_BRIEF_BUTTON_ACCEPT:
		cmd_brief_exit();
		gamesnd_play_iface(SND_COMMIT_PRESSED);
		break;

	case CMD_BRIEF_BUTTON_PAUSE:
		gamesnd_play_iface(SND_USER_SELECT);
		fsspeech_pause(Player->auto_advance != 0);
		Player->auto_advance ^= 1;
		break;

	case CMD_BRIEF_BUTTON_SCROLL_UP:
		Top_cmd_brief_text_line--;
		if (Top_cmd_brief_text_line < 0)
		{
			Top_cmd_brief_text_line = 0;
			gamesnd_play_iface(SND_GENERAL_FAIL);
		}
		else
		{
			gamesnd_play_iface(SND_SCROLL);
		}
		break;

	case CMD_BRIEF_BUTTON_SCROLL_DOWN:
		Top_cmd_brief_text_line++;
		if ((Num_brief_text_lines[0] - Top_cmd_brief_text_line) < Cmd_brief_text_max_lines[gr_screen.res])
		{
			Top_cmd_brief_text_line--;
			gamesnd_play_iface(SND_GENERAL_FAIL);
		}
		else
		{
			gamesnd_play_iface(SND_SCROLL);
		}
		break;
	}
}

void cmd_brief_ani_wave_init(int index)
{
	char* name;

	// this is the first instance of the given anim filename
	name = Cur_cmd_brief->stage[index].ani_filename;
	if (!name[0] || !stricmp(name, NOX("<default>")) || !stricmp(name, NOX("none.ani")))
	{
		name = NOX("CB_default");
		strcpy_s(Cur_cmd_brief->stage[index].ani_filename, name);
	}
}

void cmd_brief_init(int team)
{
	common_music_init(SCORE_CMD_BRIEFING);

	//#ifndef FS2_DEMO

	int i;
	ui_button_info* b;

	Cmd_brief_inited = 0;
	Cur_cmd_brief = &Cmd_briefs[team];

	// Goober5000 - replace any variables (probably persistent variables) with their values
	for (i = 0; i < Cur_cmd_brief->num_stages; i++)
	{
		if (Cur_cmd_brief->stage[i].text)
			sexp_replace_variable_names_with_values(Cur_cmd_brief->stage[i].text, CMD_BRIEF_TEXT_MAX);
	}

	if (Cur_cmd_brief->num_stages <= 0)
		return;

	gr_reset_clip();
	gr_clear();
	Mouse_hidden++;
	gr_flip();
	Mouse_hidden--;

	/*
	Palette_bmp = bm_load("BarracksPalette");	//CommandBriefPalette");
	Assert(Palette_bmp);
	bm_get_palette(Palette_bmp, Palette, Palette_name);  // get the palette for this bitmap
	gr_set_palette(Palette_name, Palette, 1);
	*/

	// first determine which layout to use
	Uses_scroll_buttons = 1;	// assume true
	Cmd_brief_background_bitmap = bm_load(Cmd_brief_fname[Uses_scroll_buttons][gr_screen.res]);	// try to load extra one first
	if (Cmd_brief_background_bitmap < 0)	// failed to load
	{
		Uses_scroll_buttons = 0;	// nope, sorry
		Cmd_brief_background_bitmap = bm_load(Cmd_brief_fname[Uses_scroll_buttons][gr_screen.res]);
	}

	Ui_window.create(0, 0, gr_screen.max_w_unscaled, gr_screen.max_h_unscaled, 0);
	Ui_window.set_mask_bmap(Cmd_brief_mask[Uses_scroll_buttons][gr_screen.res]);

	// Cmd_brief_ask_for_cd = 1;

	for (i = 0; i < NUM_CMD_BRIEF_BUTTONS; i++)
	{
		b = &Cmd_brief_buttons[gr_screen.res][i];

		b->button.create(&Ui_window, "", b->x, b->y, 60, 30, 0, 1);
		// set up callback for when a mouse first goes over a button
		b->button.set_highlight_action(common_play_highlight_sound);
		b->button.set_bmaps(b->filename);
		b->button.link_hotspot(b->hotspot);
	}

	// add text
	for (i = 0; i < CMD_BRIEF_NUM_TEXT; i++)
	{
		Ui_window.add_XSTR(&Cmd_brief_text[gr_screen.res][i]);
	}

	// set up readyrooms for buttons so we draw the correct animation frame when a key is pressed
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_FIRST_STAGE].button.set_hotkey(KEY_SHIFTED | KEY_LEFT);
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_LAST_STAGE].button.set_hotkey(KEY_SHIFTED | KEY_RIGHT);
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_PREV_STAGE].button.set_hotkey(KEY_LEFT);
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_NEXT_STAGE].button.set_hotkey(KEY_RIGHT);
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_ACCEPT].button.set_hotkey(KEY_CTRLED | KEY_ENTER);
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_HELP].button.set_hotkey(KEY_F1);
	Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_OPTIONS].button.set_hotkey(KEY_F2);

	// extra - Goober5000
	if (Uses_scroll_buttons)
	{
		Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_SCROLL_UP].button.set_hotkey(KEY_UP);
		Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_SCROLL_DOWN].button.set_hotkey(KEY_DOWN);
	}

	// load in help overlay bitmap	
	help_overlay_load(CMD_BRIEF_OVERLAY);
	help_overlay_set_state(CMD_BRIEF_OVERLAY, 0);

	for (i = 0; i < Cur_cmd_brief->num_stages; i++)
		cmd_brief_ani_wave_init(i);

	cmd_brief_init_voice();
	//Cur_anim_instance = NULL;
	cmd_brief_new_stage(0);
	Cmd_brief_paused = 0;
	Cmd_brief_inited = 1;

	//#endif
}

void cmd_brief_close()
{
	int i;

	if (Cmd_brief_inited)
	{
		cmd_brief_stop_anim(-1);
		generic_anim_unload(&Cur_Anim);
		for (i = 0; i < Cur_cmd_brief->num_stages; i++)
		{
			if (Cur_cmd_brief->stage[i].wave >= 0)
				audiostream_close_file(Cur_cmd_brief->stage[i].wave, 0);

		}

		// so that the same ani will reload properly upon return
		Cur_anim_filename = "~~~~";

		if (Cmd_brief_background_bitmap >= 0)
			bm_release(Cmd_brief_background_bitmap);

		// unload the overlay bitmap
		help_overlay_unload(CMD_BRIEF_OVERLAY);

		Ui_window.destroy();
		/*
		if (Palette_bmp){
			bm_unload(Palette_bmp);
		}
		*/

		game_flush();
		Cmd_brief_inited = 0;
	}

	// Stop any speech from running over
	fsspeech_stop();
}

void cmd_brief_do_frame(float frametime)
{
	char buf[40];
	int i, k, w, h, x, y;

	// if no command briefing exists, skip this screen.
	if (!Cmd_brief_inited)
	{
		cmd_brief_exit();
		return;
	}

	if (help_overlay_active(CMD_BRIEF_OVERLAY))
	{
		Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_HELP].button.reset_status();
		Ui_window.set_ignore_gadgets(1);
	}

	k = Ui_window.process() & ~KEY_DEBUGGED;

	if ((k > 0) || B1_JUST_RELEASED)
	{
		if (help_overlay_active(CMD_BRIEF_OVERLAY))
		{
			help_overlay_set_state(CMD_BRIEF_OVERLAY, 0);
			Ui_window.set_ignore_gadgets(0);
			k = 0;
		}
	}

	if (!help_overlay_active(CMD_BRIEF_OVERLAY))
	{
		Ui_window.set_ignore_gadgets(0);
	}

	switch (k)
	{
	case KEY_ESC:
		common_music_close();
		gameseq_post_event(GS_EVENT_MAIN_MENU);
		break;
	}	// end switch

	for (i = 0; i < NUM_CMD_BRIEF_BUTTONS; i++)
	{
		if (Cmd_brief_buttons[gr_screen.res][i].button.pressed())
		{
			cmd_brief_button_pressed(i);
		}
	}

	cmd_brief_voice_play(Cur_stage);
	common_music_do();

	if (cmd_brief_check_stage_done() && Player->auto_advance && (Cur_stage < Cur_cmd_brief->num_stages - 1))
	{
		if ((Cur_Anim.num_frames <= 1) || Cur_Anim.done_playing)
		{
			cmd_brief_new_stage(Cur_stage + 1);
		}
	}

	GR_MAYBE_CLEAR_RES(Cmd_brief_background_bitmap);
	if (Cmd_brief_background_bitmap >= 0)
	{
		gr_set_bitmap(Cmd_brief_background_bitmap);
		gr_bitmap(0, 0);
	}

	if (Cur_Anim.num_frames > 0)
	{
		bm_get_info((Cur_Anim.streaming) ? Cur_Anim.bitmap_id : Cur_Anim.first_frame, &x, &y, NULL, NULL, NULL);

		float oldAspect = (float)gr_screen.max_h_unscaled / (float)gr_screen.max_w_unscaled;
		float newAspect = (float)gr_screen.max_h / (float)gr_screen.max_w;

		x = (int)((float)x * newAspect / oldAspect);

		x = Cmd_image_center_coords[gr_screen.res][CMD_X_COORD] - x / 2;
		y = Cmd_image_center_coords[gr_screen.res][CMD_Y_COORD] - y / 2;

		// RAII aspect ratio keeping
		KeepAspectRatio keep(true);
		ActivateAlignment left(ALIGNMENT_LEFT);
		generic_anim_render(&Cur_Anim, (Cmd_brief_paused) ? 0 : frametime, x, y, true);
	}

	Ui_window.draw();

	if (!Player->auto_advance)
	{
		Cmd_brief_buttons[gr_screen.res][CMD_BRIEF_BUTTON_PAUSE].button.draw_forced(2);
	}

	gr_set_font(FONT1);
	gr_set_color_fast(&Color_text_heading);

	sprintf(buf, XSTR("Stage %d of %d", 464), Cur_stage + 1, Cur_cmd_brief->num_stages);
	gr_get_string_size(&w, NULL, buf);
	gr_string(Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_X_COORD] +
		Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_W_COORD] - w, Cmd_stage_y[gr_screen.res], buf);

	if (brief_render_text(Top_cmd_brief_text_line, Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.
		res][CMD_X_COORD], Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_Y_COORD],
		Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_H_COORD], frametime, 0, 1))
	{
		Voice_good_to_go = 1;
	}

	// maybe output the "more" indicator
	if ((Cmd_brief_text_max_lines[gr_screen.res] + Top_cmd_brief_text_line) < Num_brief_text_lines[0])
	{
		// can be scrolled down
		int more_txt_x = Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.
			res][CMD_X_COORD] + (Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_W_COORD] / 2) - 10;
		int more_txt_y = Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_Y_COORD] +
			Cmd_text_wnd_coords[Uses_scroll_buttons][gr_screen.res][CMD_H_COORD] - 2;				// located below brief text, centered

		gr_get_string_size(&w, &h, XSTR("more", 1469), strlen(XSTR("more", 1469)));
		gr_set_color_fast(&Color_black);
		gr_rect(more_txt_x - 2, more_txt_y, w + 3, h);
		gr_set_color_fast(&Color_red);
		gr_string(more_txt_x, more_txt_y, XSTR("more", 1469));  // base location on the input x and y?
	}

	// blit help overlay if active
	help_overlay_maybe_blit(CMD_BRIEF_OVERLAY);

	gr_flip();
}

int mission_has_cmd_brief()
{
	return (Cur_cmd_brief != NULL && Cur_cmd_brief->num_stages > 0);
}
