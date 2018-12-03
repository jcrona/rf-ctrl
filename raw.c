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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "raw.h"


size_t raw_write_low(uint8_t *buf, size_t offset, uint8_t length) {
	size_t i;

	for (i = offset; i < (offset + length); i++) {
		buf[i/8] &= ~(1 << (7 - (i % 8)));
	}

	return length;
}

size_t raw_write_high(uint8_t *buf, size_t offset, uint8_t length) {
	size_t i;

	for (i = offset; i < (offset + length); i++) {
		buf[i/8] |= 1 << (7 - (i % 8));
	}

	return length;
}

size_t raw_write_edge(uint8_t *buf, size_t offset, raw_edge_order_t order, uint8_t h_len, uint8_t l_len) {
	size_t count = offset;

	if (order == RAW_EDGE_ORDER_HL) {
		/* High, then low */
		count += raw_write_high(buf, count, h_len);
		count += raw_write_low(buf, count, l_len);
	} else {
		/* Low, then high */
		count += raw_write_low(buf, count, l_len);
		count += raw_write_high(buf, count, h_len);
	}

	return (count - offset);
}

size_t raw_write_bits(uint8_t *buf, size_t offset, uint8_t *data, size_t data_bit_len, raw_edge_order_t zero_order, uint8_t zero_h_len, uint8_t zero_l_len, raw_edge_order_t one_order, uint8_t one_h_len, uint8_t one_l_len) {
	size_t count = offset;
	size_t i;

	for (i = 0; i < data_bit_len; i++) {
		if ((data[i/8] & (1 << (7 - (i % 8)))) != 0) {
			count += raw_write_edge(buf, count, one_order, one_h_len, one_l_len);
		} else {
			count += raw_write_edge(buf, count, zero_order, zero_h_len, zero_l_len);
		}
	}

	return (count - offset);
}
