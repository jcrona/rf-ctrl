/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of OTAX wireless plugs
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

#define PROTOCOL_NAME	"OTAX"


static int otax_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 25;
	uint8_t raw_cmd; 		// 4 bits
	uint16_t raw_device_code = 0;	// 10 bits
	uint16_t raw_remote_code = 0;	// 10 bits
	int i;

	if (data_len * 8 < bit_count) {
		fprintf(stderr, "%s: data buffer too small (%lu available, %d needed)\n", PROTOCOL_NAME, (unsigned long) data_len, (bit_count + 7)/8);
		return -1;
	}

	switch (command) {
		case RF_CMD_OFF:
			raw_cmd = 0x01;
			break;

		case RF_CMD_ON:
			raw_cmd = 0x04;
			break;

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	/*
	 * Bits of the remote_code remains the same with
	 * a 1 added after each bit (0 -> 01, 1 -> 11)
	 */
	for (i = 0; i < 5; i++) {
		if (remote_code & (0x1 << i)) {
			raw_remote_code |= 0x3 << (2 * i);
		} else {
			raw_remote_code |= 0x1 << (2 * i);
		}
	}

	/*
	 * Bits of the device_code are inverted with a
	 * 0 added before each bit (0 -> 01, 1 -> 00)
	 */
	for (i = 0; i < 5; i++) {
		if (!(device_code & (0x1 << i))) {
			raw_device_code |= 0x1 << (2 * i);
		}
	}

	data[0] = (raw_remote_code & 0x3FF) >> 2;
	data[1] = ((raw_remote_code & 0x3) << 6) | ((raw_device_code & 0x3FF) >> 4);
	data[2] = ((raw_device_code & 0xF) << 4) | (raw_cmd & 0xF);
	data[3] = 0;

	return bit_count;
}

static struct timing_config otax_timings = {
	.start_bit_h_time = 0,		// 0 us
	.start_bit_l_time = 4860,	// 4860 us
	.end_bit_h_time = 0,		// 0 us
	.end_bit_l_time = 0,		// 0 us
	.data_bit0_h_time = 160,	// 160 us
	.data_bit0_l_time = 420,	// 420 us
	.data_bit1_h_time = 420,	// 420 us
	.data_bit1_l_time = 160,	// 160 us
	.bit_fmt = RF_BIT_FMT_HL,
	.frame_count = 10,
};

struct rf_protocol_driver otax_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "otax",
	.timings = &otax_timings,
	.format_cmd = &otax_format_cmd,
	.remote_code_max = 0x1F,
	.device_code_max = 0x1F,
	.needed_params = PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND,
};
