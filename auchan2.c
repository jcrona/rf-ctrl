/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of the new Auchan wireless plugs
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

#define PROTOCOL_NAME	"Auchan 2"


static int auchan2_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 21;
	uint8_t raw_cmd; 		// 2 bits
	uint8_t crc = 0; 		// 2 bits
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

	data[0] = (remote_code & 0x1FFF) >> 5;
	data[1] = ((remote_code & 0x1F) << 3) | ((device_code & 0x1) << 2) | (device_code & 0x2) | ((raw_cmd & 0x3) >> 1);
	data[2] = ((raw_cmd & 0x1) << 7);

	/* 2 bits XOR of the frame by 2-bits steps (the first bit of the frame will be alone) */
	for (i = 0; i < bit_count - 2; i++) {
		crc ^= ((data[i/8] >> (7 - (i % 8))) & 0x1) << (i % 2);
	}

	data[2] |= ((crc & 0x3) << 3);

	return bit_count;
}

static struct timing_config auchan2_timings = {
	.start_bit_h_time = 0,		// 0 us
	.start_bit_l_time = 0,		// 0 us
	.end_bit_h_time = 0,		// 0 us
	.end_bit_l_time = 0,		// 0 us
	.data_bit0_h_time = 1320,	// 1320 us
	.data_bit0_l_time = 680,	// 680 us
	.data_bit1_h_time = 680,	// 680 us
	.data_bit1_l_time = 1320,	// 1320 us
	.bit_fmt = RF_BIT_FMT_LH,
	.frame_count = 10,
};

struct rf_protocol_driver auchan2_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "auchan2",
	.timings = &auchan2_timings,
	.format_cmd = &auchan2_format_cmd,
	.remote_code_max = 0x1FFF,
	.device_code_max = 0x3,
	.needed_params = PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND,
};
