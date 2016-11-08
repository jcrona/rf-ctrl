/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Reversed protocol of Blyss wireless plugs
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

#define PROTOCOL_NAME			"Blyss"

#define FRAME_TYPE_SWITCH		0xFE;
#define FRAME_TYPE_TEMP_HUM		0xE5;

struct rf_protocol_driver blyss_driver;

uint8_t rolling_code_table[5] = {0x98, 0xDA, 0x1E, 0xE6, 0x67};


static int blyss_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 52;
	uint8_t raw_cmd; 		// 1 bit
	uint8_t rolling_code_idx = 0;
	uint8_t timestamp = 0;
	uint8_t channel = 0;		// 4 bits
	char data_file_path[STORAGE_PATH_MAX_LEN];
	FILE * f;

	if (data_len * 8 < bit_count) {
		fprintf(stderr, "%s: data buffer too small (%lu available, %d needed)\n", PROTOCOL_NAME, (unsigned long) data_len, (bit_count + 7)/8);
		return -1;
	}

	switch (command) {
		case RF_CMD_OFF:
			raw_cmd = 0x1;
			break;

		case RF_CMD_ON:
			raw_cmd = 0x0;
			break;

		case RF_CMD_GOFF:
			raw_cmd = 0x1;
			device_code = 0;
			break;

		case RF_CMD_GON:
			raw_cmd = 0x0;
			device_code = 0;
			break;

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	/* Get the current rolling code for that device, remote, and channel */
	get_storage_path(data_file_path, &blyss_driver);
	snprintf(data_file_path + strlen(data_file_path), STORAGE_PATH_MAX_LEN, "/%02X.%06X", remote_code, device_code);

	dbg_printf(3, "%s: Data file is %s\n", PROTOCOL_NAME, data_file_path);

	f = fopen(data_file_path, "r+");
	if (f == NULL) {
		f = fopen(data_file_path, "w+");
		if (f == NULL) {
			fprintf(stderr, "%s: Cannot open %s\n", PROTOCOL_NAME, data_file_path);
			return -1;
		}
	}

	if (fscanf(f, "%01hhX", &rolling_code_idx) != 1) {
		dbg_printf(2, "%s: Rolling code initialized to 0\n", PROTOCOL_NAME);
	}

	fseek(f, 0, SEEK_SET);
	fprintf(f, "%01X\n", (rolling_code_idx + 1) % 5);

	fclose(f);

	dbg_printf(1, "%s: Rolling code index at %u (0x%02X)\n", PROTOCOL_NAME, rolling_code_idx, rolling_code_table[rolling_code_idx]);

	/* We do not really care about this one as long as it is changing */
	timestamp = ~rolling_code_table[rolling_code_idx];

	/* Use the first 4 bits of remote_code as channel */
	channel = (remote_code >> 16) & 0xF;
	remote_code &= 0xFFFF;

	data[0] = FRAME_TYPE_SWITCH;
	data[1] = ((channel & 0x0F) << 4) | ((remote_code >> 12) & 0x0F);
	data[2] = (remote_code >> 4) & 0xFF;
	data[3] = ((remote_code & 0x0F) << 4 ) | (device_code & 0x0F);
	data[4] = ((raw_cmd & 0x0F) << 4 ) | ((rolling_code_table[rolling_code_idx] >> 4) & 0x0F);
	data[5] = ((rolling_code_table[rolling_code_idx] & 0x0F) << 4) | ((timestamp >> 4) & 0x0F);
	data[6] = (timestamp & 0x0F) << 4;

	return bit_count;
}

static struct timing_config blyss_timings = {
	.start_bit_h_time = 2400,	// 2400 us
	.start_bit_l_time = 0,		// 0 us
	.end_bit_h_time = 0,		// 0 us
	.end_bit_l_time = 24000,	// 24000 us
	.data_bit0_h_time = 400,	// 400 us
	.data_bit0_l_time = 800,	// 800 us
	.data_bit1_h_time = 800,	// 800 us
	.data_bit1_l_time = 400,	// 400 us
	.bit_fmt = RF_BIT_FMT_LH,
	.frame_count = 10,
};

struct rf_protocol_driver blyss_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "blyss",
	.timings = &blyss_timings,
	.format_cmd = &blyss_format_cmd,
	.remote_code_max = 0xFFFFF,
	.device_code_max = 0xF,
	.needed_params = PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND,
};
