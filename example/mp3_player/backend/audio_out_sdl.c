#include "audio_out.h"

#include <string.h>

#include <SDL2/SDL.h>

/*
 * SDL2 implementation of the audio_out HAL.
 *
 * Uses SDL's queued-audio API (SDL_QueueAudio): SDL owns the sample
 * FIFO and drains it from its own audio thread, so no hand-rolled
 * ring buffer or locking is needed on the producer side.
 *
 * write() never blocks: it pushes only up to AO_QUEUE_LIMIT bytes,
 * the decoding pump polls for room.
 */

#define AO_QUEUE_LIMIT (192u * 1024u) /* ~0.5 s @44.1k stereo S16 */
#define AO_SAMPLES_MAX 1024

static SDL_AudioDeviceID dev;
static SDL_AudioSpec spec;
static uint32_t volume_permil = 1000; /* 100% == 1000 permille */
static uint8_t frame_bytes;

void audio_out_pause(bool on)
{
	if(dev != 0) SDL_PauseAudioDevice(dev, on ? 1 : 0);
}

size_t audio_out_queued(void)
{
	return dev != 0 ? (size_t)SDL_GetQueuedAudioSize(dev) : 0;
}

void audio_out_set_volume(uint8_t pct)
{
	if(pct > 100) pct = 100;
	volume_permil = (uint32_t)pct * 10u;
}

int audio_out_init(void)
{
	if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		SDL_Log("audio_out: SDL_InitSubSystem failed: %s", SDL_GetError());
		return -1;
	}
	return 0;
}

int audio_out_open(uint32_t sample_rate, uint8_t channels)
{
	SDL_AudioSpec want;

	if(dev != 0) audio_out_close();

	memset(&want, 0, sizeof(want));
	want.freq = (int)sample_rate;
	want.format = AUDIO_S16SYS;
	want.channels = channels;
	want.samples = AO_SAMPLES_MAX;
	want.callback = NULL; /* use SDL_QueueAudio */

	dev = SDL_OpenAudioDevice(NULL, 0, &want, &spec, 0);
	if(dev == 0) {
		SDL_Log("audio_out: open failed: %s", SDL_GetError());
		return -1;
	}

	frame_bytes = (uint8_t)(spec.channels * sizeof(int16_t));
	SDL_ClearQueuedAudio(dev);

	SDL_PauseAudioDevice(dev, 0); /* start draining */
	return 0;
}

size_t audio_out_write(const int16_t *pcm, size_t frames)
{
	size_t bytes = frames * frame_bytes;
	size_t queued;
	uint8_t vol_scale;
	int16_t scaled[AO_SAMPLES_MAX];
	size_t off;

	if(dev == 0 || pcm == NULL || bytes == 0) return 0;

	queued = SDL_GetQueuedAudioSize(dev);
	if(queued >= AO_QUEUE_LIMIT) return 0;
	if(bytes > AO_QUEUE_LIMIT - queued) {
		bytes = (AO_QUEUE_LIMIT - queued) / frame_bytes * frame_bytes;
	}

	/* Apply software volume while pushing into the device queue. */
	vol_scale = (uint8_t)((volume_permil * 255u + 500u) / 1000u);
	if(vol_scale >= 255) {
		return (size_t)SDL_QueueAudio(dev, pcm, (Uint32)bytes) == 0 ?
		       bytes / frame_bytes : 0;
	}

	/* Scale in chunks through a local buffer. */
	for(off = 0; off < bytes; ) {
		size_t chunk = bytes - off;
		size_t i, n;
		if(chunk > sizeof(scaled)) chunk = sizeof(scaled);
		n = chunk / sizeof(int16_t);
		for(i = 0; i < n; i++) {
			int32_t s = pcm[off / sizeof(int16_t) + i];
			scaled[i] = (int16_t)((s * (int32_t)vol_scale + 127) / 255);
		}
		if(SDL_QueueAudio(dev, scaled, (Uint32)(n * sizeof(int16_t))) != 0) break;
		off += n * sizeof(int16_t);
	}
	return off / frame_bytes;
}

void audio_out_close(void)
{
	if(dev != 0) {
		SDL_PauseAudioDevice(dev, 1);
		SDL_ClearQueuedAudio(dev);
		SDL_CloseAudioDevice(dev);
		dev = 0;
	}
}
