/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#include "rf-ctrl.h"
#include "raw.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define APP_NAME			"rf-ctrl"
#define APP_VERSION			"0.6"
#define COPYRIGHT_DATE			"2018"
#define AUTHOR_NAME			"Jean-Christophe Rona"

#define STORAGE_PATH_BASE		"."APP_NAME

#define MAX_FRAME_LENGTH		512

#define DEFAULT_RAW_FALLBACK_ACCURACY	90 // This changes how accurate will be the base_time for generated RAW frames (100 minus the allowed error in % of the shortest timing)

#ifndef CONFIG_FILE_LOCATION
#define CONFIG_FILE_LOCATION		"/etc"
#endif

#define CONFIG_FILE_PATH		CONFIG_FILE_LOCATION"/"APP_NAME".conf"
#define CONFIG_LINE_MAX			1024 // bytes

#define CONFIG_FIELD_HARDWARE		"HARDWARE"
#define CONFIG_FIELD_GPIO		"GPIO"
#define CONFIG_FIELD_RAW		"FORCE_RAW"
#define CONFIG_FIELD_ACCURACY		"RAW_ACCURACY"

#define CONFIG_VALUE_TRUE		"TRUE"
#define CONFIG_VALUE_FALSE		"FALSE"

/* WARNING: Needs to remain in-sync with PARAM_* defines in rf-ctrl.h */
char *(parameter_str[]) = {
	"hw",
	"proto",
	"remote",
	"device",
	"command",
	"scan",
	"nframe",
	"accuracy",
	"raw",
	"gpio",
	"verbose",
};

/* WARNING: Needs to remain in-sync with rf_command_t enum in rf-ctrl.h */
char *(rf_command_str[]) = {
	"OFF",
	"ON",
	"Group OFF",
	"Group ON",
	"Prog",
	"F1",
	"F2",
	"F3",
};

char *(rf_cmdline_command_str[]) = {
	"off",
	"on",
	"goff",
	"gon",
	"prog",
	"f1",
	"f2",
	"f3",
};

/* WARNING: Needs to remain in-sync with rf_bit_fmt_t enum in rf-ctrl.h */
char *(rf_bit_fmt_str[]) = {
	"H-L",
	"L-H",
	"Raw",
};

static struct rf_hardware_driver *current_hw_driver = NULL;

static int debug_level = 0;

static uint8_t raw_fallback_accuracy = DEFAULT_RAW_FALLBACK_ACCURACY;

extern struct rf_protocol_driver otax_driver;
extern struct rf_protocol_driver dio_driver;
extern struct rf_protocol_driver he_driver;
extern struct rf_protocol_driver idk_driver;
extern struct rf_protocol_driver sumtech_driver;
extern struct rf_protocol_driver auchan_driver;
extern struct rf_protocol_driver auchan2_driver;
extern struct rf_protocol_driver somfy_driver;
extern struct rf_protocol_driver blyss_driver;

extern struct rf_hardware_driver he853_driver;
extern struct rf_hardware_driver ook_gpio_driver;
extern struct rf_hardware_driver sysfs_gpio_driver;
#ifdef ALSA_ENABLED
extern struct rf_hardware_driver alsa_driver;
#endif
extern struct rf_hardware_driver dummy_driver;

struct rf_protocol_driver *(protocol_drivers[]) = {
	&otax_driver,
	&dio_driver,
	&he_driver,
	&idk_driver,
	&sumtech_driver,
	&auchan_driver,
	&auchan2_driver,
	&somfy_driver,
	&blyss_driver,
};

struct rf_hardware_driver *(hardware_drivers[]) = {
	&he853_driver,
	&ook_gpio_driver,
	&sysfs_gpio_driver,
#ifdef ALSA_ENABLED
	&alsa_driver,
#endif
	&dummy_driver,
};

int is_dbg_enabled(int level) {
	return (level <= debug_level);
}

void dbg_printf(int level, char *buff, ...) {
	va_list arglist;

	if (level > debug_level) {
		return;
	}

	va_start(arglist, buff);

	vfprintf(stdout, buff, arglist);

	va_end(arglist);
}

