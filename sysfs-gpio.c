/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * SYSFS GPIO-based 433 MHz RF Transmitter driver
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "rf-ctrl.h"

#define HARDWARE_NAME			"SYSFS GPIO"

#define GPIO_SYSFS_ROOT			"/sys/class/gpio"
#define GPIO_SYSFS_EXPORT		"export"		// At root level
#define GPIO_SYSFS_UNEXPORT		"unexport"		// At root level
#define GPIO_SYSFS_DIRECTION		"direction"		// At root/gpio<num> level
#define GPIO_SYSFS_VALUE		"value"			// At root/gpio<num> level

static uint16_t gpio_num;


static int sysfs_gpio_probe(void) {
	/* This driver cannot be auto-detected */
	return -1;
}

static int sysfs_gpio_init(struct rf_hardware_params *params) {
	FILE *f;
	char path[256];

	if (!(params->provided_params & PARAM_GPIO)) {
		fprintf(stderr, "%s: GPIO parameter missing !\n", HARDWARE_NAME);
		return -1;
	}

	gpio_num = params->gpio;

	printf("%s: Using GPIO %u\n", HARDWARE_NAME, gpio_num);

	/* Export the GPIO used by the transmitter */
	f = fopen(GPIO_SYSFS_ROOT"/"GPIO_SYSFS_EXPORT, "w");
	if (f == NULL) {
		fprintf(stderr, "%s: Unable to open %s !\n", HARDWARE_NAME, GPIO_SYSFS_ROOT"/"GPIO_SYSFS_EXPORT);
		return -1;
	}

	fprintf(f, "%u\n", gpio_num);

	fclose(f);

	/* Set the direction of the GPIO as output */
	snprintf(path, 256, "%s/gpio%u/%s", GPIO_SYSFS_ROOT, gpio_num, GPIO_SYSFS_DIRECTION);

	f = fopen(path, "w");
	if (f == NULL) {
		fprintf(stderr, "%s: Unable to open %s !\n", HARDWARE_NAME, path);
		return -1;
	}

	fprintf(f, "out\n");

	fclose(f);

	return 0;
}

static void sysfs_gpio_close(void) {
	FILE *f;

	/* Release the GPIO used by the transmitter */
	f = fopen(GPIO_SYSFS_ROOT"/"GPIO_SYSFS_UNEXPORT, "w");
	if (f == NULL) {
		fprintf(stderr, "%s: Unable to open %s !\n", HARDWARE_NAME, GPIO_SYSFS_ROOT"/"GPIO_SYSFS_UNEXPORT);
		return;
	}

	fprintf(f, "%u\n", gpio_num);

	fclose(f);
}

static int sysfs_gpio_send_cmd(struct timing_config *config, uint8_t *frame_data, uint16_t bit_count) {
	int fd;
	char path[256];
	uint16_t i;
	char old_bit, new_bit;

	snprintf(path, 256, "%s/gpio%u/%s", GPIO_SYSFS_ROOT, gpio_num, GPIO_SYSFS_VALUE);

	fd = open(path, O_WRONLY| O_SYNC);
	if (fd < 0) {
		fprintf(stderr, "%s: Unable to open %s (%s) !\n", HARDWARE_NAME, path, strerror(errno));
		return fd;
	}

	switch (config->bit_fmt) {
		case RF_BIT_FMT_HL:
			write(fd, "1", 1);
			usleep(config->start_bit_h_time);
			write(fd, "0", 1);
			usleep(config->start_bit_l_time);
			break;

		case RF_BIT_FMT_LH:
			write(fd, "0", 1);
			usleep(config->start_bit_l_time);
			write(fd, "1", 1);
			usleep(config->start_bit_h_time);
			break;
	}

	/* Toggle the GPIO bit by bit */
	for (i = 0; i < bit_count; i++) {
		new_bit = (frame_data[i/8] & (1 << (7 - (i % 8)))) ? '1' : '0';

		switch (config->bit_fmt) {
			case RF_BIT_FMT_HL:
				if (new_bit == '1') {
					write(fd, "1", 1);
					usleep(config->data_bit1_h_time);
					write(fd, "0", 1);
					usleep(config->data_bit1_l_time);
				} else {
					write(fd, "1", 1);
					usleep(config->data_bit0_h_time);
					write(fd, "0", 1);
					usleep(config->data_bit0_l_time);
				}
				break;

			case RF_BIT_FMT_LH:
				if (new_bit == '1') {
					write(fd, "0", 1);
					usleep(config->data_bit1_l_time);
					write(fd, "1", 1);
					usleep(config->data_bit1_h_time);
				} else {
					write(fd, "0", 1);
					usleep(config->data_bit0_l_time);
					write(fd, "1", 1);
					usleep(config->data_bit0_h_time);
				}
				break;

			case RF_BIT_FMT_RAW:
				if (new_bit != old_bit) {
					old_bit = new_bit;
					write(fd, &new_bit, 1);
				}

				usleep(config->base_time);
				break;
		}
	}

	switch (config->bit_fmt) {
		case RF_BIT_FMT_HL:
			write(fd, "1", 1);
			usleep(config->end_bit_h_time);
			write(fd, "0", 1);
			usleep(config->end_bit_l_time);
			break;

		case RF_BIT_FMT_LH:
			write(fd, "0", 1);
			usleep(config->end_bit_l_time);
			write(fd, "1", 1);
			usleep(config->end_bit_h_time);
			break;
	}

	/* Make sure to turn off the transmitter */
	write(fd, "0", 1);

	close(fd);

	return 0;
}

struct rf_hardware_driver sysfs_gpio_driver = {
	.name = HARDWARE_NAME,
	.cmd_name = "sysfs-gpio",
	.long_name = "SYSFS GPIO-based 433 MHz RF Transmitter",
	.supported_bit_fmts = (1 << RF_BIT_FMT_HL) | (1 << RF_BIT_FMT_LH) | (1 << RF_BIT_FMT_RAW),
	.needed_hw_params = PARAM_GPIO,
	.probe = &sysfs_gpio_probe,
	.init = &sysfs_gpio_init,
	.close = &sysfs_gpio_close,
	.send_cmd = &sysfs_gpio_send_cmd,
};
