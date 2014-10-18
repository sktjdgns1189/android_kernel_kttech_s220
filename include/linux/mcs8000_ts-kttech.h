/*
 * driver/input/touch/screen/mcs8000_ts-kttech.h
 *
 * Author: Jhoonkim <jhoonkim@kttech.co.kr>
 *
 * Copyright (C) 2011 KT Tech, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 */


#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

/* Debug definitions */
#define	DEBUG_INFO			1
#define DEBUG_VERBOSE			2
#define	DEBUG_MESSAGES			5
#define	DEBUG_RAW 			8
#define	DEBUG_TRACE			10

/* KT Tech Platform, Choose one */
#ifdef CONFIG_KTTECH_MODEL_O3
#define CONFIG_KTTECH_TSP_PLAT_O3	1
#endif
#ifdef CONFIG_KTTECH_MODEL_O4
#define CONFIG_KTTECH_TSP_PLAT_O4	1
#endif
#ifdef CONFIG_KTTECH_MODEL_O6
#define CONFIG_KTTECH_TSP_PLAT_O6	1
#endif

/* For use threaded IRQ handler */
#define MELFAS_THREADED_IRQ

#define MELFAS_MAX_TOUCH		6
#define PUB_FW_VERSION			0x1
#define CORE_FW_VERSION			0x43

/* Setting TSP S/W Configuration */
#define TS_MAX_X_COORD			540
#define TS_MAX_Y_COORD			1024
#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH			30

#define TS_READ_EVENT_PACKET_SIZE	0x0F
#define TS_READ_START_ADDR		0x10
#define TS_READ_VERSION_ADDR		0xF3
#define TS_READ_REGS_LEN		66

#define TOUCH_TYPE_NONE			0
#define TOUCH_TYPE_SCREEN		1
#define TOUCH_TYPE_KEY			2

/* Setting TSP H/W Configuration */
#define I2C_RETRY_CNT			10
#define MELFAS_SLEEP_POWEROFF
#define SET_DOWNLOAD_BY_GPIO

/* Initialize Configuration */
#define INIT_FRAMERATE 			100
#define INIT_CORR_XPOS			20
#define INIT_CORR_X_VARIANT		2
#define INIT_CORR_YPOS			0
#define INIT_CORR_Y_VARIANT		0
#define ENABLE_HOME_KEY_WAKE	1

/* MCS8000 Register definitions */
#define TS_REG_MODE_CONROL		0x01
#define TS_REG_XYRES_HIGH		0x02
#define TS_REG_XRES_LOW			0x03
#define TS_REG_YRES_LOW			0x04
#define TS_REG_CNT_EVNT_THR		0x05
#define TS_REG_MOV_EVNT_THR		0x06
#define TS_REG_INPUTPKT_SZ		0x0F
#define TS_REG_INPUTENT_INFO		0x10
#define TS_REG_XYCOORD_HIGH		0x11
#define TS_REG_XCOORD_LOW		0x12
#define TS_REG_YCOORD_LOW		0x13
#define TS_REG_TOUCH_WIDTH		0x14
#define TS_REG_TOUCH_STRENGTH		0x15
#define TS_REG_UNIV_CMDID		0xA0
#define TS_REG_UNIV_CMD_PARM1		0xA1
#define TS_REG_UNIV_CMD_PARM2		0xA2
#define TS_REG_UNIV_CMD_SZ		0xAE
#define TS_REG_UNIV_CMD_RESULT		0xAF
#define TS_REG_VNDR_CMDID		0xB0
#define TS_REG_VNDR_CMD_PARM1		0xB1
#define TS_REG_VNDR_CMD_PARM2		0xB2
#define TS_REG_VNDR_CMD_SZ		0xBE
#define TS_REG_VNDR_CMD_RESULT		0xBF
#define TS_REG_FW_VER			0xF0
#define TS_REG_HW_REV			0xF1
#define TS_REG_HW_COMPGRP		0xF2
#define TS_REG_CORE_VER			0xF3
#define TS_REG_PRIV_VER			0xF4
#define TS_REG_PUB_VER			0xF5
#define TS_REG_PRV_CUSTOM_VER		0x00
#define TS_REG_PUB_CUSTOM_VER		0x01
#define TS_REG_CORE_CUSTOM_VER		0x43
#define TS_REG_PRODUCT_CORE_1		0xF6
#define TS_REG_PRODUCT_CORE_2		0xF7
#define TS_REG_PRODUCT_CORE_3		0xF8
#define TS_REG_PRODUCT_CORE_4		0xF9
#define TS_REG_PRODUCT_CORE_5		0xFA
#define TS_REG_PRODUCT_CORE_6		0xFB
#define TS_REG_PRODUCT_CORE_7		0xFC

/* Vendor Command */
#define TS_REG_CMD_SET_FRAMERATE		0x10
#define TS_REG_CMD_SET_CORRPOS_X		0x40
#define TS_REG_CMD_SET_CORRPOS_Y		0x80

