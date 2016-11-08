/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * ELRO/HomeEasy HE853 USB Dongle driver
 * Thanks to https://github.com/r10r/he853-remote for the HE853 USB commands
 *
 * Copyright (C) 2016 Jean-Christophe Rona <jc@rona.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rf-ctrl.h"
#include "hidapi.h"

#define HARDWARE_NAME		"HE853"

#define HE853_PID		0x1357
#define HE853_VID		0x04d9

#define HE853_CMD_TIMING1	0x01
#define HE853_CMD_TIMING2	0x02
#define HE853_CMD_DATA1		0x03
#define HE853_CMD_DATA2		0x04
#define HE853_CMD_EXECUTE	0x05

#define HE853_DATA_LEN_MAX	14

#define MAX_HID_WRITE_ATTEMPT	5

hid_device *handle = NULL;


static int he853_probe(void) {
	hid_device *h;

	h = hid_open(HE853_VID, HE853_PID, NULL);

	if (!h) {
		dbg_printf(2, "%s not detected\n", HARDWARE_NAME);
		return -1;
	}

	hid_close(h);

	dbg_printf(2, "%s detected\n", HARDWARE_NAME);

	return 0;
}

static int he853_init(void) {
	if (handle != NULL) {
		dbg_printf(1, "%s device already initialized\n", HARDWARE_NAME);
		return 0;
	}

	handle = hid_open(HE853_VID, HE853_PID, NULL);

	if (!handle) {
		fprintf(stderr, "%s: Unable to open device\n", HARDWARE_NAME);
		return -1;
	}

	return 0;
}

static void he853_close(void) {
	hid_close(handle);

	handle = NULL;
}

static int he853_send_hid_report(uint8_t* buf) {
	uint8_t obuf[9];
	int ret = 0;
	int i, retry_count = 0;

	obuf[0] = 0x00; // report id = 0, as it seems to be the only report

	for (i = 0; i < 8; i++) {
		obuf[i + 1] = buf[i];
	}

	if (is_dbg_enabled(2)) {
		dbg_printf(2, "  %s HID report: ", HARDWARE_NAME);
		for (i = 0; i < 9; i++) {
			dbg_printf(2, "%02X ", obuf[i]);
		}
		dbg_printf(2, "\n");
	}

	while (retry_count < MAX_HID_WRITE_ATTEMPT) {
		ret = hid_write(handle, obuf, 9);
		if (ret < 0) {
			retry_count++;
			dbg_printf(2, "%s: Warning, hid_write failed, retrying (%d)...\n", HARDWARE_NAME, retry_count);
		} else {
			break;
		}
	}

	if (ret < 0) {
		fprintf(stderr, "%s: Cannot send HID report, hid_write failed (id %d, command %d) !\n", HARDWARE_NAME, obuf[0], obuf[1]);
	}

	return ret;
}

static int he853_send_rf_frame(void) {
	uint8_t cmd_buf[8];

	memset(cmd_buf, 0x00, sizeof(cmd_buf));
	cmd_buf[0] = HE853_CMD_EXECUTE;

	return he853_send_hid_report(cmd_buf);
}

/*
 * The HE853 dongle is supposed to take timing values
 * in 10 us unit, but it appears it does not work for
 * "low" ( < 9000 us) values, and the resulting timings
 * also differ between H and L values.
 */
static uint16_t to_he853_htime(uint16_t usec) {
	switch (usec/10) {
		case 0:
			return 0x00;

		case 16:
			return 0x25;

		case 22:
			return 0x2A;

		case 26:
			return 0x2D;

		case 40:
			return 0x30;

		case 42:
			return 0x33;

		case 70:
			return 0x5C;

		case 110:
			return 0x76;

		default:
			fprintf(stderr, "%s: %d us is not supported as H time !\n", HARDWARE_NAME, usec);
			return 0;
	}
}

static uint16_t to_he853_ltime(uint16_t usec) {
	switch (usec/10) {
		case 0:
			return 0x00;

		case 16:
			return 0x04;

		case 26:
			return 0x09;

		case 32:
			return 0x0C;

		case 40:
			return 0x1F;

		case 42:
			return 0x18;

		case 80:
			return 0x3C;

		case 110:
			return 0x63;

		case 130:
			return 0x68;

		case 268:
			return 0xF8;

		case 486:
			return 0x01E0;

		case 744:
			return 0x02E3;

		case 860:
			return 0x035A;

		case 900:
			return 0x0384;

		case 1040:
			return 0x0410;

		default:
			fprintf(stderr, "%s: %d us is not supported as L time !\n", HARDWARE_NAME, usec);
			return 0;
	}
}

