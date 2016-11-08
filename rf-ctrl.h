/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
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

#ifndef _RF_CTRL_H_
#define _RF_CTRL_H_

#define PARAM_HARDWARE			0x01
#define PARAM_PROTOCOL			0x02
#define PARAM_REMOTE_ID			0x04
#define PARAM_DEVICE_ID			0x08
#define PARAM_COMMAND			0x10
#define PARAM_SCAN			0X20
#define PARAM_NFRAME			0X40
#define PARAM_VERBOSE			0X80

#define STORAGE_PATH_MAX_LEN		512

typedef enum {
	RF_CMD_OFF =		0,
	RF_CMD_ON =		1,
	RF_CMD_GOFF =		2,
	RF_CMD_GON =		3,
	RF_CMD_PROG =		4,
	RF_CMD_F1 =		5,
	RF_CMD_F2 =		6,
	RF_CMD_F3 =		7,
	RF_CMD_MAX,
} rf_command_t;

typedef enum {
	RF_BIT_FMT_HL =		0,
	RF_BIT_FMT_LH =		1,
	RF_BIT_FMT_RAW =	2,
} rf_bit_fmt_t;

extern char *(rf_command_str[]);
extern char *(rf_bit_fmt_str[]);

struct timing_config {
	uint16_t start_bit_h_time;
	uint16_t start_bit_l_time;
	uint16_t end_bit_h_time;
	uint16_t end_bit_l_time;
	uint16_t data_bit0_h_time;
	uint16_t data_bit0_l_time;
	uint16_t data_bit1_h_time;
	uint16_t data_bit1_l_time;
	uint16_t base_time;
	rf_bit_fmt_t bit_fmt;
	uint8_t frame_count;
};

struct rf_hardware_driver {
	char *name;
	char *cmd_name;
	char *long_name;
	int (*probe)(void);
	int (*init)(void);
	void (*close)(void);
	int (*send_cmd)(struct timing_config *config, uint8_t *frame_data, uint16_t bit_count);
};

struct rf_protocol_driver {
	char *name;
	char *cmd_name;
	struct timing_config *timings;
	int (*format_cmd)(uint8_t *data, size_t data_len, uint32_t remote_code, uint32_t device_code, rf_command_t command);
	uint32_t remote_code_max;
	uint32_t device_code_max;
	uint8_t needed_params;
};

int is_dbg_enabled(int level);
void dbg_printf(int level, char *buff, ...);
void get_storage_path(char *path, struct rf_protocol_driver *protocol);

#endif /* _RF_CTRL_H_ */
