/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Dummy driver for test purpose
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


static int dummy_probe(void) {
	/* Prevent from being auto-detected */
	return -1;
}

static int dummy_init(void) {
	return 0;
}

static void dummy_close(void) {
}

static int dummy_send_cmd(struct timing_config *config, uint8_t *frame_data, uint16_t bit_count) {
	return 0;
}

struct rf_hardware_driver dummy_driver = {
	.name = "Dummy HW",
	.cmd_name = "dummy",
	.long_name = "Dummy Hardware",
	.probe = &dummy_probe,
	.init = &dummy_init,
	.close = &dummy_close,
	.send_cmd = &dummy_send_cmd,
};