static void mkdir_p(const char *dir) {
	char *tmp, *p = NULL;
	size_t len;

	tmp = strdup(dir);
	len = strlen(tmp);

	if (tmp[len - 1] == '/') {
		tmp[len - 1] = 0;
	}

	for (p = tmp + 1; *p != '\0'; p++) {
		if (*p == '/') {
			*p = 0;
			mkdir(tmp, S_IRWXU);
			*p = '/';
		}
	}
	mkdir(tmp, S_IRWXU);

	free(tmp);
}

void get_storage_path(char *path, struct rf_protocol_driver *protocol) {
	struct stat st = {0};
	struct passwd *pw = getpwuid(getuid());

	snprintf(path, STORAGE_PATH_MAX_LEN, "%s/%s/%s", pw->pw_dir, STORAGE_PATH_BASE, protocol->cmd_name);

	/* Create the folder if it does not exist */
	if (stat(path, &st) < 0) {
		mkdir_p(path);
	}
}

static struct rf_protocol_driver * get_protocol_driver_by_id(unsigned int protocol) {
	if (protocol >= ARRAY_SIZE(protocol_drivers)) {
		return NULL;
	}

	return protocol_drivers[protocol];
}

static struct rf_protocol_driver * get_protocol_driver_by_name(char *protocol_str) {
	int i;

	for (i = 0; i < ARRAY_SIZE(protocol_drivers); i++) {
		if (!strcmp(protocol_drivers[i]->cmd_name, protocol_str)) {
			return protocol_drivers[i];
		}
	}

	return NULL;
}

static int get_protocol_id_by_name(char *protocol_str) {
	int i;

	for (i = 0; i < ARRAY_SIZE(protocol_drivers); i++) {
		if (!strcmp(protocol_drivers[i]->cmd_name, protocol_str)) {
			return i;
		}
	}

	return -1;
}

static struct rf_hardware_driver * get_hw_driver_by_id(unsigned int hw) {
	if (hw >= ARRAY_SIZE(hardware_drivers)) {
		return NULL;
	}

	return hardware_drivers[hw];
}

static struct rf_hardware_driver * get_hw_driver_by_name(char *hw_str) {
	int i;

	for (i = 0; i < ARRAY_SIZE(hardware_drivers); i++) {
		if (!strcmp(hardware_drivers[i]->cmd_name, hw_str)) {
			return hardware_drivers[i];
		}
	}

	return NULL;
}

static struct rf_hardware_driver * auto_detect_hw_driver(void) {
	int i;

	for (i = 0; i < ARRAY_SIZE(hardware_drivers); i++) {
		if (!hardware_drivers[i]->probe()) {
			return hardware_drivers[i];
		}
	}

	return NULL;
}

static int get_hw_id_by_name(char *hw_str) {
	int i;

	for (i = 0; i < ARRAY_SIZE(hardware_drivers); i++) {
		if (!strcmp(hardware_drivers[i]->cmd_name, hw_str)) {
			return i;
		}
	}

	return -1;
}

static int get_cmd_id_by_name(char *cmd_str) {
	int i;

	for (i = 0; i < ARRAY_SIZE(rf_cmdline_command_str); i++) {
		if (!strcmp(rf_cmdline_command_str[i], cmd_str)) {
			return i;
		}
	}

	return -1;
}