static int he853_configure(struct timing_config *conf, uint8_t *frame_data, uint8_t bit_count) {
	uint8_t cmd_buf[32];
	uint8_t frame_len = (bit_count + 7)/8;
	uint16_t sbit_htime;
	uint16_t sbit_ltime;
	uint16_t ebit_htime;
	uint16_t ebit_ltime;
	uint8_t dbit0_htime;
	uint8_t dbit0_ltime;
	uint8_t dbit1_htime;
	uint8_t dbit1_ltime;
	int i;

	if (conf->bit_fmt != RF_BIT_FMT_HL) {
			fprintf(stderr, "%s: Bit format %s is not supported !\n", HARDWARE_NAME, rf_bit_fmt_str[(int) conf->bit_fmt]);
			return -1;
	}

	/* Convert from real timings to HE853 timings */
	sbit_htime = to_he853_htime(conf->start_bit_h_time);
	sbit_ltime = to_he853_ltime(conf->start_bit_l_time);
	ebit_htime = to_he853_htime(conf->end_bit_h_time);
	ebit_ltime = to_he853_ltime(conf->end_bit_l_time);
	dbit0_htime = to_he853_htime(conf->data_bit0_h_time);
	dbit0_ltime = to_he853_ltime(conf->data_bit0_l_time);
	dbit1_htime = to_he853_htime(conf->data_bit1_h_time);
	dbit1_ltime = to_he853_ltime(conf->data_bit1_l_time);

	/* The HE853 dongle needs a positive H time in order to process the L time */
	if (sbit_ltime != 0 && sbit_htime == 0) {
		sbit_htime = 1;
	}
	if (ebit_ltime != 0 && ebit_htime == 0) {
		ebit_htime = 1;
	}
	if (dbit0_ltime != 0 && dbit0_htime == 0) {
		dbit0_htime = 1;
	}
	if (dbit1_ltime != 0 && dbit1_htime == 0) {
		dbit1_htime = 1;
	}

	cmd_buf[0] = HE853_CMD_TIMING1;
	cmd_buf[1] = (uint8_t) (sbit_htime >> 8);
	cmd_buf[2] = (uint8_t) (sbit_htime);
	cmd_buf[3] = (uint8_t) (sbit_ltime >> 8);
	cmd_buf[4] = (uint8_t) (sbit_ltime);
	cmd_buf[5] = (uint8_t) (ebit_htime >> 8);
	cmd_buf[6] = (uint8_t) (ebit_htime);
	cmd_buf[7] = (uint8_t) (ebit_ltime >> 8);

	cmd_buf[8] = HE853_CMD_TIMING2;
	cmd_buf[9] = (uint8_t) (ebit_ltime);
	cmd_buf[10] = dbit0_htime;
	cmd_buf[11] = dbit0_ltime;
	cmd_buf[12] = dbit1_htime;
	cmd_buf[13] = dbit1_ltime;
	cmd_buf[14] = (uint8_t) bit_count;
	cmd_buf[15] = (uint8_t) (conf->frame_count);

	cmd_buf[16] = HE853_CMD_DATA1;
	for (i = 0; i < 7; i++) {
		if (i < frame_len) {
			cmd_buf[17 + i] = frame_data[i];
		} else {
			cmd_buf[17 + i] = 0x00;
		}
	}

	cmd_buf[24] = HE853_CMD_DATA2;
	for (i = 0; i < 7; i++) {
		if (i + 7 < frame_len) {
			cmd_buf[25 + i] = frame_data[i + 7];
		} else {
			cmd_buf[25 + i] = 0x00;
		}
	}

	return he853_send_hid_report(cmd_buf)
		&& he853_send_hid_report(cmd_buf+8)
		&& he853_send_hid_report(cmd_buf+16)
		&& he853_send_hid_report(cmd_buf+24);
}

static int he853_send_cmd(struct timing_config *config, uint8_t *frame_data, uint16_t bit_count) {
	int ret = 0;

	if (bit_count > 255) {
		fprintf(stderr, "%s: Frame is too long !\n", HARDWARE_NAME);
		return -1;
	}

	ret = he853_configure(config, frame_data, (uint8_t) bit_count);
	if (ret < 0) {
		fprintf(stderr, "%s configuration failed\n", HARDWARE_NAME);
		return ret;
	}

	return he853_send_rf_frame();
}

struct rf_hardware_driver he853_driver = {
	.name = HARDWARE_NAME,
	.cmd_name = "he853",
	.long_name = "HE853 USB RF dongle",
	.probe = &he853_probe,
	.init = &he853_init,
	.close = &he853_close,
	.send_cmd = &he853_send_cmd,
};
