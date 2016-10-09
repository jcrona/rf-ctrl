/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of Home Easy wireless plugs (UNTESTED)
 * Using protocol from https://github.com/r10r/he853-remote
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

#define PROTOCOL_NAME	"Home Easy"


static int he_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 57;
	uint8_t tb_fx[16] = { 0x07, 0x0b, 0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c, 0x03, 0x05, 0x06, 0x09, 0x0a, 0x0c };
	uint8_t buf[4] = { 0x00, (uint8_t) ((device_code >> 8) & 0xff), (uint8_t) (device_code & 0xff), 0x00 };
	int i;

	if (data_len * 8 < bit_count) {
		fprintf(stderr, "%s: data buffer too small (%lu available, %d needed)\n", PROTOCOL_NAME, (unsigned long) data_len, (bit_count + 7)/8);
		return -1;
	}

	switch (command) {
		case RF_CMD_OFF:
			break;

		case RF_CMD_ON:
			buf[3] |= 0x10;
			break;

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	uint8_t gbuf[8] = { (uint8_t) (buf[0] >> 4),
 			    (uint8_t) (buf[0] & 15),
			    (uint8_t) (buf[1] >> 4),
			    (uint8_t) (buf[1] & 15),
			    (uint8_t) (buf[2] >> 4),
			    (uint8_t) (buf[2] & 15),
			    (uint8_t) (buf[3] >> 4),
			    (uint8_t) (buf[3] & 15) };

	uint8_t kbuf[8];
	for (i = 0; i < 8; i++) {
		kbuf[i] = (uint8_t) ((tb_fx[gbuf[i]] | 0x40) & 0x7f);
	}
	kbuf[0] |= 0x80;

	uint64_t t64 = 0;
	t64 = kbuf[0];
	for (i = 1; i < 8; i++)
	{
		t64 = (t64 << 7) | kbuf[i];
	}
	t64 = t64 << 7;

	data[0] = (uint8_t) (t64 >> 56);
	data[1] = (uint8_t) (t64 >> 48);
	data[2] = (uint8_t) (t64 >> 40);
	data[3] = (uint8_t) (t64 >> 32);
	data[4] = (uint8_t) (t64 >> 24);
	data[5] = (uint8_t) (t64 >> 16);
	data[6] = (uint8_t) (t64 >> 8);
	data[7] = (uint8_t) t64;

	return bit_count;
}

static struct timing_config he_timings = {
	.start_bit_h_time = 260,	// 260 us
	.start_bit_l_time = 8600,	// 8600 us
	.end_bit_h_time = 0,		// 0 us
	.end_bit_l_time = 0,		// 0 us
	.data_bit0_h_time = 260,	// 260 us
	.data_bit0_l_time = 260,	// 260 us
	.data_bit1_h_time = 260,	// 260 us
	.data_bit1_l_time = 1300,	// 1300 us
	.bit_fmt = RF_BIT_FMT_HL,
	.frame_count = 7,
};

struct rf_protocol_driver he_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "he",
	.timings = &he_timings,
	.format_cmd = &he_format_cmd,
	.remote_code_max = 0x00,
	.device_code_max = 0xFFFF,
	.needed_params = PARAM_DEVICE_ID | PARAM_COMMAND,
};
