/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of DI-O wireless plugs
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

#define PROTOCOL_NAME	"DI-O"


static int dio_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 64;
	uint8_t buf[4];
	uint8_t raw_cmd;
	int i;

	if (data_len * 8 < bit_count) {
		fprintf(stderr, "%s: data buffer too small (%lu available, %d needed)\n", PROTOCOL_NAME, (unsigned long) data_len, (bit_count + 7)/8);
		return -1;
	}

	switch (command) {
		case RF_CMD_OFF:
			raw_cmd = 0x00;
			break;

		case RF_CMD_ON:
			raw_cmd = 0x01;
			break;

		case RF_CMD_GOFF:
			raw_cmd = 0x02;
			break;

		case RF_CMD_GON:
			raw_cmd = 0x03;
			break;

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	buf[0] = (remote_code >> 18) & 0xFF;
	buf[1] = (remote_code >> 10) & 0xFF;
	buf[2] = (remote_code >> 2) & 0xFF;
	buf[3] = ((remote_code & 0x03) << 6 ) | ((raw_cmd & 0x03) << 4) | (device_code & 0x0F);

	/* Manchester */
	memset(data, 0, data_len);
	for (i = 0; i < 32; i++) {
		if (buf[i/8] & (0x01 << (i % 8))) {
			data[(i/8) * 2 + 1 - ((i/4) % 2)] |= 0x02 << ((i % 4) * 2);
		} else {
			data[(i/8) * 2 + 1 - ((i/4) % 2)] |= 0x01 << ((i % 4) * 2);
		}
	}

	return bit_count;
}

static struct timing_config dio_timings = {
	.start_bit_h_time = 260,	// 260 us
	.start_bit_l_time = 2680,	// 2680 us
	.end_bit_h_time = 260,		// 260 us
	.end_bit_l_time = 9000,		// 9000 us
	.data_bit0_h_time = 260,	// 260 us
	.data_bit0_l_time = 260,	// 260 us
	.data_bit1_h_time = 260,	// 260 us
	.data_bit1_l_time = 1300,	// 1300 us
	.bit_fmt = RF_BIT_FMT_HL,
	.frame_count = 7,
};

struct rf_protocol_driver dio_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "dio",
	.timings = &dio_timings,
	.format_cmd = &dio_format_cmd,
	.remote_code_max = 0x3FFFFFF,
	.device_code_max = 0x0F,
	.needed_params = PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND,
};
