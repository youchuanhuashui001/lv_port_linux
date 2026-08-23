#include "player_logic.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "lvgl/lvgl.h"

#include "audio_out.h"
#include "mp3_tags.h"

#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "decoder/minimp3.h"
#include "decoder/minimp3_ex.h"

/*
 * MP3 player state machine.
 *
 * Threading model:
 *   - All logic runs on the LVGL main loop.
 *   - A pump timer (PL_PUMP_PERIOD_MS) decodes MP3 frames and pushes
 *     them into the audio_out HAL; it also reports status periodically.
 *   - The audio backend consumes samples from its own thread; the
 *     hand-off is a non-blocking queue owned by audio_out.
 */

#define PL_MAX_TRACKS       64
#define PL_PATH_MAX         256
#define PL_TITLE_MAX        96
#define PL_ARTIST_MAX       64
#define PL_PUMP_PERIOD_MS   50u
/* Decode ahead until this much audio data is queued. */
#define PL_QUEUE_TARGET     (160u * 1024u)

typedef struct {
	char path[PL_PATH_MAX];      /* lv_fs path, e.g. "A:/dir/x.mp3" */
	char native[PL_PATH_MAX];    /* OS path for stat()              */
	char title[PL_TITLE_MAX];
	char artist[PL_ARTIST_MAX];
	uint32_t duration_ms;
} pl_track_t;

static pl_track_t tracks[PL_MAX_TRACKS];
static int32_t track_count;
static int32_t cur = -1;

static pl_state_t state = PL_STOPPED;
static pl_status_cb_t status_cb;
static void *status_user;

static lv_timer_t *pump_timer;
static uint8_t status_ticks;

static mp3dec_ex_t dec;
static bool dec_open;
static uint64_t decoded_samples;    /* within current track */
static uint32_t stream_hz;
static uint8_t stream_ch;

static void close_decoder(void);
static void report_status(void);
static void start_track(int index, bool play);
static bool open_current_stream(void);

/* ------------------------------------------------------------------ */
/* Library scanning                                                    */
/* ------------------------------------------------------------------ */

static bool path_is_mp3(const char *name)
{
	size_t len = strlen(name);
	return len > 4 && strcasecmp(name + len - 4, ".mp3") == 0;
}

/* Build "A:<native>" style lv_fs path from LV_FS_STDIO_LETTER. */
static void make_fs_path(char *dst, size_t cap, const char *native)
{
	snprintf(dst, cap, "%c:%s", (char)LV_FS_STDIO_LETTER, native);
}

