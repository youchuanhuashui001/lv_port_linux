#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-agnostic audio output HAL.
 *
 * Implementations:
 *   audio_out_sdl.c   - PC simulator (SDL2)
 *   audio_out_alsa.c  - embedded Linux boards (ALSA/libasound), phase 2
 */

/*
 * Initialize the backend. Call once before any other function.
 * Returns 0 on success, -1 on failure.
 */
int audio_out_init(void);

/*
 * Open the output device for a given stream format.
 * The implementation must accept S16_LE interleaved samples.
 * Returns 0 on success, -1 on failure.
 */
int audio_out_open(uint32_t sample_rate, uint8_t channels);

/*
 * Try to write PCM frames into the internal ring buffer.
 * Never blocks the caller: writes as many frames as fit and
 * returns the number of frames actually accepted.
 */
size_t audio_out_write(const int16_t *pcm, size_t frames);

/* Bytes of PCM currently buffered inside the backend (0 if closed). */
size_t audio_out_queued(void);

/* Pause/resume playback output. While paused the callback drains silence. */
void audio_out_pause(bool on);

/* Software volume in percent, 0..100 */
void audio_out_set_volume(uint8_t pct);

/* Stop and release the output device. */
void audio_out_close(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_OUT_H */
