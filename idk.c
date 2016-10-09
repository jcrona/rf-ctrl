/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of Idk wireless chimes
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

#define PROTOCOL_NAME	"Idk"


static int idk_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const uint16_t raw_zero = 0x5555; // 16 bits
	const int bit_count = 25;
	uint8_t raw_device_code = 0;
	int i;

	if (data_len * 8 < bit_count) {
		fprintf(stderr, "%s: data buffer too small (%lu available, %d needed)\n", PROTOCOL_NAME, (unsigned long) data_len, (bit_count + 7)/8);
		return -1;
	}

	switch (command) {
		case RF_CMD_ON:
			break;

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	/*
	 * Bits remains the same with a 1 added
	 * after each bit (0 -> 01, 1 -> 11)
	 */
	for (i = 0; i < 8; i++) {
		if (device_code & (0x1 << i)) {
			raw_device_code |= 0x3 << (2 * i);
		} else {
			raw_device_code |= 0x1 << (2 * i);
		}
	}

	data[0] = raw_device_code & 0xFF;
	data[1] = (raw_zero & 0xFFFF) >> 8;
	data[2] = raw_zero & 0xFF;
	data[3] = raw_zero;

	return bit_count;
}

static struct timing_config idk_timings = {
	.start_bit_h_time = 0,		// 0 us
	.start_bit_l_time = 7440,	// 7440 us
	.end_bit_h_time = 0,		// 0 us
	.end_bit_l_time = 0,		// 0 us
	.data_bit0_h_time = 220,	// 220 us
	.data_bit0_l_time = 800,	// 800 us
	.data_bit1_h_time = 700,	// 700 us
	.data_bit1_l_time = 320,	// 320 us
	.bit_fmt = RF_BIT_FMT_HL,
	.frame_count = 20,
};

struct rf_protocol_driver idk_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "idk",
	.timings = &idk_timings,
	.format_cmd = &idk_format_cmd,
	.remote_code_max = 0x00,
	.device_code_max = 0xF,
	.needed_params = PARAM_DEVICE_ID | PARAM_COMMAND,
};