static void text_copy(char *dst, size_t cap, const char *src)
{
	size_t n = strlen(src);
	if(n >= cap) n = cap - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

static void strip_ext(char *s)
{
	char *dot = strrchr(s, '.');
	if(dot && dot != s) *dot = '\0';
}

static void derive_title_from_name(pl_track_t *t)
{
	const char *base = strrchr(t->native, '/');
	base = base ? base + 1 : t->native;
	text_copy(t->title, sizeof(t->title), base);
	strip_ext(t->title);
}

static int scan_library(const char *music_dir)
{
	DIR *d;
	struct dirent *e;

	d = opendir(music_dir);
	if(d == NULL) return -1;

	while(track_count < PL_MAX_TRACKS && (e = readdir(d)) != NULL) {
		pl_track_t *t;
		char fs_path[PL_PATH_MAX + 8];

		if(e->d_name[0] == '.') continue;
		if(!path_is_mp3(e->d_name)) continue;

		t = &tracks[track_count];
		text_copy(t->native, sizeof(t->native), music_dir);
		strncat(t->native, "/", sizeof(t->native) - strlen(t->native) - 1);
		strncat(t->native, e->d_name,
		        sizeof(t->native) - strlen(t->native) - 1);

		make_fs_path(fs_path, sizeof(fs_path), t->native);

		t->duration_ms = 0;
		if(mp3_tags_read(fs_path, t->title, sizeof(t->title),
		                 t->artist, sizeof(t->artist)) != 0 ||
		   t->title[0] == '\0') {
			derive_title_from_name(t);
		}

		track_count++;
	}
	closedir(d);
	return track_count;
}

/* ------------------------------------------------------------------ */
/* Decoding                                                            */
/* ------------------------------------------------------------------ */

static size_t io_read(void *buf, size_t size, void *user)
{
	lv_fs_file_t *f = user;
	uint32_t br = 0;
	if(lv_fs_read(f, buf, (uint32_t)size, &br) != LV_FS_RES_OK) return 0;
	return br;
}

static int io_seek(uint64_t position, void *user)
{
	lv_fs_file_t *f = user;
	return lv_fs_seek(f, (uint32_t)position, LV_FS_SEEK_SET) ==
	       LV_FS_RES_OK ? 0 : -1;
}

static void close_decoder(void)
{
	if(dec_open) {
		mp3dec_ex_close(&dec);
		dec_open = false;
	}
}

/* duration_ms = filesize*8 / bitrate; CBR-exact, VBR approximation.
 * bits / [kbps] == bits*1000/[kbps*1000] i.e. milliseconds directly. */
static uint32_t estimate_duration(const char *native, int avg_kbps)
{
	uint64_t bits;
	FILE *fp;

	if(avg_kbps <= 0) return 0;
	fp = fopen(native, "rb");
	if(fp == NULL) return 0;
	fseek(fp, 0, SEEK_END);
	bits = (uint64_t)ftell(fp) * 8u;
	fclose(fp);
	return (uint32_t)(bits / (uint64_t)avg_kbps);
}

static bool open_current_stream(void)
{
	static lv_fs_file_t f;
	static mp3dec_io_t io;
	char fs_path[PL_PATH_MAX + 8];

	close_decoder();

	make_fs_path(fs_path, sizeof(fs_path), tracks[cur].native);
	if(lv_fs_open(&f, fs_path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
		LV_LOG_ERROR("player: cannot open %s", tracks[cur].native);
		return false;
	}

	io.read = io_read;
	io.read_data = &f;
	io.seek = io_seek;
	io.seek_data = &f;

	/* DO_NOT_SCAN keeps opening fast; duration comes from estimation. */
	if(mp3dec_ex_open_cb(&dec, &io, MP3D_DO_NOT_SCAN) != 0) {
		LV_LOG_ERROR("player: decoder open failed for %s", tracks[cur].native);
		lv_fs_close(&f);
		return false;
	}

	dec_open = true;
	decoded_samples = 0;
	stream_hz = dec.info.hz ? dec.info.hz : 44100;
	stream_ch = dec.info.channels ? dec.info.channels : 2;

	audio_out_open(stream_hz, stream_ch);

	tracks[cur].duration_ms =
	    estimate_duration(tracks[cur].native, dec.info.bitrate_kbps);

	return true;
}

static void start_track(int index, bool play)
{
	cur = index;
	decoded_samples = 0;
	status_ticks = 0;

	if(open_current_stream()) {
		state = play ? PL_PLAYING : PL_PAUSED;
		audio_out_pause(state == PL_PAUSED);
	}
	else {
		state = PL_STOPPED;
	}
	report_status();
}

/* ------------------------------------------------------------------ */
/* Status reporting                                                    */
/* ------------------------------------------------------------------ */

static void report_status(void)
{
	pl_status_t st;

	if(status_cb == NULL) return;

	st.title = cur >= 0 ? tracks[cur].title : "";
	st.artist = cur >= 0 ? tracks[cur].artist : "";
	st.duration_ms = cur >= 0 ? tracks[cur].duration_ms : 0;
	st.position_ms = stream_hz ?
	                 (uint32_t)(decoded_samples * 1000u / stream_hz) : 0;
	st.state = state;
	st.cur_index = cur;
	st.count = track_count;

	status_cb(&st, status_user);
}

/* ------------------------------------------------------------------ */
/* Pump timer - runs on the LVGL main loop                             */
/* ------------------------------------------------------------------ */

static void auto_next(void)
{
	int next = (cur + 1 < track_count) ? cur + 1 : 0;
	start_track(next, true);
}

static void pump_cb(lv_timer_t *timer)
{
	(void)timer;

	if(state != PL_PLAYING || cur < 0 || !dec_open) return;

	/* Decode until the device queue is comfortably filled. */
	while(audio_out_queued() < PL_QUEUE_TARGET) {
		mp3d_sample_t *samples;
		mp3dec_frame_info_t info;
		size_t got, frames, written;

		got = mp3dec_ex_read_frame(&dec, &samples, &info,
		                           MINIMP3_MAX_SAMPLES_PER_FRAME);
		if(got == 0) {          /* EOF or error */
			auto_next();
			return;
		}

		frames = got / stream_ch;
		written = audio_out_write(samples, frames);

		decoded_samples += (uint64_t)written * stream_ch;

		if(written < frames) break;     /* queue full, try again later */
	}

	/* Periodic UI refresh. */
	if(++status_ticks >= PL_STATUS_PERIOD_MS / PL_PUMP_PERIOD_MS) {
		status_ticks = 0;
		report_status();
	}
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int player_logic_init(const char *music_dir, pl_status_cb_t cb, void *user_data)
{
	status_cb = cb;
	status_user = user_data;
	track_count = 0;
	cur = -1;
	state = PL_STOPPED;
	dec_open = false;

	if(audio_out_init() != 0) LV_LOG_WARN("player: audio init failed");

	if(scan_library(music_dir) <= 0) {
		LV_LOG_WARN("player: no mp3 files found in %s", music_dir);
		pump_timer = lv_timer_create(pump_cb, PL_PUMP_PERIOD_MS, NULL);
		return -1;
	}

	pump_timer = lv_timer_create(pump_cb, PL_PUMP_PERIOD_MS, NULL);
	report_status();
	return track_count;
}

int player_logic_get_track(int index, pl_track_info_t *out)
{
	if(index < 0 || index >= track_count || out == NULL) return -1;
	out->title = tracks[index].title;
	out->artist = tracks[index].artist;
	out->duration_ms = tracks[index].duration_ms;
	return 0;
}

void player_logic_toggle_play(void)
{
	if(cur < 0) {
		if(track_count > 0) start_track(0, true);
		return;
	}
	if(state == PL_PLAYING) {
		state = PL_PAUSED;
		audio_out_pause(true);
	}
	else if(state == PL_PAUSED) {
		state = PL_PLAYING;
		audio_out_pause(false);
	}
	else if(cur >= 0 && !dec_open) {   /* STOPPED after an error */
		start_track(cur, true);
	}
	report_status();
}

void player_logic_next_track(void)
{
	if(track_count == 0) return;
	start_track((cur + 1 < track_count) ? cur + 1 : 0,
	            state == PL_STOPPED ? false : true);
}

void player_logic_prev_track(void)
{
	if(track_count == 0) return;
	start_track(cur > 0 ? cur - 1 : track_count - 1,
	            state == PL_STOPPED ? false : true);
}

void player_logic_select(int index)
{
	if(index < 0 || index >= track_count) return;
	start_track(index, state == PL_STOPPED ? false : true);
}
