#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <alsa/asoundlib.h>

#include "rf-ctrl.h"

#define HARDWARE_NAME			"Alsa"

#define SAMPLE_SIZE			2 // 16bits (SND_PCM_FORMAT_S16_LE)
#define SAMPLES_PER_FRAME		channels
#define BYTES_PER_FRAME			(SAMPLE_SIZE * SAMPLES_PER_FRAME)

#define ALSA_DEVICE_OUT			"default"

static snd_pcm_t *playback_handle = NULL;
static unsigned int samplerate = 48000;
static unsigned int channels = 2;
static unsigned int periods = 2;
static snd_pcm_format_t sample_format = SND_PCM_FORMAT_S16_LE;


static int alsa_configure_device(snd_pcm_t *handle, size_t sample_count)
{
	int err;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	snd_pcm_uframes_t period_size; /* Periodsize (frames) */
	snd_pcm_uframes_t buffer_size; /* Buffersize (frames) */

	if (handle == NULL) {
		fprintf(stderr, "%s: Alsa device not open\n", HARDWARE_NAME);
		return -1;
	}

	/* HW Params */
	if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		fprintf(stderr, "%s: cannot allocate hardware parameter structure (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_any(handle, hw_params)) < 0) {
		fprintf (stderr, "%s: cannot initialize hardware parameter structure (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf(stderr, "%s: cannot set access type (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_format(handle, hw_params, sample_format)) < 0) {
		fprintf(stderr, "%s: cannot set sample format (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	if ((err = snd_pcm_hw_params_set_rate_near(handle, hw_params, &samplerate, 0)) < 0) {
		fprintf(stderr, "%s: cannot set sample rate (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}
	dbg_printf(2, "%s: Samplerate: %u\n", HARDWARE_NAME, samplerate);

	if ((err = snd_pcm_hw_params_set_channels(handle, hw_params, channels)) < 0) {
		fprintf(stderr, "%s: cannot set channel count (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}
	dbg_printf(2, "%s: Channels: %u\n", HARDWARE_NAME, channels);

	if ((err = snd_pcm_hw_params_set_periods_near(handle, hw_params, &periods, 0)) < 0) {
		fprintf(stderr, "%s: cannot set period count (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}
	dbg_printf(3, "%s: Periods : %u\n", HARDWARE_NAME, periods);

	period_size = (sample_count/SAMPLES_PER_FRAME);
	if ((err = snd_pcm_hw_params_set_period_size_near(handle, hw_params, &period_size, 0)) < 0) {
		fprintf(stderr, "%s: cannot set period size (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}
	dbg_printf(3, "%s: Period size : %u\n", HARDWARE_NAME, period_size);

	buffer_size = period_size * periods;
	if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, hw_params, &buffer_size)) < 0) {
		fprintf(stderr, "%s: cannot set buffer size (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}
	dbg_printf(3, "%s: Buffer size: %lu\n", HARDWARE_NAME, (long unsigned int) buffer_size);

	if ((err = snd_pcm_hw_params(handle, hw_params)) < 0) {
		fprintf(stderr, "%s: cannot set parameters (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	/* SW Params */
	if ((err = snd_pcm_sw_params_malloc(&sw_params)) < 0) {
		fprintf(stderr, "%s: cannot allocate software parameter structure (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	if (snd_pcm_sw_params_current(handle, sw_params) < 0) {
		fprintf(stderr, "%s: cannot get sw params (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	/* Start playing after the first write */
	if (snd_pcm_sw_params_set_start_threshold(handle, sw_params, 1) < 0) {
		fprintf(stderr, "%s: cannot set start threshold (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	/* Wake up on every interrupt */
	if (snd_pcm_sw_params_set_avail_min(handle, sw_params, 1) < 0) {
		fprintf(stderr, "%s: cannot set min avail (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	if (snd_pcm_sw_params(handle, sw_params) < 0) {
		fprintf(stderr, "%s: cannot set sw params (%s)\n", HARDWARE_NAME,
			 snd_strerror (err));
		return -1;
	}

	snd_pcm_hw_params_free(hw_params);
	snd_pcm_sw_params_free(sw_params);

	return 0;
}

static int alsa_open_playback(char * device_out)
{
	int err;

	if (playback_handle == NULL) {
		if ((err = snd_pcm_open(&playback_handle, device_out, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
			fprintf(stderr, "%s: cannot open audio device %s (playback) (%s)\n", HARDWARE_NAME,
				device_out,
				snd_strerror (err));
			return -1;
		}
	}

	return 0;
}

static void alsa_close_playback(void)
{
	if (playback_handle == NULL)
		fprintf(stderr, "%s: Alsa device not open (playback)\n", HARDWARE_NAME);
	else {
		snd_pcm_close(playback_handle);
		playback_handle = NULL;
	}

	return;
}

static int alsa_write_audio(uint16_t *buf, size_t sample_count) {
	int ret = 0;
	int err;

	dbg_printf(2, "%s: Number of samples to write: %u\n", HARDWARE_NAME, sample_count);

	if (playback_handle == NULL) {
		fprintf(stderr, "%s: Alsa device not open\n", HARDWARE_NAME);
		return -1;
	}

	if (alsa_configure_device(playback_handle, sample_count) < 0) {
		fprintf(stderr, "%s: device configuration failed\n", HARDWARE_NAME);
		return -1;
	}

	if ((err = snd_pcm_prepare(playback_handle)) < 0) {
		fprintf(stderr, "%s: cannot prepare audio interface for use (%s)\n", HARDWARE_NAME,
			snd_strerror (err));
		return -1;
	}

	if ((err = snd_pcm_writei(playback_handle, buf, sample_count/SAMPLES_PER_FRAME)) != sample_count/SAMPLES_PER_FRAME) {
		fprintf(stderr, "%s: Write error (%s)\n", HARDWARE_NAME, snd_strerror (err));
	}

	snd_pcm_drain(playback_handle);

	return ret;
}

static uint16_t * alsa_convert_frame_to_audio(size_t *sample_count, struct timing_config *config, uint8_t *src_frame, size_t src_bit_count)
{
	int ret, i, j, k;
	uint16_t *dest_frame;
	size_t count;
	uint32_t samples_per_bit = round((config->base_time * samplerate)/1000000.0f);
	size_t samples_per_rf_frame = src_bit_count * samples_per_bit * channels;

	dbg_printf(2, "%s: Samples per bit: %u\n", HARDWARE_NAME, samples_per_bit);

	*sample_count = samples_per_rf_frame * config->frame_count;
	if (*sample_count < 16000) {
		/* Be sure the final buffer will be big enough to prevent underruns */
		*sample_count = 16000;
	}

	dest_frame = (uint16_t *) malloc(sizeof(uint16_t) * (*sample_count));
	if(!dest_frame) {
		fprintf(stderr, "%s: cannot allocate memory for audio buffer\n", HARDWARE_NAME);
		return NULL;
	}

	memset(dest_frame, 0, sizeof(uint16_t) * (*sample_count));

	for (k = 0; k < config->frame_count; k++) {
		for (i = 0; i < src_bit_count; i++) {
			if (src_frame[i/8] & (1 << (7 - (i % 8)))) {
				for (j = 0; j < samples_per_bit; j++) {
					dest_frame[k * samples_per_rf_frame + 2 * (i * samples_per_bit + j) + 1] = 0x8000;
				}
			} else {
				for (j = 0; j < samples_per_bit; j++) {
					dest_frame[k * samples_per_rf_frame + 2 * (i * samples_per_bit + j) + 1] = 0x7FFF;
				}
			}
		}
	}

	return dest_frame;
}

static int alsa_probe(void) {
	/* This driver cannot be auto-detected */
	return -1;
}

static int alsa_init(struct rf_hardware_params *params) {
	return alsa_open_playback(ALSA_DEVICE_OUT);
}

static void alsa_close(void) {
	alsa_close_playback();
}

static int alsa_send_cmd(struct timing_config *config, uint8_t *frame_data, uint16_t bit_count) {
	int ret = -1;
	size_t sample_count = 0;
	uint16_t *audio_data = alsa_convert_frame_to_audio(&sample_count, config, frame_data, bit_count);

	if (audio_data != NULL) {
		ret = alsa_write_audio(audio_data, sample_count);
		free(audio_data);
	}

	return ret;
}

struct rf_hardware_driver alsa_driver = {
	.name = HARDWARE_NAME,
	.cmd_name = "alsa",
	.long_name = "Alsa 433MHz Differential RF Transceiver",
	.supported_bit_fmts = (1 << RF_BIT_FMT_RAW),
	.probe = &alsa_probe,
	.init = &alsa_init,
	.close = &alsa_close,
	.send_cmd = &alsa_send_cmd,
};
