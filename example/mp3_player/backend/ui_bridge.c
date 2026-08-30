#include "ui_bridge.h"

#include <stdio.h>
#include <stdlib.h>

#include "lvgl/lvgl.h"

#include "ui/ui.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

#define ROW_W   668
#define ROW_H   40
/* label x offsets inside a row, relative to row center */
#define COL_NAME_X   (-234) /* keep the 200px name column inside the 668px row */
#define COL_SONGER_X (-22)
#define COL_TIME_X   (211)
#define COL_TEXT_W   200
#define METADATA_FONT_PATH "A:lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf"

static lv_obj_t **rows;
static int32_t row_count;
static lv_font_t *metadata_font;

static const lv_font_t *get_metadata_font(void)
{
	if(metadata_font == NULL)
		metadata_font = lv_tiny_ttf_create_file(METADATA_FONT_PATH, 16);

	return metadata_font ? metadata_font : &lv_font_source_han_sans_sc_16_cjk;
}

static void fmt_time(char *buf, size_t cap, uint32_t ms)
{
	snprintf(buf, cap, "%02u:%02u",
	         (unsigned)(ms / 60000u), (unsigned)((ms / 1000u) % 60u));
}

/* ------------------------------------------------------------------ */
/* Playlist construction                                               */
/* ------------------------------------------------------------------ */

static void row_click_cb(lv_event_t *e)
{
	intptr_t idx = (intptr_t)lv_event_get_user_data(e);
	player_logic_select((int)idx);
	lv_obj_add_flag(ui_AudioPlaylistPanel, LV_OBJ_FLAG_HIDDEN);
}

void AudioPlayToPause(lv_event_t *e)
{
	(void)e;
	player_logic_toggle_play();
}

void AudioPauseToPlay(lv_event_t *e)
{
	(void)e;
	player_logic_toggle_play();
}

void AudioPreMusic(lv_event_t *e)
{
	(void)e;
	player_logic_prev_track();
}

void AudioNextMusic(lv_event_t *e)
{
	(void)e;
	player_logic_next_track();
}

static lv_obj_t *make_row(int index, const char *name, const char *songer,
                          uint32_t duration_ms)
{
	lv_obj_t *row = lv_obj_create(ui_AudioPlaylistPanel);
	const lv_font_t *font = get_metadata_font();
	lv_obj_set_size(row, ROW_W, ROW_H);
	lv_obj_set_align(row, LV_ALIGN_CENTER);
	lv_obj_set_clickable(row, true);
	lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

	lv_obj_t *l_name = lv_label_create(row);
	lv_obj_set_x(l_name, COL_NAME_X);
	lv_obj_set_y(l_name, 0);
	lv_obj_set_align(l_name, LV_ALIGN_CENTER);
	lv_obj_set_width(l_name, COL_TEXT_W);
	lv_label_set_long_mode(l_name, LV_LABEL_LONG_DOT);
	lv_obj_set_style_text_font(l_name, font,
	                           LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_label_set_text(l_name, name);

	lv_obj_t *l_songer = lv_label_create(row);
	lv_obj_set_x(l_songer, COL_SONGER_X);
	lv_obj_set_y(l_songer, 0);
	lv_obj_set_align(l_songer, LV_ALIGN_CENTER);
	lv_obj_set_width(l_songer, COL_TEXT_W);
	lv_label_set_long_mode(l_songer, LV_LABEL_LONG_DOT);
	lv_obj_set_style_text_font(l_songer, font,
	                           LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_label_set_text(l_songer, songer);

	lv_obj_t *l_time = lv_label_create(row);
	lv_obj_set_x(l_time, COL_TIME_X);
	lv_obj_set_y(l_time, 0);
	lv_obj_set_align(l_time, LV_ALIGN_CENTER);
	lv_label_set_text_fmt(l_time, "%02u:%02u",
	                      (unsigned)(duration_ms / 60000u),
	                      (unsigned)((duration_ms / 1000u) % 60u));

	lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED,
	                    (void *)(intptr_t)index);
	return row;
}

static void build_playlist(int count)
{
	int i;

	/* keep the SLS prototype row but hide it */
	lv_obj_add_flag(ui_AudioPlaylistSubPanel, LV_OBJ_FLAG_HIDDEN);

	if(count <= 0) return;

	rows = calloc((size_t)count, sizeof(lv_obj_t *));
	if(rows == NULL) return;
	row_count = count;

	for(i = 0; i < count; i++) {
		pl_track_info_t ti;
		if(player_logic_get_track(i, &ti) != 0) {
			rows[i] = NULL;
			continue;
		}
		rows[i] = make_row(i, ti.title, ti.artist, ti.duration_ms);
	}
}

/* ------------------------------------------------------------------ */
/* Status -> widgets                                                   */
/* ------------------------------------------------------------------ */

static void update_transport_icons(pl_state_t state)
{
	if(state == PL_PLAYING) {
		lv_obj_add_flag(ui_AudioPlayButton, LV_OBJ_FLAG_HIDDEN);
		lv_obj_remove_flag(ui_AudioPauseButton, LV_OBJ_FLAG_HIDDEN);
	}
	else {
		lv_obj_remove_flag(ui_AudioPlayButton, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(ui_AudioPauseButton, LV_OBJ_FLAG_HIDDEN);
	}
}

void ui_bridge_on_status(const pl_status_t *st, void *user_data)
{
	char buf[16];
	const lv_font_t *font = get_metadata_font();

	(void)user_data;

	lv_label_set_text(ui_AudioSongLabel, st->title);
	lv_label_set_text(ui_AudioSongerLabel, st->artist);
	lv_label_set_long_mode(ui_AudioSongLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
	lv_label_set_long_mode(ui_AudioSongerLabel, LV_LABEL_LONG_DOT);

	lv_obj_set_style_text_font(ui_AudioSongLabel,
	                           font,
	                           LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui_AudioSongerLabel,
	                           font,
	                           LV_PART_MAIN | LV_STATE_DEFAULT);

	lv_bar_set_range(ui_AudioPlayBar, 0, (int32_t)st->duration_ms);
	lv_bar_set_value(ui_AudioPlayBar, (int32_t)st->position_ms, LV_ANIM_OFF);

	fmt_time(buf, sizeof(buf), st->position_ms);
	lv_label_set_text(ui_AudioCurrentTimeLabel, buf);

	fmt_time(buf, sizeof(buf), st->duration_ms);
	lv_label_set_text(ui_AudioTotalTimeLabel, buf);

	update_transport_icons(st->state);
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

void ui_bridge_init(const char *music_dir)
{
	int count = player_logic_init(music_dir, ui_bridge_on_status, NULL);

	build_playlist(count > 0 ? count : 0);
	if(count > 0) player_logic_select(0);
	update_transport_icons(PL_STOPPED);
}
