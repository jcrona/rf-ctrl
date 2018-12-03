/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Dummy driver for test purpose
 *
 * Copyright (C) 2018 Jean-Christophe Rona <jc@rona.fr>
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

#define HARDWARE_NAME			"Dummy HW"

#define BASE_TIME_HL			100 // us
#define H_CHAR				0x18
#define L_CHAR				0x5F


static int dummy_probe(void) {
	/* Prevent from being auto-detected */
	return -1;
}

static int dummy_init(void) {
	return 0;
}

static void dummy_close(void) {
}

static void dbg_printnc(int level, char c, uint16_t n) {
	uint16_t i;

	for (i = 0; i < n; i++) {
		dbg_printf(level, "%c", c);
	}
}

static int dummy_send_cmd(struct timing_config *config, uint8_t *frame_data, uint16_t bit_count) {
	uint16_t i;

	if (!is_dbg_enabled(1)) {
		return 0;
	}

	dbg_printf(1, "%s: Count = %u\n", HARDWARE_NAME, config->frame_count);

	dbg_printf(1, "%s: Frame = ", HARDWARE_NAME);
	switch (config->bit_fmt) {
		case RF_BIT_FMT_HL:
			dbg_printnc(1, H_CHAR, config->start_bit_h_time/BASE_TIME_HL);
			dbg_printnc(1, L_CHAR, config->start_bit_l_time/BASE_TIME_HL);
			break;

		case RF_BIT_FMT_LH:
			dbg_printnc(1, L_CHAR, config->start_bit_l_time/BASE_TIME_HL);
			dbg_printnc(1, H_CHAR, config->start_bit_h_time/BASE_TIME_HL);
			break;
	}

	for (i = 0; i < bit_count; i++) {
		if (frame_data[i/8] & (1 << (7 - (i % 8)))) {
			switch (config->bit_fmt) {
				case RF_BIT_FMT_HL:
					dbg_printnc(1, H_CHAR, config->data_bit1_h_time/BASE_TIME_HL);
					dbg_printnc(1, L_CHAR, config->data_bit1_l_time/BASE_TIME_HL);
					break;

				case RF_BIT_FMT_LH:
					dbg_printnc(1, L_CHAR, config->data_bit1_l_time/BASE_TIME_HL);
					dbg_printnc(1, H_CHAR, config->data_bit1_h_time/BASE_TIME_HL);
					break;

				case RF_BIT_FMT_RAW:
					dbg_printnc(1, H_CHAR, 1);
					break;

			}
		} else {
			switch (config->bit_fmt) {
				case RF_BIT_FMT_HL:
					dbg_printnc(1, H_CHAR, config->data_bit0_h_time/BASE_TIME_HL);
					dbg_printnc(1, L_CHAR, config->data_bit0_l_time/BASE_TIME_HL);
					break;

				case RF_BIT_FMT_LH:
					dbg_printnc(1, L_CHAR, config->data_bit0_l_time/BASE_TIME_HL);
					dbg_printnc(1, H_CHAR, config->data_bit0_h_time/BASE_TIME_HL);
					break;

				case RF_BIT_FMT_RAW:
					dbg_printnc(1, L_CHAR, 1);
					break;

			}
		}
	}

	switch (config->bit_fmt) {
		case RF_BIT_FMT_HL:
			dbg_printnc(1, H_CHAR, config->end_bit_h_time/BASE_TIME_HL);
			dbg_printnc(1, L_CHAR, config->end_bit_l_time/BASE_TIME_HL);
			break;

		case RF_BIT_FMT_LH:
			dbg_printnc(1, L_CHAR, config->end_bit_l_time/BASE_TIME_HL);
			dbg_printnc(1, H_CHAR, config->end_bit_h_time/BASE_TIME_HL);
			break;
	}

	dbg_printf(1, "\n");

	return 0;
}

struct rf_hardware_driver dummy_driver = {
	.name = HARDWARE_NAME,
	.cmd_name = "dummy",
	.long_name = "Dummy Hardware",
	.supported_bit_fmts = (1 << RF_BIT_FMT_HL) | (1 << RF_BIT_FMT_LH) | (1 << RF_BIT_FMT_RAW),
	.probe = &dummy_probe,
	.init = &dummy_init,
	.close = &dummy_close,
	.send_cmd = &dummy_send_cmd,
};