static int parse_config_file(uint16_t *provided_params, int *hw, struct rf_hardware_params *hw_params) {
	FILE * f;
	char line[CONFIG_LINE_MAX];
	char *field, *value;
	char *p;

	f = fopen(CONFIG_FILE_PATH, "r");
	if (f == NULL) {
		dbg_printf(1, "Warning: Cannot open %s\n", CONFIG_FILE_PATH);
		return -1;
	}

	while (fgets(line, sizeof(line), f) != NULL) {
		/* Remove leading spaces */
		field = line;
		while (*field == ' ' || *field == '\t') {
			field++;
		}

		/* Detect comments */
		if (*field == '#') {
			continue;
		}

		value = strstr(line, "=");
		if (value == NULL) {
			continue;
		}

		/* Remove '=' */
		p = value;
		*value++ = '\0';

		/* Remove trailing spaces */
		while (p > field && (*p == ' ' || *p == '\t' || *p == '\0')) {
			*p-- = '\0';
		}

		/* Remove leading spaces */
		while (*value == ' ' || *value == '\t') {
			value++;
		}

		/* Remove trailing spaces */
		p = value + strlen(value);
		while (p > value && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\0')) {
			*p-- = '\0';
		}

		if (!strncmp(field, CONFIG_FIELD_HARDWARE, sizeof(CONFIG_FIELD_HARDWARE) - 1)) {
			*hw = strtoul(value, &p, 0);
			if (*p != '\0') {
				*hw = get_hw_id_by_name(value);
			}

			if (*hw < 0 || *hw >= ARRAY_SIZE(hardware_drivers)) {
				fprintf(stderr, "Unsupported RF hardware %s in configuration file\n", value);
				continue;
			}

			*provided_params |= PARAM_HARDWARE;
		} else if (!strncmp(field, CONFIG_FIELD_GPIO, sizeof(CONFIG_FIELD_GPIO) - 1)) {
			hw_params->gpio = strtoul(value, &p, 0);
			*provided_params |= PARAM_GPIO;
		} else if (!strncmp(field, CONFIG_FIELD_RAW, sizeof(CONFIG_FIELD_RAW) - 1)) {
			if (!strncmp(value, CONFIG_VALUE_TRUE, sizeof(CONFIG_VALUE_TRUE) - 1)) {
				*provided_params |= PARAM_RAW;
			}
		} else if (!strncmp(field, CONFIG_FIELD_ACCURACY, sizeof(CONFIG_FIELD_ACCURACY) - 1)) {
			raw_fallback_accuracy = strtoul(value, NULL, 0);
			if (raw_fallback_accuracy > 100) {
				fprintf(stderr, "Invalid accuracy %s in configuration file\n", value);
				continue;
			}

			*provided_params |= PARAM_ACCURACY;
		}
	}

	fclose(f);
}

static int gcd(uint16_t a, uint16_t b, uint16_t accuracy) {
	if (a <= accuracy) {
		return b;
	}

	return gcd(b % a, a, accuracy);
}

static uint16_t find_best_base_time(struct timing_config *config) {
	uint16_t gcd1, gcd2, gcd3, gcd4;
	uint16_t accuracy = 0xFFFF;

	if ((config->start_bit_h_time > 0) && (accuracy > config->start_bit_h_time)) {
		accuracy = config->start_bit_h_time;
	}

	if ((config->start_bit_l_time > 0) && (accuracy > config->start_bit_l_time)) {
		accuracy = config->start_bit_l_time;
	}

	if ((config->end_bit_h_time > 0) && (accuracy > config->end_bit_h_time)) {
		accuracy = config->end_bit_h_time;
	}

	if ((config->end_bit_l_time > 0) && (accuracy > config->end_bit_l_time)) {
		accuracy = config->end_bit_l_time;
	}

	if ((config->data_bit0_h_time > 0) && (accuracy > config->data_bit0_h_time)) {
		accuracy = config->data_bit0_h_time;
	}

	if ((config->data_bit0_l_time > 0) && (accuracy > config->data_bit0_l_time)) {
		accuracy = config->data_bit0_l_time;
	}

	if ((config->data_bit1_h_time > 0) && (accuracy > config->data_bit1_h_time)) {
		accuracy = config->data_bit1_h_time;
	}

	if ((config->data_bit1_l_time > 0) && (accuracy > config->data_bit1_l_time)) {
		accuracy = config->data_bit1_l_time;
	}

	accuracy = (accuracy * (100 - raw_fallback_accuracy))/100;

	dbg_printf(3, "  RAW Accuracy: %u us (%u%%)\n", accuracy, raw_fallback_accuracy);

	gcd1 = gcd(config->start_bit_h_time, config->start_bit_l_time, accuracy);
	gcd2 = gcd(config->end_bit_h_time, config->end_bit_l_time, accuracy);
	gcd3 = gcd(config->data_bit0_h_time, config->data_bit0_l_time, accuracy);
	gcd4 = gcd(config->data_bit1_h_time, config->data_bit1_l_time, accuracy);

	gcd1 = gcd(gcd1, gcd2, accuracy);
	gcd3 = gcd(gcd3, gcd4, accuracy);

	return (uint16_t) gcd(gcd1, gcd3, accuracy);
}

