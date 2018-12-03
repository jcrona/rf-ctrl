/*
 * rf-ctrl - A command-line tool to control 433MHz OOK based devices
 * Helper functions for RAW frame generation
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

#ifndef _RAW_H_
#define _RAW_H_

typedef enum {
	RAW_EDGE_ORDER_HL =		0,
	RAW_EDGE_ORDER_LH =		1,
} raw_edge_order_t;

size_t raw_write_low(uint8_t *buf, size_t offset, uint8_t length);
size_t raw_write_high(uint8_t *buf, size_t offset, uint8_t length);
size_t raw_write_edge(uint8_t *buf, size_t offset, raw_edge_order_t order, uint8_t h_len, uint8_t l_len);
size_t raw_write_bits(uint8_t *buf, size_t offset, uint8_t *data, size_t data_bit_len, raw_edge_order_t zero_order, uint8_t zero_h_len, uint8_t zero_l_len, raw_edge_order_t one_order, uint8_t one_h_len, uint8_t one_l_len);
int raw_generate_hl_frame(uint8_t *dest_frame_data, size_t dest_data_len, struct timing_config *config, uint8_t *src_frame_data, uint16_t src_bit_count, uint16_t base_time);

#endif /* _RAW_H_ */
