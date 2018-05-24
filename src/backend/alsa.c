/*
 * Copyright 2016 Jonathan Eyolfson
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "alsa.h"

#include "../exit_code.h"

#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>

static snd_pcm_t *handle;

unsigned char buffer[16*1024];

uint8_t alsa_init()
{
	int ret;
	ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (ret != 0) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

	unsigned int channels = 1;
	unsigned int sample_rate = 48000; // Hz
	int soft_resample = 1;
	unsigned int latency = 500000; // us

	ret = snd_pcm_set_params(
		handle,
		SND_PCM_FORMAT_U8,
		SND_PCM_ACCESS_RW_INTERLEAVED,
		channels,
		sample_rate,
		soft_resample,
		latency
	);
	if (ret != 0) {
		return EXIT_CODE_OS_ERROR_BIT;
	}

	for (unsigned i = 0; i < sizeof(buffer); ++i) {
		if ((i / 16) % 2 == 0) {
			buffer[i] = 10;
		}
		else {
			buffer[i] = 0;
		}
	}

	snd_pcm_sframes_t frames;
	for (unsigned int i = 0; i < 16; i++) {
		frames = snd_pcm_writei(handle, buffer, sizeof(buffer));
		if (frames < 0)
			frames = snd_pcm_recover(handle, frames, 0);
		if (frames < 0) {
			printf("snd_pcm_writei failed: %s\n",
				snd_strerror(frames));
			break;
		}
		if (frames > 0 && frames < (long)sizeof(buffer))
			printf("Short write (expected %li, wrote %li)\n",
				(long)sizeof(buffer), frames);
	}

	return 0;
}

void alsa_fini()
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}