static int send_cmd(uint32_t remote_code, uint32_t device_code, rf_command_t command, int protocol, uint8_t force_raw) {
	struct rf_protocol_driver *protocol_driver;
	uint8_t data[MAX_FRAME_LENGTH];
	int data_bit_count;
	struct timing_config fallback_timings;
	uint8_t fallback_data[MAX_FRAME_LENGTH];
	int fallback_data_bit_count;
	uint16_t base_time;
	int ret = 0;
	int i;

	protocol_driver = get_protocol_driver_by_id(protocol);

	data_bit_count = protocol_driver->format_cmd(data, sizeof(data), remote_code, device_code, command);
	if (data_bit_count < 0) {
		fprintf(stderr, "%s - %s: Format command failed\n", current_hw_driver->name, protocol_driver->name);
		return data_bit_count;
	}

	printf("Sending %s command '%s'", protocol_drivers[protocol]->name, rf_command_str[(int) command]);

	if (protocol_drivers[protocol]->needed_params & PARAM_DEVICE_ID) {
		printf(" to device ID %u", device_code);
	}

	if (protocol_drivers[protocol]->needed_params & PARAM_REMOTE_ID) {
		printf(", using remote ID %u", remote_code);
	}

	printf(" ...\n");

	if (is_dbg_enabled(3)) {
		dbg_printf(3, "  Timings (%s): Bit Format %s - Frame Count %u\n", protocol_driver->name,
				rf_bit_fmt_str[(int) protocol_driver->timings->bit_fmt], protocol_driver->timings->frame_count);

		switch (protocol_driver->timings->bit_fmt) {
			case RF_BIT_FMT_HL:
			case RF_BIT_FMT_LH:
				dbg_printf(3, "  Timings (%s): Start-bit HTime %u us - Start-bit LTime %u us\n", protocol_driver->name,
						protocol_driver->timings->start_bit_h_time, protocol_driver->timings->start_bit_l_time);
				dbg_printf(3, "  Timings (%s): End-bit HTime %u us - End-bit LTime %u us\n", protocol_driver->name,
						protocol_driver->timings->end_bit_h_time, protocol_driver->timings->end_bit_l_time);
				dbg_printf(3, "  Timings (%s): Data-bit0 HTime %u us - Data-bit0 LTime %u us\n", protocol_driver->name,
						protocol_driver->timings->data_bit0_h_time, protocol_driver->timings->data_bit0_l_time);
				dbg_printf(3, "  Timings (%s): Data-bit1 HTime %u us - Data-bit1 LTime %u us\n", protocol_driver->name,
						protocol_driver->timings->data_bit1_h_time, protocol_driver->timings->data_bit1_l_time);
				break;

			case RF_BIT_FMT_RAW:
				dbg_printf(3, "  Timings (%s): Base HLTime %u us\n", protocol_driver->name,
						protocol_driver->timings->base_time);
				break;
		}
	}

	if (is_dbg_enabled(1)) {
		dbg_printf(1, "  Frame data (%s):", protocol_driver->name);
		for (i = 0; i < (data_bit_count + 7)/8; i++) {
			dbg_printf(1, " %02X", data[i]);
		}
		dbg_printf(1, "\n");
	}

	if (protocol_driver->timings->bit_fmt != RF_BIT_FMT_RAW && (!(current_hw_driver->supported_bit_fmts & (1 << protocol_driver->timings->bit_fmt)) || force_raw)) {
		if (current_hw_driver->supported_bit_fmts & (1 << RF_BIT_FMT_RAW)) {
			dbg_printf(1, "\n");
			if (!force_raw) {
				dbg_printf(1, "  Requested bit format not supported by %s, falling back to RAW\n", current_hw_driver->name);
			}

			/* Generate a RAW frame */
			base_time = find_best_base_time(protocol_driver->timings);

			fallback_data_bit_count = raw_generate_hl_frame(fallback_data, sizeof(fallback_data), protocol_driver->timings, data, (uint16_t) data_bit_count, base_time);

			fallback_timings.base_time = base_time;
			fallback_timings.bit_fmt = RF_BIT_FMT_RAW;
			fallback_timings.frame_count = protocol_driver->timings->frame_count;

			if (is_dbg_enabled(1)) {
				dbg_printf(3, "  RAW Timings (%s): Base HLTime %u us\n", protocol_driver->name,
						fallback_timings.base_time);

				dbg_printf(1, "  RAW Frame data (%s):", protocol_driver->name);
				for (i = 0; i < (fallback_data_bit_count + 7)/8; i++) {
					dbg_printf(1, " %02X", fallback_data[i]);
				}
				dbg_printf(1, "\n");
			}

			ret = current_hw_driver->send_cmd(&fallback_timings, fallback_data, (uint16_t) fallback_data_bit_count);
		} else {
			fprintf(stderr, "%s - %s: Bit format %u not supported\n", current_hw_driver->name, protocol_driver->name, protocol_driver->timings->bit_fmt);
			return -1;
		}
	} else {
		ret = current_hw_driver->send_cmd(protocol_driver->timings, data, (uint16_t) data_bit_count);
	}

	if (ret < 0) {
		fprintf(stderr, "%s - %s: configuration failed\n", current_hw_driver->name, protocol_driver->name);
		return ret;
	}

	return 0;
}

