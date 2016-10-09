/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of Sumtech wireless plugs
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

#define PROTOCOL_NAME	"Sumtech"


static int sumtech_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 32;
	uint8_t raw_cmd;

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

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	data[0] = (remote_code >> 16) & 0xFF;
	data[1] = (remote_code >> 8) & 0xFF;
	data[2] = remote_code & 0xFF;
	data[3] = ((raw_cmd & 0x01) << 7 ) | (device_code & 0x7F);

	return bit_count;
}

static struct timing_config sumtech_timings = {
	.start_bit_h_time = 4940,	// 4940 us
	.start_bit_l_time = 9640,	// 9640 us
	.end_bit_h_time = 0,		// 0 us
	.end_bit_l_time = 0,		// 0 us
	.data_bit0_h_time = 1200,	// 1200 us
	.data_bit0_l_time = 400,	// 400 us
	.data_bit1_h_time = 400,	// 400 us
	.data_bit1_l_time = 1200,	// 1200 us
	.bit_fmt = RF_BIT_FMT_LH,
	.frame_count = 10,
};

struct rf_protocol_driver sumtech_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "sumtech",
	.timings = &sumtech_timings,
	.format_cmd = &sumtech_format_cmd,
	.remote_code_max = 0xFFFFFF,
	.device_code_max = 0x7F,
	.needed_params = PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND,
};
