/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef MT9M114_H
#define MT9M114_H

#include <linux/types.h>
#include <mach/camera.h>

extern struct mt9m114_reg mt9m114_regs;

enum mt9m114_width {
	WORD_LEN,
	BYTE_LEN
};

enum mt9m114_setting {
	RES_PREVIEW,
	RES_CAPTURE
};
enum mt9m114_reg_update {
	/* Sensor egisters that need to be updated during initialization */
	REG_INIT,
	/* Sensor egisters that needs periodic I2C writes */
	UPDATE_PERIODIC,
	/* All the sensor Registers will be updated */
	UPDATE_ALL,
	/* Not valid update */
	UPDATE_INVALID
};

struct mt9m114_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum mt9m114_width width;
	unsigned short mdelay_time;
};

struct mt9m114_reg {
	const struct mt9m114_i2c_reg_conf *regtbl;
	uint16_t regtbl_size; 
};

#endif /* MT9M114_H */