static void usage(FILE * fp, int argc, char **argv) {
	int i;

	fprintf(fp,"\n"
		APP_NAME" v"APP_VERSION" - (C)"COPYRIGHT_DATE" "AUTHOR_NAME"\n\n"
		"Usage: %s [options]\n\n"
		"Options:\n"
		"  -H | --hw <hardware>       Hardware driver to use (otherwise try to auto-detect)\n"
		"  -p | --proto <protocol>    Protocol to use\n"
		"  -r | --remote <id>         Remote ID to take\n"
		"  -d | --device <id>         Device ID to reach\n"
		"  -c | --command <command>   Command to send\n"
		"  -s | --scan                Perform a brute force scan (-p, -r and -d can be used to force specific values)\n"
		"  -n | --nframe <0-255>      Number of frames to send (override per protocol default value)\n"
		"  -a | --accuracy <0-100>    Accuracy of the timings in percent when HL frames are converted to RAW (default %u%%)\n"
		"  -R | --raw                 Convert HL frames to RAW if possible\n"
		"  -g | --gpio <num>          Which GPIO to use to transmit the signal (only for hardware drivers supporting it)\n"
		"  -v | --verbose             Print more detailed information (-vv and -vvv for even more details)\n"
		"  -h | --help                Print this message\n\n",
		argv[0], DEFAULT_RAW_FALLBACK_ACCURACY);

	fprintf(fp, "Available hardware drivers:");
	for (i = 0; i < ARRAY_SIZE(hardware_drivers); i++) {
		if (i > 0) {
			fprintf(fp, ",");
		}
		fprintf(fp, " %s (%s)", hardware_drivers[i]->cmd_name, hardware_drivers[i]->name);
	}
	fprintf(fp, "\n");

	fprintf(fp, "Available protocols:");
	for (i = 0; i < ARRAY_SIZE(protocol_drivers); i++) {
		if (i > 0) {
			fprintf(fp, ",");
		}
		fprintf(fp, " %s (%s)", protocol_drivers[i]->cmd_name, protocol_drivers[i]->name);
	}
	fprintf(fp, "\n");

	fprintf(fp, "Available commands:");
	for (i = 0; i < ARRAY_SIZE(rf_command_str); i++) {
		if (i > 0) {
			fprintf(fp, ",");
		}
		fprintf(fp, " %s (%s)", rf_cmdline_command_str[i], rf_command_str[i]);
	}
	fprintf(fp, "\n\n");

	fprintf(fp,
		"Examples:\n"
		"Turning off the DI-O device number 3 paired to the remote ID 424242:\n"
		"  $ %s -p dio -r 424242 -d 3 -c off\n\n"
		"Starting a fast RF scan on every OTAX devices, sending the 'ON' command:\n"
		"  $ %s -p otax -c on -s -n 1\n\n",
		argv[0], argv[0]);

}

static const char short_options[] = "H:p:r:d:c:sn:a:Rg:vh";

