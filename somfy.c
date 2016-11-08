/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Somfy protocol implementation (carrier frequency is 433.42MHz)
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

#define PROTOCOL_NAME		"Somfy RTS"

#define SOMFY_FRAME_COUNT	10

struct rf_protocol_driver somfy_driver;


static size_t write_low(uint8_t *buf, size_t offset, uint8_t length) {
	size_t i;

	for (i = offset; i < (offset + length); i++) {
		buf[i/8] &= ~(1 << (7 - (i % 8)));
	}

	return length;
}

static size_t write_high(uint8_t *buf, size_t offset, uint8_t length) {
	size_t i;

	for (i = offset; i < (offset + length); i++) {
		buf[i/8] |= 1 << (7 - (i % 8));
	}

	return length;
}

static size_t write_bit_zero(uint8_t *buf, size_t offset) {
	size_t count = offset;

	/* High, then low */
	count += write_high(buf, count, 1);
	count += write_low(buf, count, 1);

	return (count - offset);
}

static size_t write_bit_one(uint8_t *buf, size_t offset) {
	size_t count = offset;

	/* Low, then high */
	count += write_low(buf, count, 1);
	count += write_high(buf, count, 1);

	return (count - offset);
}

static size_t write_pulses(uint8_t *buf, size_t offset, uint8_t pulses) {
	size_t count = offset;
	uint8_t i;

	/* Starting pulses (4 x high, 4 x low) */
	for (i = 0; i < pulses; i++) {
		count += write_high(buf, count, 4);
		count += write_low(buf, count, 4);
	}

	/* Sync pulse (8 x high, 1 x low) */
	count += write_high(buf, count, 8);
	count += write_low(buf, count, 1);

	return (count - offset);
}

static size_t write_bits(uint8_t *buf, size_t offset, uint8_t *data, size_t data_len) {
	size_t count = offset;
	size_t i;

	for (i = 0; i < (data_len * 8); i++) {
		if ((data[i/8] & (1 << (7 - (i % 8)))) != 0) {
			count += write_bit_one(buf, count);
		} else {
			count += write_bit_zero(buf, count);
		}
	}

	return (count - offset);
}

static size_t write_data(uint8_t *buf, size_t offset, uint16_t rolling_code, uint8_t key, uint32_t address, uint8_t command) {
	size_t count = offset;
	uint8_t data[7];
	uint8_t checksum = 0;
	uint8_t i;

	/*
	 * |  0  |     1      |   2     3    | 4   5   6 |
	 * | key | cmd/chksum | rolling code |  address  |
	 */

	data[0] = key & 0xFF;
	data[1] = (command << 4) & 0xF0;
	data[2] = (rolling_code >> 8) & 0xFF;
	data[3] = rolling_code & 0xFF;
	data[4] = address & 0xFF;
	data[5] = (address >> 8) & 0xFF;
	data[6] = (address >> 16) & 0xFF;

	/* Checksum */
	for (i = 0; i < 7; i++) {
		checksum = checksum ^ data[i] ^ (data[i] >> 4);
	}
	data[1] |= checksum & 0x0F;

	/* Encryption */
	for (i = 1; i < 7; i++) {
		data[i] = data[i] ^ data[i - 1];
	}

	count += write_bits(buf, count, data, 7);

	return (count - offset);
}

static int somfy_format_cmd(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command) {
	const int bit_count = 213 + (SOMFY_FRAME_COUNT - 1) * 225;
	uint8_t raw_cmd; 		// 4 bits
	uint16_t rolling_code = 0;
	char data_file_path[STORAGE_PATH_MAX_LEN];
	FILE * f;
        size_t count = 0;
	uint8_t i;

	if (data_len * 8 < bit_count) {
		fprintf(stderr, "%s: data buffer too small (%lu available, %d needed)\n", PROTOCOL_NAME, (unsigned long) data_len, (bit_count + 7)/8);
		return -1;
	}

	switch (command) {
		case RF_CMD_OFF:
			raw_cmd = 0x04;
			break;

		case RF_CMD_ON:
			raw_cmd = 0x02;
			break;

		case RF_CMD_PROG:
			raw_cmd = 0x08;
			break;

		case RF_CMD_F1:		// My/Dim button
			raw_cmd = 0x01;
			break;

		default:
			fprintf(stderr, "%s: Unsupported command %d\n", PROTOCOL_NAME, (int) command);
			return -1;
	}

	if (device_code < 0x400) {
		printf("%s: Device ID is less than 0x400, it might not be recognized/allowed by the device\n", PROTOCOL_NAME);
	}

	/* Get the current rolling code for that device and remote */
	get_storage_path(data_file_path, &somfy_driver);
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

	if (fscanf(f, "%04hX", &rolling_code) != 1) {
		dbg_printf(2, "%s: Rolling code initialized to 0\n", PROTOCOL_NAME);
	}

	fseek(f, 0, SEEK_SET);
	fprintf(f, "%04X\n", (rolling_code + 1) % 0x10000);

	fclose(f);

	dbg_printf(1, "%s: Rolling code at %04X\n", PROTOCOL_NAME, rolling_code);

	/* Generate the whole frame */
	for (i = 0; i < SOMFY_FRAME_COUNT; i++) {
		/* Starting/sync pulses */
		if (i == 0) {
			/* First frame */
			/* Big starting pulse (16 x high, 12 x low) */
			count += write_high(data, count, 16);
			count += write_low(data, count, 12);

			/* Then 2 regular starting pulses */
			count += write_pulses(data, count, 2);
		} else {
			/* Following frames (7 regular starting pulses) */
			count += write_pulses(data, count, 7);
		}

		/* Actual data */
		count += write_data(data, count, rolling_code, remote_code, device_code, raw_cmd);

		/* Gap before the next frame (48 x low = ~30ms) */
		count += write_low(data, count, 48);
	}

	return bit_count;
}

static struct timing_config somfy_timings = {
	.base_time = 625,		// 625 us
	.bit_fmt = RF_BIT_FMT_RAW,
	.frame_count = 1,		// the sequence of frames is generated as one
};

struct rf_protocol_driver somfy_driver = {
	.name = PROTOCOL_NAME,
	.cmd_name = "somfy",
	.timings = &somfy_timings,
	.format_cmd = &somfy_format_cmd,
	.remote_code_max = 0xFF,
	.device_code_max = 0xFFFFFF,
	.needed_params = PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND,
};
