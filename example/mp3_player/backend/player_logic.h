#ifndef PLAYER_LOGIC_H
#define PLAYER_LOGIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MP3 player business logic (Model).
 *
 * Single-threaded: every entry point must be called from the LVGL
 * main loop. Internally driven by an lv_timer decoding pump which
 * feeds the audio_out HAL and periodically reports status through
 * the registered callback.
 */

typedef enum {
	PL_STOPPED = 0,
	PL_PLAYING,
	PL_PAUSED,
} pl_state_t;

typedef struct {
	const char *title;   /* ID3 title or file name, UTF-8 */
	const char *artist;  /* ID3 artist or "" */
	uint32_t duration_ms;
	uint32_t position_ms;
	pl_state_t state;
	int32_t cur_index;   /* -1 when nothing selected */
	int32_t count;       /* total tracks in library */
} pl_status_t;

/* Invoked from the LVGL main loop roughly every PL_STATUS_PERIOD_MS. */
typedef void (*pl_status_cb_t)(const pl_status_t *status, void *user_data);

#define PL_STATUS_PERIOD_MS 500u

typedef struct {
	const char *title;
	const char *artist;
	uint32_t duration_ms;
} pl_track_info_t;

/*
 * Scan `music_dir` for *.mp3 files and initialize the player.
 * Returns the number of tracks found, or -1 if the directory could
 * not be opened. The callback (if not NULL) is invoked once ready.
 */
int player_logic_init(const char *music_dir, pl_status_cb_t cb, void *user_data);

/* Fill `out` with metadata of track `index`; returns 0 on success. */
int player_logic_get_track(int index, pl_track_info_t *out);

void player_logic_toggle_play(void);
void player_logic_next_track(void);
void player_logic_prev_track(void);

/* Select a track by index [0..count-1]; keeps current play state. */
void player_logic_select(int index);

#ifdef __cplusplus
}
#endif

#endif /* PLAYER_LOGIC_H */