static const struct option long_options[] = {
	{"hw", required_argument, NULL, 'H'},
	{"proto", required_argument, NULL, 'p'},
	{"remote", required_argument, NULL, 'r'},
	{"device", required_argument, NULL, 'd'},
	{"command", required_argument, NULL, 'c'},
	{"scan", no_argument, NULL, 's'},
	{"nframe", required_argument, NULL, 'n'},
	{"accuracy", required_argument, NULL, 'a'},
	{"raw", no_argument, NULL, 'R'},
	{"gpio", required_argument, NULL, 'g'},
	{"verbose", required_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{0, 0, 0, 0}
};

int main(int argc, char **argv)
{
	int ret = 0;
	int hw = -1;
	int nframe = -1;
	int protocol = -1, command = -1;
	uint8_t force_raw = 0;
	uint32_t remote_id = 0, device_id = 0;
	uint8_t needed_params = PARAM_PROTOCOL | PARAM_REMOTE_ID | PARAM_DEVICE_ID | PARAM_COMMAND;
	uint16_t provided_params = 0;
	uint32_t proto_first = 0, proto_last = 0, remote_first = 0, remote_last = 0, device_first = 0, device_last = 0;
	struct rf_hardware_params hw_params;
	char * p;
	uint32_t i, j, k;

	/* Parse the configuration file first */
	parse_config_file(&provided_params, &hw, &hw_params);

	for (;;) {
		int index;
		int c;

		c = getopt_long(argc, argv, short_options, long_options, &index);

		if (-1 == c)
			break;

		switch (c) {
			case 0:	/* getopt_long() flag */
				break;

			case 'H':
				hw = strtoul(optarg, &p, 0);
				if (*p != '\0') {
					hw = get_hw_id_by_name(optarg);
				}

				if (hw < 0 || hw >= ARRAY_SIZE(hardware_drivers)) {
					fprintf(stderr, "Unsupported RF hardware %s\n", optarg);
					usage(stderr, argc, argv);
					return -1;
				}

				provided_params |= PARAM_HARDWARE;
				break;

			case 'p':
				protocol = strtoul(optarg, &p, 0);
				if (*p != '\0') {
					protocol = get_protocol_id_by_name(optarg);
				}

				if (protocol < 0 || protocol >= ARRAY_SIZE(protocol_drivers)) {
					fprintf(stderr, "Unsupported RF protocol %s\n", optarg);
					usage(stderr, argc, argv);
					return -1;
				}

				needed_params &= (PARAM_PROTOCOL | protocol_drivers[protocol]->needed_params);

				provided_params |= PARAM_PROTOCOL;
				break;

			case 'r':
				remote_id = strtoul(optarg, NULL, 0);
				provided_params |= PARAM_REMOTE_ID;
				break;

			case 'd':
				device_id = strtoul(optarg, NULL, 0);
				provided_params |= PARAM_DEVICE_ID;
				break;

			case 'c':
				command = strtoul(optarg, &p, 0);
				if (*p != '\0') {
					command = get_cmd_id_by_name(optarg);
				}

				if (command < 0 || command >= RF_CMD_MAX) {
					fprintf(stderr, "Unsupported RF command %s\n", optarg);
					usage(stderr, argc, argv);
					return -1;
				}

				provided_params |= PARAM_COMMAND;
				break;

			case 's':
				provided_params |= PARAM_SCAN;
				needed_params &= ~(PARAM_PROTOCOL | PARAM_REMOTE_ID | PARAM_DEVICE_ID);
				break;

			case 'n':
				nframe = strtoul(optarg, NULL, 0);
				provided_params |= PARAM_NFRAME;
				break;

			case 'a':
				raw_fallback_accuracy = strtoul(optarg, NULL, 0);
				if (raw_fallback_accuracy > 100) {
					fprintf(stderr, "Invalid accuracy %s\n", optarg);
					usage(stderr, argc, argv);
					return -1;
				}

				provided_params |= PARAM_ACCURACY;
				break;

			case 'R':
				provided_params |= PARAM_RAW;
				break;

			case 'g':
				hw_params.gpio = strtoul(optarg, &p, 0);
				provided_params |= PARAM_GPIO;
				break;

			case 'v':
				debug_level++;
				provided_params |= PARAM_VERBOSE;
				break;

			case 'h':
				usage(stdout, argc, argv);
				return 0;

			default:
				usage(stderr, argc, argv);
				return -1;
		}
	}

	dbg_printf(2, "Used configuration file is %s\n", CONFIG_FILE_PATH);

	if (needed_params & ~(provided_params)) {
		fprintf(stderr, "Missing arguments:");
		for (i = 0; i < ARRAY_SIZE(parameter_str); i++) {
			if ((needed_params & ~(provided_params)) & (0x1 << i)) {
				fprintf(stderr, " %s", parameter_str[i]);
			}
		}
		fprintf(stderr, " !\n");
		usage(stderr, argc, argv);
		return -1;
	}

	if (hw < 0) {
		/* Try to auto-detect */
		current_hw_driver = auto_detect_hw_driver();
		if (!current_hw_driver) {
			fprintf(stderr, "Cannot auto-detect HW driver\n");
			return -1;
		}
	} else {
		current_hw_driver = get_hw_driver_by_id(hw);
		if (!current_hw_driver) {
			fprintf(stderr, "Cannot find HW driver with index %d\n", hw);
			return -1;
		}
	}

	hw_params.provided_params = provided_params;

	if (current_hw_driver->needed_hw_params & ~(provided_params)) {
		fprintf(stderr, "Missing arguments specific to the %s driver:", current_hw_driver->name);
		for (i = 0; i < ARRAY_SIZE(parameter_str); i++) {
			if ((current_hw_driver->needed_hw_params & ~(provided_params)) & (0x1 << i)) {
				fprintf(stderr, " %s", parameter_str[i]);
			}
		}
		fprintf(stderr, " !\n");
		usage(stderr, argc, argv);
		return -1;
	}

	printf("Initializing %s driver...\n", current_hw_driver->long_name);
	ret = current_hw_driver->init(&hw_params);
	if (ret < 0) {
		fprintf(stderr, "Cannot initialize %s driver\n", current_hw_driver->name);
		goto exit;
	}

	if (nframe > 0) {
		printf("Number of frames forced to %d\n", nframe);
	}

	if (provided_params & PARAM_RAW) {
		if (!(current_hw_driver->supported_bit_fmts & (1 << RF_BIT_FMT_RAW))) {
			printf("Warning: %s does not support the RAW format, ignoring RAW conversion\n", current_hw_driver->name);
		} else {
			force_raw = 1;
		}
	}

	if (provided_params & PARAM_SCAN) {
		printf("Scanning");

		if (provided_params & PARAM_DEVICE_ID) {
			device_first = device_last = device_id;
			printf(" device ID %u", device_id);
		}

		if (provided_params & PARAM_REMOTE_ID) {
			remote_first = remote_last = remote_id;
			printf(", using remote ID %u", remote_id);
		}

		if (provided_params & PARAM_PROTOCOL) {
			proto_first = proto_last = protocol;
			printf(", with protocol fixed to %s", protocol_drivers[protocol]->name);
		} else {
			proto_last = ARRAY_SIZE(protocol_drivers) - 1;
		}

		printf("...\n");

		for (i = proto_first; i <= proto_last; i++) {
			if (nframe > 0) {
				protocol_drivers[i]->timings->frame_count = nframe;
			}

			if (!(provided_params & PARAM_REMOTE_ID)) {
				if (protocol_drivers[i]->needed_params & PARAM_REMOTE_ID) {
					remote_last = protocol_drivers[i]->remote_code_max;
				}
			}

			if (!(provided_params & PARAM_DEVICE_ID)) {
				if (protocol_drivers[i]->needed_params & PARAM_DEVICE_ID) {
					device_last = protocol_drivers[i]->device_code_max;
				}
			}

			for (j = remote_first; j <= remote_last; j++) {
				for (k = device_first; k <= device_last; k++) {
					send_cmd(j, k, (rf_command_t) command, i, force_raw);
				}
			}
		}
	} else {
		if (nframe > 0) {
			protocol_drivers[protocol]->timings->frame_count = nframe;
		}

		ret = send_cmd(remote_id, device_id, (rf_command_t) command, protocol, force_raw);
	}

exit:
	current_hw_driver->close();

	return ret;
}