/* ADD TRACKING_ID */
#define REPORT_MT(touch_number, x, y, amplitude, size)		\
	do {							\
		input_report_abs(ts->input, ABS_MT_TRACKING_ID, touch_number); \
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, size);         \
		input_report_abs(ts->input, ABS_MT_POSITION_X, x);             \
		input_report_abs(ts->input, ABS_MT_POSITION_Y, y);             \
		input_report_abs(ts->input, ABS_MT_PRESSURE, amplitude);    \
		input_mt_sync(ts->input);                                      \
	} while (0)

/* TSP KEY Block */
#define KEY_PRESS			1
#define KEY_RELEASE			0
#define NUMOF3KEYS			3

/* 3 Touch key array */
#define	TOUCH_3KEY_MENU			0x01
#define	TOUCH_3KEY_HOME			0x02
#define	TOUCH_3KEY_BACK			0x03
#define TOUCH_KEY_NULL			0x03

/* 2 Touch key array */
#define	TOUCH_2KEY_MENU			0x01
#define	TOUCH_2KEY_BACK			0x03
#define TOUCH_KEY_NULL			0x03

/* Touchscreen Key Positions : X */
#define TOUCH_3KEY_MENU_WITDTH		120
#define TOUCH_3KEY_HOME_WITDTH		120
#define TOUCH_3KEY_BACK_WITDTH		120
#define TOUCH_3KEY_MENU_CENTER		86
#define TOUCH_3KEY_HOME_CENTER		270
#define TOUCH_3KEY_BACK_CENTER		460

/* Touchscreen Key Positions : Y */ 
#define TOUCH_3KEY_HEIGHT		60
#define TOUCH_3KEY_TOP_GAP		15
#define TOUCH_3KEY_BOTTON_GAP		0
#define TOUCH_3KEY_MAX_YC		960
#define TOUCH_3KEY_MAX_TOUCH_YC		1024
#define TOUCH_3KEY_AREA_Y_TOP		(TOUCH_3KEY_MAX_YC + TOUCH_3KEY_TOP_GAP)
#define TOUCH_3KEY_AREA_Y_BOTTOM	(TOUCH_3KEY_MAX_TOUCH_YC - TOUCH_3KEY_BOTTON_GAP)

struct melfas_callbacks {
	void (*inform_charger)(struct melfas_callbacks *, int mode);
};

struct melfas_ts_platform_data {
	const char                     *platform_name;
	uint8_t                        numtouch;   /* Number of touches to report  */
	void                           (*init_platform_hw)(struct melfas_ts_platform_data *);
	void                           (*exit_platform_hw)(struct melfas_ts_platform_data *);
	void                           (*suspend_platform_hw)(struct melfas_ts_platform_data *);
	void                           (*resume_platform_hw)(struct melfas_ts_platform_data *);

	int                            max_x;      /* The default reported X range   */
	int                            max_y;      /* The default reported Y range   */
	void	                       (*register_cb)(struct melfas_callbacks *);

	const char                     *reg_lvs3_name;
	const char                     *reg_l4_name;
	const char                     *reg_mvs0_name;

	struct regulator               *reg_lvs3;  /* TSP 1.8V Pull-up       */
	struct regulator               *reg_l4;	   /* TSP 2.7V Main power */
	struct regulator               *reg_mvs0;  /* TSP 2.7V Switch        */

	int                            reg_lvs3_level; /* uV range */
	int                            reg_l4_level;   /* uV range */
	int                            reg_mvs0_level; /* uV range */

	u32                            irq_gpio;
	u32                            key_gpio;
	u32                            board_rev;  /* for set revision check               */
	u32                            boot_mode;  /* for set kernel boot mode check */
};

struct melfas_ts_data
{
	struct melfas_ts_platform_data *pdata;
	struct i2c_client              *client;
	struct input_dev               *input;
	struct delayed_work            initial_dwork;
	struct work_struct             ta_work;
	struct semaphore               msg_sem;
	spinlock_t                     lock;
	uint8_t                        phys_name[32];
	uint32_t                       irq;
	uint32_t                       irq_key;
	uint32_t                       flags;
	uint16_t                       addr;
	uint16_t                       set_mode_for_ta;

	/* TS information */
	uint32_t                       framerate;
	uint32_t                       framerate_initialized;	
	uint32_t                       correction_x;
	uint32_t                       correction_x_variant;
	uint32_t                       correction_y;
	uint32_t                       correction_y_variant;
	uint32_t                       correcton_initialized_x;					
	uint32_t                       correcton_initialized_y;		
	uint32_t                       enable_key_wake;

	/* For debugging */
	uint32_t                       message_counter;
	uint32_t                       read_fail_counter;
	uint32_t                       irq_counter;
	uint32_t                       valid_irq_counter;
	uint32_t                       invalid_irq_counter;
	
	struct melfas_callbacks        callbacks;	
	struct wake_lock               wakelock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend           early_suspend;
#endif
};

#endif /* _LINUX_MELFAS_TS_H */
