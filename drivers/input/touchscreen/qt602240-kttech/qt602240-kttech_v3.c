/*
*  qt602240_kttech_v3.c - Atmel maXTouch Touchscreen Controller
*
*  Version 0.3a
*
*  An early alpha version of the maXTouch Linux driver.
*
*  Copyright (C) 2012-2011 JhoonKim <jhoonkim@kttech.co.kr>
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define	DEBUG_INFO      1
#define DEBUG_VERBOSE   2
#define	DEBUG_MESSAGES  5
#define	DEBUG_RAW       8
#define	DEBUG_TRACE     10
#define	TS_100S_TIMER_INTERVAL 1

/* Archicture definitions */
#include <asm/delay.h>
#include <asm/atomic.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

#include <linux/init.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/major.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/delay.h>

#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

/* Machine definitions */
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/pmic.h>
#include <mach/board.h>

#ifdef CONFIG_KTTECH_MODEL_O3
#include "qt602240-kttech_v3.h"
#include "qt602240-kttech_v3_cfg.h"
#include "qt602240-kttech_v3_config.h"
#endif
#ifdef CONFIG_KTTECH_MODEL_O4
#include "qt602240-kttech_v3_o4.h"
#include "qt602240-kttech_v3_cfg.h"
#include "qt602240-kttech_v3_config_o4.h"
#endif
#ifdef CONFIG_KTTECH_MODEL_O5
#include "qt602240-kttech_v3_o5.h"
#include "qt602240-kttech_v3_cfg.h"
#include "qt602240-kttech_v3_config_o5.h"
#endif

int tsp_3keycodes[NUMOF3KEYS] = { 
	KEY_MENU,
	KEY_HOME,
	KEY_BACK
};

char *tsp_3keyname[NUMOF3KEYS + 1] ={
	"Menu",
	"Home",
	"Back",
	"Null",
};

static u16 tsp_keystatus;
static u16 tsp_key_status_pressed;

enum {
	DISABLE,
	ENABLE
};

static bool cal_check_flag;
static u8 recal_comp_flag;
static u8 facesup_message_flag;
static u8 facesup_message_flag_T9 ;
static bool timer_flag = DISABLE;
static uint8_t timer_ticks = 0;
static unsigned int mxt_time_point;
static unsigned int time_after_autocal_enable = 0;
static bool coin_check_flag = 0;
static u8 coin_check_count = 0;
static bool metal_suppression_chk_flag = true;
static u8 chk_touch_cnt, chk_antitouch_cnt;

#define ABS(x,y)		( (x < y) ? (y - x) : (x - y))

struct  tch_msg_t{
	u8  id;
	u8  status[10];
	u16  xpos[10];
	u16  ypos[10];
	u8   area[10];
	u8   amp[10];
};

struct tch_msg_t new_touch;
struct tch_msg_t old_touch;

struct  {
	s16 length[5];
	u8 angle[5];
	u8 cnt;
}tch_vct[10];

static void check_chip_calibration(struct mxt_data *mxt);
static void check_chip_channel(struct mxt_data *mxt);
static void cal_maybe_good(struct mxt_data *mxt);
static int calibrate_chip(struct mxt_data *mxt);
static void mxt_palm_recovery(struct work_struct *work);
static void check_chip_palm(struct mxt_data *mxt);

static int debug = DEBUG_INFO; //DEBUG_MESSAGES; //DEBUG_TRACE;

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Activate debugging output");

#if ENABLE_NOISE_TEST_MODE
/*botton_right, botton_left, center, top_right, top_left */
u16 test_node[5] = { 12, 20, 104, 188, 196};;
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static void mxt_early_suspend(struct early_suspend *h);
static void mxt_late_resume(struct early_suspend *h);
#endif

#if	TS_100S_TIMER_INTERVAL
static struct workqueue_struct *ts_100s_tmr_workqueue;
static void ts_100ms_timeout_handler(unsigned long data);
static void ts_100ms_timer_start(struct mxt_data *mxt);
static void ts_100ms_timer_stop(struct mxt_data *mxt);
static void ts_100ms_timer_init(struct mxt_data *mxt);
static void ts_100ms_tmr_work(struct work_struct *work);
#endif

static int  mxt_identify(struct i2c_client *client, struct mxt_data *mxt);
static int  mxt_read_object_table(struct i2c_client *client, struct mxt_data *mxt);

#define I2C_RETRY_COUNT 10

const u8 *maxtouch_family = "maXTouch";
const u8 *mxt224_variant  = "mXT224";

u8 *object_type_name[MXT_MAX_OBJECT_TYPES] = { 
	[5] = "GEN_MESSAGEPROCESSOR_T5",
	[6] = "GEN_COMMANDPROCESSOR_T6",
	[7] = "GEN_POWERCONFIG_T7",
	[8] = "GEN_ACQUIRECONFIG_T8",
	[9] = "TOUCH_MULTITOUCHSCREEN_T9",
	[15] = "TOUCH_KEYARRAY_T15",
	[18] = "SPT_COMMSCONFIG_T18",
	[19] = "MXT_SPT_GPIOPWM_T19",
	[20] = "PROCI_GRIPSUPPRESSION_T20",
	[22] = "PROCG_NOISESUPPRESSION_T22",
	[23] = "MXT_TOUCH_PROXIMITY_T23",
	[24] = "PROCI_ONETOUCHGESTUREPROCESSOR_T24",
	[25] = "SPT_SELFTEST_T25",
	[28] = "SPT_CTECONFIG_T28",
	[37] = "DEBUG_DIAGNOSTICS_T37",
	[38] = "USER_DATA_T38",
};

struct multi_touch_info { 
	uint16_t size;
	int16_t pressure;
	int16_t x;
	int16_t y;
	int16_t component;
};

static struct multi_touch_info mtouch_info[MXT_MAX_NUM_TOUCHES];

static bool palm_check_timer_flag = false;
static bool palm_release_flag = true;
/*
* declaration of external functions
*/
u16 get_object_address(uint8_t object_type,
		       uint8_t instance,
		       struct mxt_object *object_table,
		       int max_objs);

int backup_to_nv(struct mxt_data *mxt)
{ 
#ifndef ENABLE_FATORY_TUNING
	/* backs up settings to the non-volatile memory */
	return mxt_write_byte(mxt->client,
		MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) +
		MXT_ADR_T6_BACKUPNV,
		0x55);
#else
	return 0;
#endif
}

int reset_chip(struct mxt_data *mxt, u8 mode)
{ 
	u8 data;
	if (debug >= DEBUG_MESSAGES)
		pr_info("[TSP] Reset chip Reset mode (%d)", mode);
	if (mode == RESET_TO_NORMAL)
		data = 0x1;  /* non-zero value */
	else if (mode == RESET_TO_BOOTLOADER)
		data = 0xA5;
	else { 
		pr_err("[TSP] Invalid reset mode(%d)", mode);
		return -1;
	}

	return mxt_write_byte(mxt->client,
		MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) +
		MXT_ADR_T6_RESET,
		data);
}

#ifdef MXT_ESD_WORKAROUND
static void mxt_force_reset(struct mxt_data *mxt)
{ 
	int cnt;

	if (debug >= DEBUG_MESSAGES)
		pr_info("[TSP] %s has been called!", __func__);

#ifndef MXT_THREADED_IRQ
	wake_lock(&mxt->wakelock);	/* prevents the system from entering suspend during updating */
	disable_irq(mxt->client->irq);	/* disable interrupt */
#endif
	if (mxt->pdata->suspend_platform_hw != NULL)
		mxt->pdata->suspend_platform_hw(mxt->pdata);
	msleep(100);
	if (mxt->pdata->resume_platform_hw != NULL)
		mxt->pdata->resume_platform_hw(mxt->pdata);

	for (cnt = 10; cnt > 0; cnt--) { 
		if (reset_chip(mxt, RESET_TO_NORMAL) == 0)	/* soft reset */
			break;
	}
	if (cnt == 0) { 
		pr_err("[TSP] mxt_force_reset failed!!!");
		return;
	}
	msleep(250);  /* 200ms */
#ifndef MXT_THREADED_IRQ
	enable_irq(mxt->client->irq);	/* enable interrupt */
	wake_unlock(&mxt->wakelock);
#endif
	if (debug >= DEBUG_MESSAGES)
		pr_info("[TSP] %s success!!!", __func__);
}
#endif

#if defined(MXT_DRIVER_FILTER)
static void equalize_coordinate(bool detect, u8 id, u16 *px, u16 *py)
{ 
	static int tcount[MXT_MAX_NUM_TOUCHES] = {  0, };
	static u16 pre_x[MXT_MAX_NUM_TOUCHES][4] = { { 0}, };
	static u16 pre_y[MXT_MAX_NUM_TOUCHES][4] = { { 0}, };
#if 0
	int coff[4] = { 0,};
	int distance = 0;
#endif

	if (detect) { 
		tcount[id] = 0;
	}

	pre_x[id][tcount[id]%4] = *px;
	pre_y[id][tcount[id]%4] = *py;

#if 0
	if (tcount[id] > 3)	{ 
		distance = abs(pre_x[id][(tcount[id]-1)%4] - *px) + abs(pre_y[id][(tcount[id]-1)%4] - *py);

		coff[0] = (u8)(2 + distance/5);
		if (coff[0] < 8) { 
			coff[0] = max(2, coff[0]);
			coff[1] = min((8 - coff[0]), (coff[0]>>1)+1);
			coff[2] = min((8 - coff[0] - coff[1]), (coff[1]>>1)+1);
			coff[3] = 8 - coff[0] - coff[1] - coff[2];

			printk("[TSP] %d, %d, %d, %d \n",
				coff[0], coff[1], coff[2], coff[3]);

			*px = (u16)((*px*(coff[0])
				+ pre_x[id][(tcount[id]-1)%4]*(coff[1])
				+ pre_x[id][(tcount[id]-2)%4]*(coff[2])
				+ pre_x[id][(tcount[id]-3)%4]*(coff[3]))/8);
			*py = (u16)((*py*(coff[0])
				+ pre_y[id][(tcount[id]-1)%4]*(coff[1])
				+ pre_y[id][(tcount[id]-2)%4]*(coff[2])
				+ pre_y[id][(tcount[id]-3)%4]*(coff[3]))/8);
		} else { 
			*px = (u16)((*px*4 + pre_x[id][(tcount[id]-1)%4])/5);
			*py = (u16)((*py*4 + pre_y[id][(tcount[id]-1)%4])/5);
		}
	}
#else
	if (tcount[id] > 0) {
		
		*px = (u16)((*px + pre_x[id][(tcount[id]-1)%4])>>1);
		*py = (u16)((*py + pre_y[id][(tcount[id]-1)%4])>>1);
		/*
		*px = (u16)((*px*4 + pre_x[id][(tcount[id]-1)%4])/5);
		*py = (u16)((*py*4 + pre_y[id][(tcount[id]-1)%4])/5);
		
		*px = (u16)((*px*2 + pre_x[id][(tcount[id]-1)%4])/3);
		*py = (u16)((*py*2 + pre_y[id][(tcount[id]-1)%4])/3);
		*/
	}
#endif
	tcount[id]++;
}
#endif  /* MXT_DRIVER_FILTER */

static void mxt_release_all_fingers(struct mxt_data *mxt)
{ 
	int id;
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] %s \n", __func__);
	
	for (id = 0 ; id < MXT_MAX_NUM_TOUCHES ; ++id) { 
		if ( mtouch_info[id].pressure == -1 )
			continue;

		/* force release check*/
		mtouch_info[id].pressure = 0;

		/* ADD TRACKING_ID*/
		REPORT_MT(id, mtouch_info[id].x, mtouch_info[id].y, mtouch_info[id].pressure, mtouch_info[id].size);

		if (debug >= DEBUG_MESSAGES)
			pr_info("[TSP] r (%d,%d) %d\n", mtouch_info[id].x, mtouch_info[id].y, id);

		if (mtouch_info[id].pressure == 0)
			mtouch_info[id].pressure = -1;
	}

	input_sync(mxt->input);
}

static void mxt_release_all_keys(struct mxt_data *mxt)
{ 
	if (debug >= DEBUG_INFO)
			pr_info("[TSP] %s, tsp_keystatus = %d \n", __func__, tsp_keystatus);
	if (tsp_keystatus != TOUCH_KEY_NULL) {
		switch (tsp_keystatus) {
			case TOUCH_3KEY_MENU:
				input_report_key(mxt->input, KEY_MENU, KEY_RELEASE);
				break;
			case TOUCH_3KEY_HOME:
				input_report_key(mxt->input, KEY_HOME, KEY_RELEASE);
				break;
			case TOUCH_3KEY_BACK:
				input_report_key(mxt->input, KEY_BACK, KEY_RELEASE);
				break;
			default:
				break;
			}

		if (debug >= DEBUG_MESSAGES) 
				pr_info("[TSP_KEY] r %s\n", tsp_3keyname[tsp_keystatus - 1]);

		tsp_keystatus = TOUCH_KEY_NULL;
	}
}

/*mode 1 = Charger connected */
/*mode 0 = Charger disconnected*/

static void mxt_inform_charger_connection(struct mxt_callbacks *cb, int mode)
{ 
	struct mxt_data *mxt = container_of(cb, struct mxt_data, callbacks);

	mxt->set_mode_for_ta = !!mode;
	if (mxt->mxt_status && !work_pending(&mxt->ta_work))
		schedule_work(&mxt->ta_work);
}

static void mxt_ta_worker(struct work_struct *work)
{ 
	struct mxt_data *mxt = container_of(work, struct mxt_data, ta_work);
	int error = 0;
#if defined(CONFIG_KTTECH_MODEL_O3) || defined(CONFIG_KTTECH_MODEL_O5)
	u8 T9_TCHTHR_TA, T9_TCHDI_TA, T9_MOVHYSTI_TA, T9_MOVFILTER_TA, T22_NOISETHR_TA;
#endif
#ifdef CONFIG_KTTECH_MODEL_O4
	u8 T9_TCHTHR_TA, T9_TCHDI_TA, T22_NOISETHR_TA, T9_MOVFILTER_TA;
#endif

	if (debug >= DEBUG_INFO)
		pr_info("[TSP] %s\n", __func__);

	disable_irq(mxt->client->irq);

	if (mxt->set_mode_for_ta) {
#if defined(CONFIG_KTTECH_MODEL_O3) || defined(CONFIG_KTTECH_MODEL_O5)
		/* Read TA Configurations */
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER3, &T9_TCHTHR_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER4, &T9_TCHDI_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER5, &T9_MOVHYSTI_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER6, &T9_MOVFILTER_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER7, &T22_NOISETHR_TA);

		/* Write TA Configurations */
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
			T9_TCHTHR_TA);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHDI,
			T9_TCHDI_TA);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_MOVHYSTI,
			T9_MOVHYSTI_TA);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_MOVFILTER,
			T9_MOVFILTER_TA);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22) + MXT_ADR_T22_NOISETHR,
			T22_NOISETHR_TA);
#endif
#ifdef CONFIG_KTTECH_MODEL_O4
		/* Read TA Configurations */
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER3, &T9_TCHTHR_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER4, &T9_TCHDI_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER5, &T9_MOVFILTER_TA);
		error += mxt_read_byte(mxt->client,	
			MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER7, &T22_NOISETHR_TA);

		/* Write TA Configurations */
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
			T9_TCHTHR_TA);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHDI,
			T9_TCHDI_TA);
		error += mxt_write_byte(mxt->client,				
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_MOVFILTER,
			T9_MOVFILTER_TA);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22) + MXT_ADR_T22_NOISETHR,
			T22_NOISETHR_TA);
#endif
	} else { 
#if defined(CONFIG_KTTECH_MODEL_O3) || defined(CONFIG_KTTECH_MODEL_O5)
		/* Write normal mode configurations */
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
			T9_TCHTHR);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHDI,
			T9_TCHDI);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_MOVHYSTI,
			T9_MOVHYSTI);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_MOVFILTER,
			T9_MOVFILTER);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22) + MXT_ADR_T22_NOISETHR,
			T22_NOISETHR);
#endif
#ifdef CONFIG_KTTECH_MODEL_O4
		/* Write normal mode configurations */
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
			T9_TCHTHR);
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHDI,
			T9_TCHDI);
		error += mxt_write_byte(mxt->client,				
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_MOVFILTER,
			T9_MOVFILTER);		
		error += mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_PROCG_NOISESUPPRESSION_T22) + MXT_ADR_T22_NOISETHR,
			T22_NOISETHR);
#endif
	}
	if (error < 0) pr_err("[TSP] mxt TA/USB mxt_noise_suppression_config Error!!\n");
	else { 
		if (debug >= DEBUG_INFO) {
			if (mxt->set_mode_for_ta) { 
				pr_info("[TSP] mxt TA/USB mxt_noise_suppression_config Success!!\n");				
			} else {
				pr_info("[TSP] mxt BATTERY mxt_noise_suppression_config Success!!\n");
			}
		}
		calibrate_chip(mxt);
	}

	enable_irq(mxt->client->irq);
	return;
}

/* metal suppress enable timer function */
static void mxt_metal_suppression_off(struct work_struct *work)
{ 
	int error, i;
	struct	mxt_data *mxt;
	mxt = container_of(work, struct mxt_data, timer_dwork.work);
	
	metal_suppression_chk_flag = false;
	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP]%s, metal_suppression_chk_flag = %d \n", __func__, metal_suppression_chk_flag);
	disable_irq(mxt->client->irq);
	mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_AMPHYST, T9_AMPHYST);

	if (time_after_autocal_enable != 0) {
		if ((jiffies_to_msecs(jiffies) - time_after_autocal_enable) > 1500) {
			if (debug >= DEBUG_MESSAGES)
				pr_info("[TSP] Floating metal Suppressed time out!! Autocal = 0, (%d), coin_check_count = %d \n", 
					(jiffies_to_msecs(jiffies) - time_after_autocal_enable), 
					coin_check_count);

			/* T8_TCHAUTOCAL  */			
			error = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_TCHAUTOCAL, 0);
			if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

			time_after_autocal_enable = 0;

			for(i=0;i < 10; i++) {
				tch_vct[i].cnt = 0;
			}
		}
	}
	enable_irq(mxt->client->irq);
	return;	
}

/* Calculates the 24-bit CRC sum. */
static u32 mxt_CRC_24(u32 crc, u8 byte1, u8 byte2)
{ 
	static const u32 crcpoly = 0x80001B;
	u32 result;
	u16 data_word;

	data_word = (u16) ((u16) (byte2 << 8u) | byte1);
	result = ((crc << 1u) ^ (u32) data_word);
	if (result & 0x1000000)
		result ^= crcpoly;
	return result;
}

/* Returns object address in mXT chip, or zero if object is not found */
u16 get_object_address(uint8_t object_type,
		       uint8_t instance,
		       struct mxt_object *object_table,
		       int max_objs)
{ 
	uint8_t object_table_index = 0;
	uint8_t address_found = 0;
	uint16_t address = 0;

	struct mxt_object obj;

	while ((object_table_index < max_objs) && !address_found) { 
		obj = object_table[object_table_index];
		if (obj.type == object_type) { 
			address_found = 1;
			/* Are there enough instances defined in the FW? */
			if (obj.instances >= instance) { 
				address = obj.chip_addr +
					(obj.size + 1) * instance;
			} else { 
				return 0;
			}
		}
		object_table_index++;
	}

	return address;
}

/* Returns object size in mXT chip, or zero if object is not found */
u16 get_object_size(uint8_t object_type, struct mxt_object *object_table, int max_objs)
{ 
	uint8_t object_table_index = 0;
	struct mxt_object obj;

	while (object_table_index < max_objs) { 
		obj = object_table[object_table_index];
		if (obj.type == object_type) { 
			return obj.size;
		}
		object_table_index++;
	}
	return 0;
}

/*
* Reads one byte from given address from mXT chip (which requires
* writing the 16-bit address pointer first).
*/

int mxt_read_byte(struct i2c_client *client, u16 addr, u8 *value)
{ 
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];
	__le16 le_addr = cpu_to_le16(addr);
	struct mxt_data *mxt;

	mxt = i2c_get_clientdata(client);


	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *) &le_addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = 1;
	msg[1].buf   = (u8 *) value;
	if  (i2c_transfer(adapter, msg, 2) == 2) { 
		mxt->last_read_addr = addr;
		return 0;
	} else { 
	/*
	* In case the transfer failed, set last read addr to invalid
	* address, so that the next reads won't get confused.
		*/
		mxt->last_read_addr = -1;
		return -EIO;
	}
}

/*
* Reads a block of bytes from given address from mXT chip. If we are
* reading from message window, and previous read was from message window,
* there's no need to write the address pointer: the mXT chip will
* automatically set the address pointer back to message window start.
*/

int mxt_read_block(struct i2c_client *client,
		   u16 addr,
		   u16 length,
		   u8 *value)
{ 
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];
	__le16	le_addr;
	struct mxt_data *mxt;

	mxt = i2c_get_clientdata(client);

	if (mxt != NULL) { 
		if ((mxt->last_read_addr == addr) &&
			(addr == mxt->msg_proc_addr)) { 
			if  (i2c_master_recv(client, value, length) == length)
				return 0;
			else
				return -EIO;
		} else { 
			mxt->last_read_addr = addr;
		}
	}

	le_addr = cpu_to_le16(addr);
	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *) &le_addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = (u8 *) value;
	if  (i2c_transfer(adapter, msg, 2) == 2)
		return 0;
	else
		return -EIO;

}

/* Reads a block of bytes from current address from mXT chip. */

int mxt_read_block_wo_addr(struct i2c_client *client,
			   u16 length,
			   u8 *value)
{ 


	if  (i2c_master_recv(client, value, length) == length) { 
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] read ok\n");
		return length;
	} else { 
		pr_err("[TSP] read failed\n");
		return -EIO;
	}

}


/* Writes one byte to given address in mXT chip. */

int mxt_write_byte(struct i2c_client *client, u16 addr, u8 value)
{ 
	struct { 
		__le16 le_addr;
		u8 data;

	} i2c_byte_transfer;

	struct mxt_data *mxt;

	mxt = i2c_get_clientdata(client);
	if (mxt != NULL)
		mxt->last_read_addr = -1;

	i2c_byte_transfer.le_addr = cpu_to_le16(addr);
	i2c_byte_transfer.data = value;


	if  (i2c_master_send(client, (u8 *) &i2c_byte_transfer, 3) == 3)
		return 0;
	else
		return -EIO;
}


/* Writes a block of bytes (max 256) to given address in mXT chip. */

int mxt_write_block(struct i2c_client *client,
		    u16 addr,
		    u16 length,
		    u8 *value)
{ 
	int i;
	struct { 
		__le16	le_addr;
		u8	data[256];

	} i2c_block_transfer;

	struct mxt_data *mxt;

	if (length > 256)
		return -EINVAL;

	mxt = i2c_get_clientdata(client);
	if (mxt != NULL)
		mxt->last_read_addr = -1;



	for (i = 0; i < length; i++)
		i2c_block_transfer.data[i] = *value++;


	i2c_block_transfer.le_addr = cpu_to_le16(addr);

	i = i2c_master_send(client, (u8 *) &i2c_block_transfer, length + 2);

	if (i == (length + 2))
		return length;
	else
		return -EIO;
}

/* TODO: make all other access block until the read has been done? Otherwise
an arriving message for example could set the ap to message window, and then
the read would be done from wrong address! */

/* Writes the address pointer (to set up following reads). */

int mxt_write_ap(struct i2c_client *client, u16 ap)
{ 

	__le16	le_ap = cpu_to_le16(ap);
	struct mxt_data *mxt;

	mxt = i2c_get_clientdata(client);
	if (mxt != NULL)
		mxt->last_read_addr = -1;

	if (debug >= DEBUG_INFO) 
		pr_info("[TSP] Address pointer set to %d\n", ap);

	if (i2c_master_send(client, (u8 *) &le_ap, 2) == 2)
		return 0;
	else
		return -EIO;
}

/* Calculates the CRC value for mXT infoblock. */
int calculate_infoblock_crc(u32 *crc_result, struct mxt_data *mxt)
{ 
	u32 crc = 0;
	u16 crc_area_size;
	u8 *mem;
	int i;

	int error;
	struct i2c_client *client;

	client = mxt->client;

	crc_area_size = MXT_ID_BLOCK_SIZE +
		mxt->device_info.num_objs * MXT_OBJECT_TABLE_ELEMENT_SIZE;

	mem = kmalloc(crc_area_size, GFP_KERNEL);

	if (mem == NULL) { 
		dev_err(&client->dev, "[TSP] Error allocating memory\n");
		return -ENOMEM;
	}

	error = mxt_read_block(client, 0, crc_area_size, mem);
	if (error < 0) { 
		kfree(mem);
		return error;
	}

	for (i = 0; i < (crc_area_size - 1); i = i + 2)
		crc = mxt_CRC_24(crc, *(mem + i), *(mem + i + 1));

	/* If uneven size, pad with zero */
	if (crc_area_size & 0x0001)
		crc = mxt_CRC_24(crc, *(mem + i), 0);

	kfree(mem);

	/* Return only 24 bits of CRC. */
	*crc_result = (crc & 0x00FFFFFF);
	return 1;

}

void Process_Touch_3KEY(struct mxt_data *mxt, struct multi_touch_info *mtouch_info, int i){
	u8 status = 0;

	/* Key Status Calculation */
	if (debug >= DEBUG_MESSAGES) 
		pr_info("[TSP_KEY] X : %d, Y : %d, Num : %d\n",  mtouch_info[i].x, mtouch_info[i].y, i);

	/* check MENU key */
	if(((TOUCH_3KEY_MENU_CENTER-TOUCH_3KEY_MENU_WITDTH/2) <= mtouch_info[i].x) && 
		(mtouch_info[i].x <= (TOUCH_3KEY_MENU_CENTER+TOUCH_3KEY_MENU_WITDTH/2))) {
		tsp_keystatus = TOUCH_3KEY_MENU;
	}  
	/* check HOME key */
	else if(((TOUCH_3KEY_HOME_CENTER-TOUCH_3KEY_HOME_WITDTH/2) <= mtouch_info[i].x) && 
		(mtouch_info[i].x <= (TOUCH_3KEY_HOME_CENTER+TOUCH_3KEY_HOME_WITDTH/2))) {
		tsp_keystatus = TOUCH_3KEY_HOME;
	}
	/* check BACK key */    
	else if(((TOUCH_3KEY_BACK_CENTER-TOUCH_3KEY_BACK_WITDTH/2) <= mtouch_info[i].x) &&
		(mtouch_info[i].x <= (TOUCH_3KEY_BACK_CENTER+TOUCH_3KEY_BACK_WITDTH/2))) {
		tsp_keystatus = TOUCH_3KEY_BACK;
	}
	else {
		REPORT_MT(i, mtouch_info[i].x, mtouch_info[i].y, mtouch_info[i].pressure, mtouch_info[i].size);
		
		if (mtouch_info[i].pressure == 0) 	/* if released */
			mtouch_info[i].pressure = -1;

		tsp_keystatus = TOUCH_KEY_NULL;
	}

	if(mtouch_info[i].pressure > 0) {	/* Key pressed */		
		status = KEY_PRESS;
	}	
	else {
		status = KEY_RELEASE;
	}
	
	/* defence code, if there is any Pressed key, force release!! */
	if(tsp_keystatus == TOUCH_KEY_NULL) {
		switch (tsp_key_status_pressed) {
		case TOUCH_3KEY_MENU:
			input_report_key(mxt->input, KEY_MENU, KEY_RELEASE);
			break;
		case TOUCH_3KEY_HOME:
			input_report_key(mxt->input, KEY_HOME, KEY_RELEASE);
			break;
		case TOUCH_3KEY_BACK:
			input_report_key(mxt->input, KEY_BACK, KEY_RELEASE);
			break;
		default:
			break;		
		}
		tsp_key_status_pressed = 0;
	}

	switch (tsp_keystatus) {
	case TOUCH_3KEY_MENU:
		input_report_key(mxt->input, KEY_MENU, status);
		break;
	case TOUCH_3KEY_HOME:
		input_report_key(mxt->input, KEY_HOME, status);
		/* Doing TS Calibration */
		if(status == KEY_RELEASE) {
			mxt_release_all_fingers(mxt);
			mxt_release_all_keys(mxt);				
			calibrate_chip(mxt);
		}
		break;
	case TOUCH_3KEY_BACK:
		input_report_key(mxt->input, KEY_BACK, status);
		/* Doing TS Calibration */
		/*
		if(status == KEY_RELEASE) {
			calibrate_chip(mxt);
		}
		*/
		break;
	default:
		break;		
	}

	if(status == KEY_PRESS)
		tsp_key_status_pressed = tsp_keystatus;
	else
		tsp_key_status_pressed = 0;

	if (debug >= DEBUG_MESSAGES) 
		pr_info("[TSP_KEY] r %s, s %d\n", tsp_3keyname[tsp_keystatus], status);
	
	tsp_keystatus = TOUCH_KEY_NULL;
}

void process_T9_message(u8 *message, struct mxt_data *mxt)
{ 
	struct	input_dev *input;
	u8  status;
	u16 xpos = 0xFFFF;
	u16 ypos = 0xFFFF;
	u8 report_id;
	u8 touch_id;  /* to identify each touches. starts from 0 to 15 */
	u8 pressed_or_released = 0;
	static int prev_touch_id = -1;
	int i, error;
	u16 chkpress = 0;
	u8 touch_message_flag = 0;

	input = mxt->input;
	status = message[MXT_MSG_T9_STATUS];
	report_id = message[0];
	touch_id = report_id - 2;

	if (touch_id >= MXT_MAX_NUM_TOUCHES) { 
		pr_err("[TSP] Invalid touch_id (toud_id=%d)", touch_id);
		return;
	}

	/* calculate positions of x, y */
	xpos = message[2];
	xpos = xpos << 4;
	xpos = xpos | (message[4] >> 4);
	xpos >>= 2;

	ypos = message[3];
	ypos = ypos << 4;
	ypos = ypos | (message[4] & 0x0F);
#ifndef CONFIG_KTTECH_MODEL_O5
	/* O5 Over 1024 y lines */
	ypos >>= 2;
#endif

	/************************************************************************
							defence coin lock-up added
	************************************************************************/
	if ((coin_check_count <= 2)/* && (cal_check_flag == 0)*/ && metal_suppression_chk_flag) {
		new_touch.id		= report_id;
		new_touch.status[new_touch.id]	= status;
		new_touch.xpos[new_touch.id]	= xpos;
		new_touch.ypos[new_touch.id]	= ypos;
		new_touch.area[new_touch.id]	= message[MXT_MSG_T9_TCHAREA];
		new_touch.amp[new_touch.id] 	= message[MXT_MSG_T9_TCHAMPLITUDE];

		tch_vct[new_touch.id].length[new_touch.id] = ABS(old_touch.xpos[new_touch.id],  new_touch.xpos[new_touch.id]);
		tch_vct[new_touch.id].length[new_touch.id] += ABS(old_touch.ypos[new_touch.id],new_touch.ypos[new_touch.id]);

		if( new_touch.area[new_touch.id] != 0) {
			tch_vct[new_touch.id].angle[new_touch.id] = new_touch.amp[new_touch.id] / new_touch.area[new_touch.id];
		} else {
			tch_vct[new_touch.id].angle[new_touch.id] = 255;
		}
		
		/* Software auto calibration */
#if 1
		 if(new_touch.area[new_touch.id] > 15) {
			tch_vct[new_touch.id].cnt = 0;
		} else if(new_touch.area[new_touch.id] < 4) { //4
			tch_vct[new_touch.id].cnt = 0;
		} else  if(new_touch.amp[new_touch.id] > 45) {
			tch_vct[new_touch.id].cnt = 0;
		} else if(new_touch.amp[new_touch.id] < 10) {
			tch_vct[new_touch.id].cnt = 0;
		} else if( tch_vct[new_touch.id].length[new_touch.id] > 20 ){
			tch_vct[new_touch.id].cnt = 0;
		} else if( tch_vct[new_touch.id].angle[new_touch.id] > 15 ) { 
			tch_vct[new_touch.id].cnt = 0;
		} else{
			tch_vct[new_touch.id].cnt ++;
		}
#else
		 if (/* for metal coin floating */
		 	(((new_touch.area[new_touch.id] > 3) && (new_touch.area[new_touch.id] < 14))
		 	&& ((new_touch.amp[new_touch.id] > 9) && (new_touch.amp[new_touch.id] < 44))
		 	&& ( tch_vct[new_touch.id].length[tch_vct[new_touch.id].cnt] < 18 )
		 	&& ( tch_vct[new_touch.id].angle[tch_vct[new_touch.id].cnt] < 10 ))
#if 0
		 	/* for finger floating */
		 	|| ((new_touch.area[new_touch.id] < 2)
		 	&& ((new_touch.amp[new_touch.id] > 1) && (new_touch.amp[new_touch.id] < 5))
		 	&& ( tch_vct[new_touch.id].length[tch_vct[new_touch.id].cnt] == 0 ))
#endif
			)
		 {
		 	tch_vct[new_touch.id].cnt ++;
		 } else {
			tch_vct[new_touch.id].cnt = 0;
		 }
#endif
		if (debug >= DEBUG_TRACE) {
			pr_info("[TSP] TCH_MSG :  %4d, 0x%2x, %4d, %4d , area=%d,  amp=%d, \n",
				report_id,
				status,
				new_touch.xpos[new_touch.id], //xpos,
				new_touch.ypos[new_touch.id], //ypos,
				new_touch.area[new_touch.id], //message[MXT_MSG_T9_TCHAREA], 
				new_touch.amp[new_touch.id]); //message[MXT_MSG_T9_TCHAMPLITUDE]);
			
			pr_info("[TSP] TCH_VCT :  %4d, length=%d, angle=%d, cnt=%d  \n",
				new_touch.id, 
				tch_vct[new_touch.id].length[new_touch.id],
				tch_vct[new_touch.id].angle[new_touch.id],
				tch_vct[new_touch.id].cnt);
		}

		if((tch_vct[new_touch.id].cnt >= 3) && (time_after_autocal_enable == 0)){
			check_chip_channel(mxt);
			if (((chk_touch_cnt < 7) || (chk_touch_cnt > 15)) || ((chk_touch_cnt >= 7) && ((chk_antitouch_cnt >= 4) &&(chk_antitouch_cnt < (chk_touch_cnt + 3))))) {
				for(i=0;i < 10; i++) {
					tch_vct[i].cnt = 0;
				}
			} else {
				for(i=0;i < 10; i++) {
					tch_vct[i].cnt = 0;
				}

				/* T8_TCHAUTOCAL  */
				//if (debug >= DEBUG_MESSAGES)
					pr_info("[TSP] Floating metal Suppressed!! Autocal = 3\n");
				error = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_TCHAUTOCAL, 3);
				if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

				time_after_autocal_enable = jiffies_to_msecs(jiffies);
			}
		} 
		else if ((tch_vct[new_touch.id].cnt >= 4) && (time_after_autocal_enable == 1)){
			for(i=0;i < 10; i++) {
				tch_vct[i].cnt = 0;
			}
		}

		if (time_after_autocal_enable != 0) {
			if ((jiffies_to_msecs(jiffies) - time_after_autocal_enable) > 1500) {
				coin_check_count++;
				if (debug >= DEBUG_MESSAGES)
					pr_info("[TSP] Floating metal Suppressed time out!! Autocal = 0, (%d), coin_check_count = %d \n", 
						(jiffies_to_msecs(jiffies) - time_after_autocal_enable), 
						coin_check_count);
				
				/* T8_TCHAUTOCAL  */
				error = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_TCHAUTOCAL, 0);
				if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

				time_after_autocal_enable = 0;

				for(i=0;i < 10; i++) {
					tch_vct[i].cnt = 0;
				}
			}
		}
		old_touch.id = new_touch.id ;
		old_touch.status[new_touch.id]	=  new_touch.status[new_touch.id];
		old_touch.xpos[new_touch.id]	=  new_touch.xpos[new_touch.id];
		old_touch.ypos[new_touch.id]	=  new_touch.ypos[new_touch.id];
		old_touch.area[new_touch.id]	=  new_touch.area[new_touch.id];
		old_touch.amp[new_touch.id]		=  new_touch.amp[new_touch.id];	  
	}
	 /************************************************************************
							  end 
	  ************************************************************************/

	if (status & MXT_MSGB_T9_DETECT) {   /* case 1: detected */
		
		touch_message_flag = 1;
						
		mtouch_info[touch_id].pressure = message[MXT_MSG_T9_TCHAMPLITUDE];  /* touch amplitude */
		mtouch_info[touch_id].x = (int16_t)xpos;
		mtouch_info[touch_id].y = (int16_t)ypos;

		if (status & MXT_MSGB_T9_PRESS) { 
			pressed_or_released = 1;  /* pressed */
#if defined(MXT_DRIVER_FILTER)
			equalize_coordinate(1, touch_id, &mtouch_info[touch_id].x, &mtouch_info[touch_id].y);
#endif
		} else if (status & MXT_MSGB_T9_MOVE) { 
#if defined(MXT_DRIVER_FILTER)
			equalize_coordinate(0, touch_id, &mtouch_info[touch_id].x, &mtouch_info[touch_id].y);
#endif
		}
	} else if (status & MXT_MSGB_T9_RELEASE) {   /* case 2: released */
		pressed_or_released = 1;
		mtouch_info[touch_id].pressure = 0;
	} else if (status & MXT_MSGB_T9_SUPPRESS) {   /* case 3: suppressed */
	  /*
	     * Atmel's recommendation:
	     * In the case of supression,
	     * mxt224 chip doesn't make a release event.
	     * So we need to release them forcibly.
	    */
		if (debug >= DEBUG_MESSAGES)
			pr_info("[TSP] Palm(T9) Suppressed !!! \n");
		facesup_message_flag_T9 = 1;
		pressed_or_released = 1;
		mtouch_info[touch_id].pressure = 0;
	} else { 
		pr_err("[TSP] Unknown status (0x%x)", status);
		
		if(facesup_message_flag_T9 == 1) { 
			facesup_message_flag_T9 = 0;
			if (debug >= DEBUG_MESSAGES)
				pr_info("[TSP] Palm(T92) Suppressed !!! \n");
		}
		
	}

	/*only get size , id would use TRACKING_ID*/
	mtouch_info[touch_id].size = message[MXT_MSG_T9_TCHAREA];

	if (prev_touch_id >= touch_id || pressed_or_released) { 
		for (i = 0; i < MXT_MAX_NUM_TOUCHES; ++i) { 
			if (mtouch_info[i].pressure == -1)
				continue;

			/* Normalize pressure : Don't use pressure O3,O4,O5 TSP Panel on MXT224 Serise */
			if (mtouch_info[i].pressure > 0) {
				mtouch_info[i].pressure = 30;
			} 
			
			/* ADD TRACKING_ID*/
			if((TOUCH_3KEY_AREA_Y_BOTTOM > mtouch_info[i].y) && 
				(TOUCH_3KEY_AREA_Y_TOP < mtouch_info[i].y)) {
				Process_Touch_3KEY(mxt, mtouch_info, i);
			}
			else {				
				if (mtouch_info[i].pressure == 0) { 	/* if released */
					mtouch_info[i].pressure = -1;
				} else { 
					REPORT_MT(i+1, mtouch_info[i].x, mtouch_info[i].y, mtouch_info[i].pressure, mtouch_info[i].size);
					chkpress ++;
				}
			}
		}
		input_report_key(input, BTN_TOUCH, !!chkpress);
		if (chkpress == 0)
			input_mt_sync(input);
		input_sync(input);  /* TO_CHK: correct position? */
	}
	prev_touch_id = touch_id;

	
	/* simple touch log */
	if (debug >= DEBUG_MESSAGES) { 
		if (status & MXT_MSGB_T9_SUPPRESS) { 
			pr_info("[TSP] Suppress\n");
			pr_info("[TSP] r (%d,%d) %d No.%d\n", xpos, ypos, touch_id, chkpress);

		} else { 
			if (status & MXT_MSGB_T9_PRESS) { 
				pr_info("[TSP] P (%d,%d) %d No.%d amp=%d area=%d\n", xpos, ypos, touch_id, chkpress, message[MXT_MSG_T9_TCHAMPLITUDE], message[MXT_MSG_T9_TCHAREA]);
			} else if (status & MXT_MSGB_T9_RELEASE) { 
				pr_info("[TSP] r (%d,%d) %d No.%d\n", xpos, ypos, touch_id, chkpress);
			}
		}
	}

	/* detail touch log */
	if (debug >= DEBUG_TRACE) { 
		char msg[64] = { 0};
		char info[64] = { 0};
		if (status & MXT_MSGB_T9_SUPPRESS) { 
			strcpy(msg, "[TSP] Suppress: ");
		} else { 
			if (status & MXT_MSGB_T9_DETECT) { 
				strcpy(msg, "Detect(");
				if (status & MXT_MSGB_T9_PRESS)
					strcat(msg, "P");
				if (status & MXT_MSGB_T9_MOVE)
					strcat(msg, "M");
				if (status & MXT_MSGB_T9_AMP)
					strcat(msg, "A");
				if (status & MXT_MSGB_T9_VECTOR)
					strcat(msg, "V");
				strcat(msg, "): ");
			} else if (status & MXT_MSGB_T9_RELEASE) { 
				strcpy(msg, "Release: ");
			} else { 
				strcpy(msg, "[!] Unknown status: ");
			}
		}
		sprintf(info, "[TSP] (%d,%d) amp=%d, size=%d, id=%d", xpos, ypos,
			message[MXT_MSG_T9_TCHAMPLITUDE], message[MXT_MSG_T9_TCHAREA], touch_id);
		strcat(msg, info);
		pr_info("%s\n", msg);
	}

	if(cal_check_flag == 1) {
		/* If chip has recently calibrated and there are any touch or face suppression
		* messages, we must confirm if the calibration is good. */
		if(touch_message_flag) {
			if(timer_flag == DISABLE) {
				timer_flag = ENABLE;
				timer_ticks = 0u;
				ts_100ms_timer_start(mxt);
			}
			if (mxt_time_point == 0) 
				mxt_time_point = jiffies_to_msecs(jiffies);
			check_chip_calibration(mxt);
		}
	}
#if 0
	if(mxt->check_auto_cal == 5) { 
		if (debug >= DEBUG_MESSAGES) 
			pr_info("[TSP] Autocal = 0 \n");
		mxt->check_auto_cal = 0;
		error = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_TCHAUTOCAL, 0);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);
	}
#endif
	return;
}

void process_T15_message(u8 *message, struct mxt_data *mxt)
{ 
	/*
	TODO : To use key array.
	*/
}

static void mxt_palm_recovery(struct work_struct *work)
{
	struct	mxt_data *mxt;
	int error = 0;
	mxt = container_of(work, struct mxt_data, config_dwork.work);
	/*client = mxt->client;*/
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] %s \n", __func__);

	if( mxt->check_auto_cal == 1)
	{
		mxt->check_auto_cal = 2;
		/* T8_ATCHCALST */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALST,
			T8_ATCHCALST);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

		/* T8_ATCHCALSTHR */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALSTHR,
			T8_ATCHCALSTHR);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

		/* T8_ATCHFRCCALTHR */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALTHR,
			T8_ATCHFRCCALTHR);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);
		
		/* T8_ATCHFRCCALRATIO */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALRATIO,
			T8_ATCHFRCCALRATIO);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

		/* T9_NUMTOUCH */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_NUMTOUCH,
			T9_NUMTOUCH);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);
		
	} else if(mxt->check_auto_cal == 2) { 
		mxt->check_auto_cal = 1;
		/* T8_ATCHCALST */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALST,
			T8_ATCHCALST);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

		/* T8_ATCHCALSTHR */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALSTHR,
			T8_ATCHCALSTHR);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

		/* T8_ATCHFRCCALTHR */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALTHR,
			0);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);
		
		/* T8_ATCHFRCCALRATIO */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALRATIO,
			0);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);

		/* T9_NUMTOUCH */
		error = mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_NUMTOUCH,
			0);
		if (error < 0) pr_err("[TSP] %s, %d Error!!\n", __func__, __LINE__);
			
	/* Case 3 : Autocalibration enabled */
	} else if(mxt->check_auto_cal == 3) {
		mxt->check_auto_cal = 5;

	/* Case 4 : Calibrated from none Calibraton command*/
	} else if(mxt->check_auto_cal == 4) {
	        mxt->check_auto_cal = 0;
		facesup_message_flag  = 0;
	}
}

void process_T20_message(u8 *message, struct mxt_data *mxt)
{ 
	struct	input_dev *input;
	u8  status = false;

	input = mxt->input;
	status = message[MXT_MSG_T20_STATUS];

	if (message[MXT_MSG_T20_STATUS] == MXT_MSGB_T20_FACE_SUPPRESS) { 
		palm_release_flag = false;
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Palm(T20) Suppressed !!! \n");
		// 0506 LBK
		if (facesup_message_flag && timer_flag) 
			return;
		
		check_chip_palm(mxt);

		if(facesup_message_flag)
		{
			/* 100ms timer Enable */	
			timer_flag = ENABLE;
			timer_ticks = 0;
			ts_100ms_timer_start(mxt);
			klogi_if("[TSP] Palm(T20) Timer start !!! \n");
		}
	} else {
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Palm(T20) Released !!! \n");
		
		palm_release_flag = true;
		facesup_message_flag = 0;
		/* 100ms timer disable */
		timer_flag = DISABLE;
		timer_ticks = 0;
		ts_100ms_timer_stop(mxt);		
	}
	return;
}


int process_message(u8 *message, u8 object, struct mxt_data *mxt)
{ 

	struct i2c_client *client;

	u8  status, state;
	/*
	u16 xpos = 0xFFFF;
	u16 ypos = 0xFFFF;
	u8  event;
	*/
	u8  length;
	u8  report_id;

	client = mxt->client;
	length = mxt->message_size;
	report_id = message[0];

	if (debug >= DEBUG_TRACE)
		pr_info("process_message 0: (0x%x) 1:(0x%x) object:(%d)", message[0], message[1], object);

	switch (object) { 
	case MXT_PROCG_NOISESUPPRESSION_T22:
		state = message[4];
		if (state == 0x05) {	/* error state */
			if (debug >= DEBUG_MESSAGES) 
				dev_info(&client->dev, "[TSP] NOISESUPPRESSION_T22 error\n");
		}
		break;
	case MXT_GEN_COMMANDPROCESSOR_T6:
		status = message[1];
		if (status & MXT_MSGB_T6_COMSERR) { 
			dev_err(&client->dev, "[TSP] maXTouch checksum error\n");
		}
		if (status & MXT_MSGB_T6_CFGERR) { 
			dev_err(&client->dev, "[TSP] maXTouch configuration error\n");
			reset_chip(mxt, RESET_TO_NORMAL);
			msleep(250);
			/* re-configurate */
			mxt_config_settings(mxt);
			/* backup to nv memory */
			backup_to_nv(mxt);
			/* forces a reset of the chipset */
			reset_chip(mxt, RESET_TO_NORMAL);
			msleep(250); /*need 250ms*/
		}
		if (status & MXT_MSGB_T6_CAL) { 
			if (debug >= DEBUG_MESSAGES) 
				dev_info(&client->dev, "[TSP] maXTouch calibration in progress(cal_check_flag = %d)\n",cal_check_flag);

			if(cal_check_flag == 0)
			{
				/* ATMEL_DEBUG 0406 */
				mxt->check_auto_cal = 4;
				/* mxt_palm_recovery(mxt); */
				cancel_delayed_work(&mxt->config_dwork);
				schedule_delayed_work(&mxt->config_dwork, 0);
				cal_check_flag = 1;
				recal_comp_flag = 1;
			}
 		}
		if (status & MXT_MSGB_T6_SIGERR) { 
			dev_err(&client->dev,
				"[TSP] maXTouch acquisition error\n");
#ifdef MXT_ESD_WORKAROUND
			mxt_release_all_fingers(mxt);
			mxt_release_all_keys(mxt);
			mxt_force_reset(mxt);
#endif
		}
		if (status & MXT_MSGB_T6_OFL) { 
			dev_err(&client->dev, "[TSP] maXTouch cycle overflow\n");
		}
		if (status & MXT_MSGB_T6_RESET) { 
			if (debug >= DEBUG_MESSAGES) 
				dev_info(&client->dev, "[TSP] maXTouch chip reset\n");
		}
		if (status == MXT_MSG_T6_STATUS_NORMAL) { 
			if (debug >= DEBUG_MESSAGES) 
				dev_info(&client->dev, "[TSP] maXTouch status normal\n");
#if defined(MXT_FACTORY_TEST) || defined(MXT_FIRMUP_ENABLE)
				/*check if firmware started*/
				if (mxt->firm_status_data == 1) { 
					if (debug >= DEBUG_MESSAGES) 
						dev_info(&client->dev,
							"maXTouch mxt->firm_normal_status_ack after firm up\n");
					/*got normal status ack*/
					mxt->firm_normal_status_ack = 1;
				}
#endif

		}
		break;

	case MXT_TOUCH_MULTITOUCHSCREEN_T9:
		process_T9_message(message, mxt);
		break;

	case MXT_TOUCH_KEYARRAY_T15:
		process_T15_message(message, mxt);
		break;

	case MXT_PROCI_GRIPSUPPRESSION_T20:
		process_T20_message(message, mxt);
		if (debug >= DEBUG_TRACE)
			dev_info(&client->dev, "[TSP] Receiving Touch suppression msg\n");
		break;

	case MXT_SPT_SELFTEST_T25:
		if (debug >= DEBUG_TRACE) { 
			dev_info(&client->dev,"[TSP] Receiving Self-Test msg\n");
		}

		if (message[MXT_MSG_T25_STATUS] == MXT_MSGR_T25_OK) { 
			if (debug >= DEBUG_TRACE)
				dev_info(&client->dev,
				"[TSP] maXTouch: Self-Test OK\n");

		} else  { 
			dev_err(&client->dev,
				"[TSP] maXTouch: Self-Test Failed [%02x]:"
				"{ %02x,%02x,%02x,%02x,%02x}\n",
				message[MXT_MSG_T25_STATUS],
				message[MXT_MSG_T25_STATUS + 0],
				message[MXT_MSG_T25_STATUS + 1],
				message[MXT_MSG_T25_STATUS + 2],
				message[MXT_MSG_T25_STATUS + 3],
				message[MXT_MSG_T25_STATUS + 4]
				);
		}
		break;

	case MXT_SPT_CTECONFIG_T28:
		if (debug >= DEBUG_TRACE)
			dev_info(&client->dev,
			"[TSP] Receiving CTE message...\n");
		status = message[MXT_MSG_T28_STATUS];
		if (status & MXT_MSGB_T28_CHKERR)
			dev_err(&client->dev,
			"[TSP] maXTouch: Power-Up CRC failure\n");

		break;
	default:
		if (debug >= DEBUG_TRACE)
			dev_info(&client->dev,"[TSP] maXTouch: Unknown message!\n");
			break;
	}

	return 0;
}


#ifdef MXT_THREADED_IRQ
/* Processes messages when the interrupt line (CHG) is asserted. */

static void mxt_threaded_irq_handler(struct mxt_data *mxt)
{ 
	struct	i2c_client *client;

	u8	*message;
	u16	message_length;
	u16	message_addr;
	u8	report_id = 0;
	u8	object;
	int	error;
	bool need_reset = false;
	int	i;

	message = NULL;
	client = mxt->client;
	message_addr = 	mxt->msg_proc_addr;
	message_length = mxt->message_size;
	if (message_length < 256) { 
		message = kmalloc(message_length, GFP_KERNEL);
		if (message == NULL) { 
			dev_err(&client->dev, "[TSP] Error allocating memory\n");
			return;
		}
	} else { 
		dev_err(&client->dev, "[TSP] Message length larger than 256 bytes not supported\n");
	}

	if (debug >= DEBUG_TRACE)
		dev_info(&mxt->client->dev, "[TSP] maXTouch worker active: \n");

	do { 
		/* Read next message */
		mxt->message_counter++;
		/* Reread on failure! */
		for (i = I2C_RETRY_COUNT; i > 0; i--) { 
			/* note: changed message_length to 8 in ver0.9*/
			error = mxt_read_block(client, message_addr, 8/*message_length*/, message);
			if (error >= 0)
				break;
			mxt->read_fail_counter++;
			pr_alert("[TSP] mXT: message read failed!\n");
			/* Register read failed */
			dev_err(&client->dev, "[TSP] Failure reading maxTouch device\n");
		}
		if (i == 0) need_reset = true;

		report_id = message[0];
		if (debug >= DEBUG_RAW) { 
			pr_info("[TSP] %s message [%08x]:",
				REPORT_ID_TO_OBJECT_NAME(report_id),
				mxt->message_counter
				);
			for (i = 0; i < message_length; i++) { 
				pr_info("0x%02x ", message[i]);;
			}
			pr_info("\n");
		}

		if ((report_id != MXT_END_OF_MESSAGES) && (report_id != 0)) { 

			for (i = 0; i < message_length; i++)
				mxt->last_message[i] = message[i];

			if (down_interruptible(&mxt->msg_sem)) { 
				pr_warning("[TSP] mxt_worker Interrupted "
					"while waiting for msg_sem!\n");
				kfree(message);
				return;
			}
			mxt->new_msgs = 1;
			up(&mxt->msg_sem);
			wake_up_interruptible(&mxt->msg_queue);
			/* Get type of object and process the message */
			object = mxt->rid_map[report_id].object;
			process_message(message, object, mxt);
		}

	} while ((report_id != MXT_END_OF_MESSAGES) && (report_id != 0));
	kfree(message);

	/* TSP reset */
	if (need_reset) { 
		mxt_release_all_fingers(mxt);
		mxt_release_all_keys(mxt);
		mxt_force_reset(mxt);
	}
}

static irqreturn_t mxt_threaded_irq(int irq, void *_mxt)
{ 
	struct	mxt_data *mxt = _mxt;
	mxt->irq_counter++;
	mxt_threaded_irq_handler(mxt);
	return IRQ_HANDLED;
}


/* boot initial delayed work */
static void mxt_boot_delayed_initial(struct work_struct *work)
{ 
	int error;
	struct	mxt_data *mxt;
	mxt = container_of(work, struct mxt_data, initial_dwork.work);

	if (debug >= DEBUG_MESSAGES) 
				dev_info(&mxt->client->dev, "[TSP] %s\n", __func__);
	
	calibrate_chip(mxt);
	
	if (mxt->irq) { 
	/* Try to request IRQ with falling edge first. This is
		* not always supported. If it fails, try with any edge. */
#ifdef MXT_THREADED_IRQ
		error = request_threaded_irq(mxt->irq,
			NULL,
			mxt_threaded_irq,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			mxt->client->dev.driver->name,
			mxt);
		if (error < 0) { 
			error = request_threaded_irq(mxt->irq,
				NULL,
				mxt_threaded_irq,
				IRQF_DISABLED,
				mxt->client->dev.driver->name,
				mxt);
		}
#else
		error = request_irq(mxt->irq,
			mxt_irq_handler,
			IRQF_TRIGGER_FALLING,
			mxt->client->dev.driver->name,
			mxt);
		if (error < 0) { 
			error = request_irq(mxt->irq,
				mxt_irq_handler,
				0,
				mxt->client->dev.driver->name,
				mxt);
		}
#endif
		if (error < 0) { 
			dev_err(&mxt->client->dev,
				"[TSP] failed to allocate irq %d\n", mxt->irq);
		}
	}

	if (debug > DEBUG_INFO)
		dev_info(&mxt->client->dev, "[TSP] touchscreen, irq %d\n", mxt->irq);

	return;	
}

#else
/* Processes messages when the interrupt line (CHG) is asserted. */
static void mxt_worker(struct work_struct *work)
{ 
	struct	mxt_data *mxt;
	struct	i2c_client *client;

	u8	*message;
	u16	message_length;
	u16	message_addr;
	u8	report_id;
	u8	object;
	int	error;
	int	i;

	message = NULL;
	mxt = container_of(work, struct mxt_data, dwork.work);
	client = mxt->client;
	message_addr = mxt->msg_proc_addr;
	message_length = mxt->message_size;

	if (message_length < 256) { 
		message = kmalloc(message_length, GFP_KERNEL);
		if (message == NULL) { 
			dev_err(&client->dev, "[TSP] Error allocating memory\n");
			return;
		}
	} else { 
		dev_err(&client->dev,
			"[TSP] Message length larger than 256 bytes not supported\n");
	}

	if (debug >= DEBUG_TRACE)
		dev_info(&mxt->client->dev, "[TSP] maXTouch worker active: \n");

	do { 
		/* Read next message */
		mxt->message_counter++;
		/* Reread on failure! */
		for (i = 1; i < I2C_RETRY_COUNT; i++) { 
			/* note: changed message_length to 8 in ver0.9 */
			error = mxt_read_block(client, message_addr, 8/*message_length*/, message);
			if (error >= 0)
				break;
			mxt->read_fail_counter++;
			pr_alert("[TSP] mXT: message read failed!\n");
			/* Register read failed */
			dev_err(&client->dev,
				"[TSP] Failure reading maxTouch device\n");
		}

		report_id = message[0];
		if (debug >= DEBUG_RAW) { 
			pr_info("[TSP] %s message [%08x]:",
				REPORT_ID_TO_OBJECT_NAME(report_id),
				mxt->message_counter
				);
			for (i = 0; i < message_length; i++) { 
				pr_info("0x%02x ", message[i]);;
			}
			pr_info("\n");
		}

		if ((report_id != MXT_END_OF_MESSAGES) && (report_id != 0)) { 

			for (i = 0; i < message_length; i++)
				mxt->last_message[i] = message[i];

			if (down_interruptible(&mxt->msg_sem)) { 
				pr_warning("[TSP] mxt_worker Interrupted "
					"while waiting for msg_sem!\n");
				kfree(message);
				return;
			}
			mxt->new_msgs = 1;
			up(&mxt->msg_sem);
			wake_up_interruptible(&mxt->msg_queue);
			/* Get type of object and process the message */
			object = mxt->rid_map[report_id].object;
			process_message(message, object, mxt);
		}

	} while ((report_id != MXT_END_OF_MESSAGES) && (report_id != 0));

	kfree(message);
}

/*
* The maXTouch device will signal the host about a new message by asserting
* the CHG line. This ISR schedules a worker routine to read the message when
* that happens.
*/
static irqreturn_t mxt_irq_handler(int irq, void *_mxt)
{ 
	struct	mxt_data *mxt = _mxt;
	unsigned long	flags;
	mxt->irq_counter++;
	spin_lock_irqsave(&mxt->lock, flags);

	if (mxt_valid_interrupt()) { 
		/* Send the signal only if falling edge generated the irq. */
		cancel_delayed_work(&mxt->dwork);
		schedule_delayed_work(&mxt->dwork, 0);
		mxt->valid_irq_counter++;
	} else { 
		mxt->invalid_irq_counter++;
	}
	spin_unlock_irqrestore(&mxt->lock, flags);

	return IRQ_HANDLED;
}
#endif

static ssize_t show_deltas(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{ 
	struct i2c_client *client;
	struct mxt_data *mxt;
	s16     *delta;
	s16     size, read_size;
	u16     diagnostics;
	u16     debug_diagnostics;
	char    *bufp;
	int     x, y;
	int     error;
	u16     *val;

	client = to_i2c_client(dev);
	mxt = i2c_get_clientdata(client);

	/* Allocate buffer for delta's */
	size = mxt->device_info.num_nodes * sizeof(__u16);
	if (mxt->delta == NULL) { 
		mxt->delta = kzalloc(size, GFP_KERNEL);
		if (!mxt->delta) { 
			sprintf(buf, "insufficient memory\n");
			return strlen(buf);
		}
	}

	diagnostics =  T6_REG(MXT_ADR_T6_DIAGNOSTICS);
	debug_diagnostics = T37_REG(2);

	/* Configure T37 to show deltas */
	error = mxt_write_byte(client, diagnostics, MXT_CMD_T6_DELTAS_MODE);
	if (error)
		return error;

	delta = mxt->delta;

	while (size > 0) { 
		read_size = size > 128 ? 128 : size;
		error = mxt_read_block(client,
			debug_diagnostics,
			read_size,
			(__u8 *) delta);
		if (error < 0) { 
			mxt->read_fail_counter++;
			dev_err(&client->dev,
				"[TSP] maXTouch: Error reading delta object\n");
		}
		delta += (read_size / 2);
		size -= read_size;
		/* Select next page */
		mxt_write_byte(client, diagnostics, MXT_CMD_T6_PAGE_UP);
	}

	bufp = buf;
	val  = (s16 *) mxt->delta;
	for (x = 0; x < mxt->device_info.x_size; x++) { 
		for (y = 0; y < mxt->device_info.y_size; y++)
			bufp += sprintf(bufp, "%05d  ",
			(s16) le16_to_cpu(*val++));
		bufp -= 2;	/* No spaces at the end */
		bufp += sprintf(bufp, "\n");
	}
	bufp += sprintf(bufp, "\n");
	return strlen(buf);
}


static ssize_t show_references(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{ 
	struct i2c_client *client;
	struct mxt_data *mxt;
	s16   *reference;
	s16   size, read_size;
	u16   diagnostics;
	u16   debug_diagnostics;
	char  *bufp;
	int   x, y;
	int   error;
	u16   *val;

	client = to_i2c_client(dev);
	mxt = i2c_get_clientdata(client);
	/* Allocate buffer for reference's */
	size = mxt->device_info.num_nodes * sizeof(u16);
	if (mxt->reference == NULL) { 
		mxt->reference = kzalloc(size, GFP_KERNEL);
		if (!mxt->reference) { 
			sprintf(buf, "insufficient memory\n");
			return strlen(buf);
		}
	}

	diagnostics =  T6_REG(MXT_ADR_T6_DIAGNOSTICS);
	debug_diagnostics = T37_REG(2);

	/* Configure T37 to show references */
	mxt_write_byte(client, diagnostics, MXT_CMD_T6_REFERENCES_MODE);
	/* Should check for error */
	reference = mxt->reference;
	while (size > 0) { 
		read_size = size > 128 ? 128 : size;
		error = mxt_read_block(client,
			debug_diagnostics,
			read_size,
			(__u8 *) reference);
		if (error < 0) { 
			mxt->read_fail_counter++;
			dev_err(&client->dev,
				"[TSP] maXTouch: Error reading reference object\n");
		}
		reference += (read_size / 2);
		size -= read_size;
		/* Select next page */
		mxt_write_byte(client, diagnostics, MXT_CMD_T6_PAGE_UP);
	}

	bufp = buf;
	val  = (u16 *) mxt->reference;

	for (x = 0; x < mxt->device_info.x_size; x++) { 
		for (y = 0; y < mxt->device_info.y_size; y++) {
			bufp += sprintf(bufp, "%05d  ", le16_to_cpu(*val)>0?le16_to_cpu(*val)-16384:0);
			val++;
		}
		bufp -= 2; /* No spaces at the end */
		bufp += sprintf(bufp, "\n");
	}
	bufp += sprintf(bufp, "\n");
	return strlen(buf);
}

static ssize_t show_device_info(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{ 
	struct i2c_client *client;
	struct mxt_data *mxt;
	char *bufp;

	client = to_i2c_client(dev);
	mxt = i2c_get_clientdata(client);

	bufp = buf;
	bufp += sprintf(bufp,
		"Family:\t\t\t[0x%02x] %s\n",
		mxt->device_info.family_id,
		mxt->device_info.family
		);
	bufp += sprintf(bufp,
		"Variant:\t\t[0x%02x] %s\n",
		mxt->device_info.variant_id,
		mxt->device_info.variant
		);
	bufp += sprintf(bufp,
		"Firmware version:\t[%d.%d], build 0x%02X\n",
		mxt->device_info.major,
		mxt->device_info.minor,
		mxt->device_info.build
		);
	bufp += sprintf(bufp,
		"%d Sensor nodes:\t[X=%d, Y=%d]\n",
		mxt->device_info.num_nodes,
		mxt->device_info.x_size,
		mxt->device_info.y_size
		);
	bufp += sprintf(bufp,
		"Reported resolution:\t[X=%d, Y=%d]\n",
		mxt->pdata->max_x,
		mxt->pdata->max_y
		);
	return strlen(buf);
}

static ssize_t show_stat(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{ 
	struct i2c_client *client;
	struct mxt_data *mxt;
	char *bufp;

	client = to_i2c_client(dev);
	mxt = i2c_get_clientdata(client);

	bufp = buf;
	bufp += sprintf(bufp,
		"Interrupts:\t[VALID=%d ; INVALID=%d]\n",
		mxt->valid_irq_counter,
		mxt->invalid_irq_counter
		);
	bufp += sprintf(bufp, "Messages:\t[%d]\n", mxt->message_counter);
	bufp += sprintf(bufp, "Read Failures:\t[%d]\n", mxt->read_fail_counter);
	return strlen(buf);
}

static ssize_t show_object_info(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{ 
	struct i2c_client	*client;
	struct mxt_data		*mxt;
	char			*bufp;
	struct mxt_object	*object_table;
	int			i;

	client = to_i2c_client(dev);
	mxt = i2c_get_clientdata(client);
	object_table = mxt->object_table;

	bufp = buf;

	bufp += sprintf(bufp, "maXTouch: %d Objects\n",
		mxt->device_info.num_objs);

	for (i = 0; i < mxt->device_info.num_objs; i++) { 
		if (object_table[i].type != 0) { 
			bufp += sprintf(bufp,
				"Type:\t\t[%d]: %s\n",
				object_table[i].type,
				object_type_name[object_table[i].type]);
			bufp += sprintf(bufp,
				"Address:\t0x%04X\n",
				object_table[i].chip_addr);
			bufp += sprintf(bufp,
				"Size:\t\t%d Bytes\n",
				object_table[i].size);
			bufp += sprintf(bufp,
				"Instances:\t%d\n",
				object_table[i].instances
				);
			bufp += sprintf(bufp,
				"Report Id's:\t%d\n\n",
				object_table[i].num_report_ids);
		}
	}
	return strlen(buf);
}

static ssize_t show_report_id(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{ 
	struct i2c_client    *client;
	struct mxt_data      *mxt;
	struct report_id_map *report_id;
	int                  i;
	int                  object;
	char                 *bufp;

	client    = to_i2c_client(dev);
	mxt       = i2c_get_clientdata(client);
	report_id = mxt->rid_map;

	bufp = buf;
	for (i = 0 ; i < mxt->report_id_count ; i++) { 
		object = report_id[i].object;
		bufp += sprintf(bufp, "Report Id [%03d], object [%03d], "
			"instance [%03d]:\t%s\n",
			i,
			object,
			report_id[i].instance,
			object_type_name[object]);
	}
	return strlen(buf);
}

static ssize_t set_debug(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	int state;

	sscanf(buf, "%d", &state);

	switch (state) { 
	case	DEBUG_INFO:
		pr_info("[TSP] DEBUG_INFO : Default TSP information messages enabled.");
		debug = DEBUG_INFO;
		break;
	case	DEBUG_VERBOSE:
		pr_info("[TSP] DEBUG_VERBOSE : Verbose TSP information messages enabled.");
		debug = DEBUG_VERBOSE;
		break;
	case	DEBUG_MESSAGES:
		pr_info("[TSP] DEBUG_MESSAGES : Low-level TSP information messages enabled.");
		debug = DEBUG_MESSAGES;
		break;
	case	DEBUG_RAW:
		pr_info("[TSP] DEBUG_RAW : Raw TSP information messages enabled.");
		debug = DEBUG_VERBOSE;
		break;
	case	DEBUG_TRACE:
		pr_info("[TSP] DEBUG_TRACE : Trase TSP information messages enabled.");
		debug = DEBUG_TRACE;
		break;
	default:
		pr_info("[TSP] DEBUG : Invalid value. (INFO:1/VERBOSE:2/MESSAGES:5/RAW:8/TRACE:10)");
		break;
	}

	return count;
}

static ssize_t show_firmware(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct mxt_data *mxt = dev_get_drvdata(dev);
	u8 val[7];

	mxt_read_block(mxt->client, MXT_ADDR_INFO_BLOCK, 7, (u8 *)val);
	mxt->device_info.major = ((val[2] >> 4) & 0x0F);
	mxt->device_info.minor = (val[2] & 0x0F);
	mxt->device_info.build	= val[3];

	return snprintf(buf, PAGE_SIZE,
		"Atmel %s Firmware version [%d.%d] Build %d\n",
		mxt224_variant,
		mxt->device_info.major,
		mxt->device_info.minor,
		mxt->device_info.build);
}

static ssize_t store_firmware(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	int state, ret;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	if (sscanf(buf, "%i", &state) != 1 || (state < 0 || state > 1))
		return -EINVAL;

	/* prevents the system from entering suspend during updating */
	wake_lock(&mxt->wakelock);
	disable_irq(mxt->client->irq);

	mxt_load_firmware(dev, MXT224_FIRMWARE);
	mdelay(100);

	/* chip reset and re-initialize */
	mxt->pdata->suspend_platform_hw(mxt->pdata);
	mdelay(50);
	mxt->pdata->resume_platform_hw(mxt->pdata);

	ret = mxt_identify(mxt->client, mxt);
	if (ret >= 0) {
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] mxt_identify Sucess ");
	} else pr_err("[TSP] mxt_identify Fail ");

	ret = mxt_read_object_table(mxt->client, mxt);
	if (ret >= 0) {
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] mxt_read_object_table Sucess ");
	} else pr_err("[TSP] mxt_read_object_table Fail ");

	enable_irq(mxt->client->irq);
	wake_unlock(&mxt->wakelock);

	return count;
}


#ifdef MXT_FIRMUP_ENABLE
static int set_mxt_auto_update_exe(struct device *dev)
{ 
	int error;
	struct mxt_data *mxt = dev_get_drvdata(dev);
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] set_mxt_auto_update_exe \n");

	error = mxt_load_firmware(&mxt->client->dev, MXT224_FIRMWARE);

	if (error >= 0) { 
		mxt->firm_status_data = 2;	/*firmware update success */
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Reprogram done : Firmware update Success~~~~~~~~~~\n");
		/* for stable-time */
		mdelay(100);
	} else { 
		mxt->firm_status_data = 3;	/* firmware update Fail */
		pr_err("[TSP] Reprogram done : Firmware update Fail~~~~~~~~~~\n");
	}

	/* chip reset and re-initialize */
	mxt->pdata->suspend_platform_hw(mxt->pdata);
	mdelay(50);
	mxt->pdata->resume_platform_hw(mxt->pdata);

	error = mxt_identify(mxt->client, mxt);
	if (error >= 0) {
		if (debug >= DEBUG_INFO)
			pr_info("[TSP]  mxt_identify Sucess ");
	} else pr_err("[TSP]  mxt_identify Fail ");

	mxt_read_object_table(mxt->client, mxt);

	return error;
}
#endif

#ifdef MXT_FACTORY_TEST
static void set_mxt_update_exe(struct work_struct *work)
{ 
	struct	mxt_data *mxt;
	int ret, cnt;;
	mxt = container_of(work, struct mxt_data, firmup_dwork.work);
	/*client = mxt->client;*/
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] set_mxt_update_exe \n");


	/*wake_lock(&mxt->wakelock);*/  /* prevents the system from entering suspend during updating */
	disable_irq(mxt->client->irq);
	ret = mxt_load_firmware(&mxt->client->dev, MXT224_FIRMWARE);
	mdelay(100);

	/* chip reset and re-initialize */
	mxt->pdata->suspend_platform_hw(mxt->pdata);
	mdelay(50);
	mxt->pdata->resume_platform_hw(mxt->pdata);

	ret = mxt_identify(mxt->client, mxt);
	if (ret >= 0) {
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] mxt_identify Sucess ");
	} else pr_err("[TSP] mxt_identify Fail ");


	ret = mxt_read_object_table(mxt->client, mxt);
	if (ret >= 0) {
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] mxt_read_object_table Sucess ");
	}else pr_err("[TSP] mxt_read_object_table Fail ");

	enable_irq(mxt->client->irq);
	/*wake_unlock(&mxt->wakelock);*/

	if (ret >= 0) { 
		for (cnt = 10; cnt > 0; cnt--) { 
			if (mxt->firm_normal_status_ack == 1) { 
				mxt->firm_status_data = 2;	/* firmware update success */
				if (debug >= DEBUG_INFO)
					pr_info("[TSP] Reprogram done : Firmware update Success \n");
				break;
			} else { 
				if (debug >= DEBUG_INFO)
					pr_info("[TSP] Reprogram done , but not yet normal status : 3s delay needed \n");
				msleep(500);/* 3s delay */
			}

		}
		if (cnt == 0) { 
			mxt->firm_status_data = 3;	/* firmware update Fail */
			pr_err("[TSP] Reprogram done : Firmware update Fail \n");
		}
	} else { 
		mxt->firm_status_data = 3;	/* firmware update Fail */
		pr_err("[TSP] Reprogram done : Firmware update Fail \n");
	}
	mxt->firm_normal_status_ack = 0;
}

static ssize_t set_mxt_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int count;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	if (debug >= DEBUG_INFO)
		pr_info("[TSP] touch firmware update \n");
	mxt->firm_status_data = 1;	/* start firmware updating */
	cancel_delayed_work(&mxt->firmup_dwork);
	schedule_delayed_work(&mxt->firmup_dwork, 0);

	if (mxt->firm_status_data == 3) { 
		count = sprintf(buf, "FAIL\n");
	} else
		count = sprintf(buf, "OK\n");
	return count;
}

/*Current(Panel) Version*/
static ssize_t set_mxt_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct mxt_data *mxt = dev_get_drvdata(dev);
	int error, cnt;
	u8 val[7];
	u8 fw_current_version;

	for (cnt = 10; cnt > 0; cnt--) { 
		error = mxt_read_block(mxt->client, MXT_ADDR_INFO_BLOCK, 7, (u8 *)val);
		if (error < 0) { 
			pr_err("[TSP] Atmel touch version read fail it will try 2s later\n");
			msleep(2000);
		} else { 
			break;
		}
	}
	if (cnt == 0) { 
		pr_err("[TSP] set_mxt_firm_version_show failed!!!\n");
		fw_current_version = 0;
	}

	mxt->device_info.major = ((val[2] >> 4) & 0x0F);
	mxt->device_info.minor = (val[2] & 0x0F);
	mxt->device_info.build	= val[3];
	fw_current_version = val[2];
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] Atmel %s Firmware version [%d.%d](%d) Build %d\n",
			mxt224_variant,
			mxt->device_info.major,
			mxt->device_info.minor,
			fw_current_version,
			mxt->device_info.build);

	return sprintf(buf, "Ver %d.%d Build 0x%x\n", mxt->device_info.major, mxt->device_info.minor, mxt->device_info.build);
}

/* Last(Phone) Version */
extern u8 firmware_latest[];
static ssize_t set_mxt_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	u8 fw_latest_version;
	fw_latest_version = firmware_latest[0];
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] Atmel Last firmware version is 0x%02x\n", fw_latest_version);
	return sprintf(buf, "Ver %d.%d Build 0x%x\n", (firmware_latest[0]>>4)&0x0f, firmware_latest[0]&0x0f, firmware_latest[1]);
}
static ssize_t set_mxt_firm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 

	int count;
	struct mxt_data *mxt = dev_get_drvdata(dev);
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] Enter firmware_status_show by Factory command \n");

	if (mxt->firm_status_data == 1) { 
		count = sprintf(buf, "Downloading\n");
	} else if (mxt->firm_status_data == 2) { 
		count = sprintf(buf, "PASS\n");
	} else if (mxt->firm_status_data == 3) { 
		count = sprintf(buf, "FAIL\n");
	} else
		count = sprintf(buf, "PASS\n");

	return count;

}

static ssize_t threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	u8 val;
	struct mxt_data *mxt = dev_get_drvdata(dev);
	mxt_read_byte(mxt->client,
		MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
		&val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t threshold_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t size)
{ 
	struct mxt_data *mxt = dev_get_drvdata(dev);
	int i;
	if (sscanf(buf, "%d", &i) == 1) { 
		wake_lock(&mxt->wakelock);  /* prevents the system from entering suspend during updating */
		disable_irq(mxt->client->irq);   /* disable interrupt */
		mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
			i);
		/* backup to nv memory */
		backup_to_nv(mxt);
		/* forces a reset of the chipset */
		reset_chip(mxt, RESET_TO_NORMAL);
		msleep(250);  /* 250ms */

		enable_irq(mxt->client->irq);    /* enable interrupt */
		wake_unlock(&mxt->wakelock);
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] threshold is changed to %d\n", i);
	} else
		pr_err("[TSP] threshold write error\n");

	return size;
}
#endif

#if ENABLE_NOISE_TEST_MODE
uint8_t read_uint16_t(u16 Address, u16 *Data, struct mxt_data *mxt)
{ 
	uint8_t status;
	uint8_t temp[2];

	status = mxt_read_block(mxt->client, Address, 2, temp);
	*Data = ((uint16_t)temp[1] << 8) + (uint16_t)temp[0];

	return status;
}
int  read_dbg_data(u8 dbg_mode , u8 node, u16 *dbg_data, struct mxt_data *mxt)
{ 
	int  status;
	u8 mode, page, i;
	u8 read_page;
	u16 read_point;
	u16	diagnostics;
	u16 diagnostic_addr;

	diagnostic_addr = MXT_BASE_ADDR(MXT_DEBUG_DIAGNOSTICS_T37);
	diagnostics =  T6_REG(MXT_ADR_T6_DIAGNOSTICS);

	read_page = node / 64;
	node %= 64;
	read_point = (node * 2) + 2;

	/* Page Num Clear */
	mxt_write_byte(mxt->client, diagnostics, MXT_CMD_T6_CTE_MODE);
	msleep(20);
	mxt_write_byte(mxt->client, diagnostics, dbg_mode);
	msleep(20);

	for (i = 0; i < 5; i++) { 
		msleep(20);
		status = mxt_read_byte(mxt->client, diagnostic_addr, &mode);
		if (status == 0) { 
			if (mode == dbg_mode) { 
				break;
			}
		} else { 
			pr_err("[TSP] read mode fail \n");
			return status;
		}
	}



	for (page = 0; page < read_page; page++) { 
		mxt_write_byte(mxt->client, diagnostics, MXT_CMD_T6_PAGE_UP);
		msleep(10);
	}

	status = read_uint16_t(diagnostic_addr + read_point, dbg_data, mxt);

	msleep(10);

	return status;
}
static ssize_t set_refer0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_refrence = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_REFERENCES_MODE, test_node[0], &qt_refrence, mxt);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_refer1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_refrence = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_REFERENCES_MODE, test_node[1], &qt_refrence, mxt);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_refer2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_refrence = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_REFERENCES_MODE, test_node[2], &qt_refrence, mxt);
	return sprintf(buf, "%u\n", qt_refrence);
}


static ssize_t set_refer3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_refrence = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_REFERENCES_MODE, test_node[3], &qt_refrence, mxt);
	return sprintf(buf, "%u\n", qt_refrence);
}


static ssize_t set_refer4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_refrence = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_REFERENCES_MODE, test_node[4], &qt_refrence, mxt);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_delta0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_delta = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_DELTAS_MODE, test_node[0], &qt_delta, mxt);
	if (qt_delta < 32767) { 
		return sprintf(buf, "%u\n", qt_delta);
	} else	{ 
		qt_delta = 65535 - qt_delta;
		return sprintf(buf, "-%u\n", qt_delta);
	}
}

static ssize_t set_delta1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_delta = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_DELTAS_MODE, test_node[1], &qt_delta, mxt);
	if (qt_delta < 32767) { 
		return sprintf(buf, "%u\n", qt_delta);
	} else	{ 
		qt_delta = 65535 - qt_delta;
		return sprintf(buf, "-%u\n", qt_delta);
	}
}

static ssize_t set_delta2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_delta = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_DELTAS_MODE, test_node[2], &qt_delta, mxt);
	if (qt_delta < 32767) { 
		return sprintf(buf, "%u\n", qt_delta);
	} else { 
		qt_delta = 65535 - qt_delta;
		return sprintf(buf, "-%u\n", qt_delta);
	}
}

static ssize_t set_delta3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_delta = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_DELTAS_MODE, test_node[3], &qt_delta, mxt);
	if (qt_delta < 32767) { 
		return sprintf(buf, "%u\n", qt_delta);
	} else { 
		qt_delta = 65535 - qt_delta;
		return sprintf(buf, "-%u\n", qt_delta);
	}
}

static ssize_t set_delta4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	int  status;
	u16 qt_delta = 0;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	status = read_dbg_data(MXT_CMD_T6_DELTAS_MODE, test_node[4], &qt_delta, mxt);
	if (qt_delta < 32767) { 
		return sprintf(buf, "%u\n", qt_delta);
	} else { 
		qt_delta = 65535 - qt_delta;
		return sprintf(buf, "-%u\n", qt_delta);
	}
}

static ssize_t set_threshold_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	u8 val;
	struct mxt_data *mxt = dev_get_drvdata(dev);

	mxt_read_byte(mxt->client,
		MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR,
		&val);
	return sprintf(buf, "%d\n", val);
}

#endif


static int chk_obj(u8 type)
{ 
	switch (type) { 
		/*	case	MXT_GEN_MESSAGEPROCESSOR_T5:*/
		/*	case	MXT_GEN_COMMANDPROCESSOR_T6:*/
	case	MXT_GEN_POWERCONFIG_T7:
	case	MXT_GEN_ACQUIRECONFIG_T8:
	case	MXT_TOUCH_MULTITOUCHSCREEN_T9:
	case	MXT_TOUCH_KEYARRAY_T15:
	case	MXT_SPT_COMMSCONFIG_T18:
	case	MXT_SPT_GPIOPWM_T19:
	case	MXT_PROCI_GRIPSUPPRESSION_T20:              
	case	MXT_PROCG_NOISESUPPRESSION_T22:
	case	MXT_TOUCH_PROXIMITY_T23:
	case	MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24:
	case	MXT_SPT_SELFTEST_T25:
	case	MXT_SPT_CTECONFIG_T28:
		/* case	MXT_DEBUG_DIAGNOSTICS_T37: */
	case	MXT_USER_INFO_T38:
 		return 0;
	default:
		return -1;
	}
}

static ssize_t show_object(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	/*	struct qt602240_data *data = dev_get_drvdata(dev); */
	/*	struct qt602240_object *object; */
	struct mxt_data *mxt;
	struct mxt_object	 *object_table;

	int count = 0;
	int i, j;
	u8 val;

	mxt = dev_get_drvdata(dev);
	object_table = mxt->object_table;

	for (i = 0; i < mxt->device_info.num_objs; i++) { 
		u8 obj_type = object_table[i].type;

		if (chk_obj(obj_type))
			continue;

		count += sprintf(buf + count, "%s: %d bytes\n",
			object_type_name[obj_type], object_table[i].size);

		for (j = 0; j < object_table[i].size; j++) { 
			mxt_read_byte(mxt->client, MXT_BASE_ADDR(obj_type)+(u16)j, &val);
			count += sprintf(buf + count,
				"  Byte %2d: 0x%02x (%d)\n", j, val, val);
		}

		count += sprintf(buf + count, "\n");
	}

	/* debug only */
	/*
	count += sprintf(buf + count, "%s: %d bytes\n", "debug_config_T0", 32);

	for (j = 0; j < 32; j++) { 
		count += sprintf(buf + count,
			"  Byte %2d: 0x%02x (%d)\n", j, mxt->debug_config[j], mxt->debug_config[j]);
	}
	* */
	

#ifdef MXT_TUNNING_ENABLE
	backup_to_nv(mxt);
#endif

	return count;
}

static ssize_t store_object(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	/*	struct qt602240_data *data = dev_get_drvdata(dev); */
	/*	struct qt602240_object *object; */
	struct mxt_data *mxt;
	/*	struct mxt_object	*object_table;//TO_CHK: not used now */

	unsigned int type, offset, val;
	u16	chip_addr;
	int ret;

	mxt = dev_get_drvdata(dev);

	if ((sscanf(buf, "%u %u %u", &type, &offset, &val) != 3) || (type >= MXT_MAX_OBJECT_TYPES)) { 
		pr_err("Invalid values");
		return -EINVAL;
	}

	if (debug >= DEBUG_INFO)
		pr_info("[TSP] Object type: %u, Offset: %u, Value: %u\n", type, offset, val);


	/* debug only */
	/*
	count += sprintf(buf + count, "%s: %d bytes\n", "debug_config_T0", 32);
	*/


	if(type == 0)
	{
		/* 
		mxt->debug_config[offset] = (u8)val;
		*/
	} else { 
	
	chip_addr = get_object_address(type, 0, mxt->object_table,
		mxt->device_info.num_objs);
	if (chip_addr == 0) { 
		pr_err("[TSP] Invalid object type(%d)!", type);
		return -EIO;
	}

	ret = mxt_write_byte(mxt->client, chip_addr+(u16)offset, (u8)val);
	pr_err("[TSP] store_object result: (%d)\n", ret);
	if (ret < 0) { 
		return ret;
	}
	mxt_read_byte(mxt->client,
		MXT_BASE_ADDR(MXT_USER_INFO_T38)+
		MXT_ADR_T38_CFG_CTRL,
		(u8*)&val);

	if (val == MXT_CFG_DEBUG) { 
		backup_to_nv(mxt);
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] backup_to_nv\n");
	}

	}

	return count;
}

#if 0  /* FOR_TEST */
static ssize_t test_suspend(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	char *bufp;
	struct early_suspend  *fake;

	bufp = buf;
	bufp += sprintf(bufp, "Running early_suspend function...\n");

	fake = kzalloc(sizeof(struct early_suspend), GFP_KERNEL);
	mxt_early_suspend(fake);
	kfree(fake);

	return strlen(buf);
}

static ssize_t test_resume(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	char *bufp;
	struct early_suspend  *fake;

	bufp = buf;
	bufp += sprintf(bufp, "Running late_resume function...\n");

	fake = kzalloc(sizeof(struct early_suspend), GFP_KERNEL);
	mxt_late_resume(fake);
	kfree(fake);

	return strlen(buf);
}
#endif

/* Register sysfs files */

static DEVICE_ATTR(deltas,      S_IRUGO, show_deltas,      NULL);
static DEVICE_ATTR(references,  S_IRUGO, show_references,  NULL);
static DEVICE_ATTR(device_info, S_IRUGO, show_device_info, NULL);
static DEVICE_ATTR(object_info, S_IRUGO, show_object_info, NULL);
static DEVICE_ATTR(report_id,   S_IRUGO, show_report_id,   NULL);
static DEVICE_ATTR(stat,        S_IRUGO, show_stat,        NULL);
static DEVICE_ATTR(debug,       S_IWUSR, NULL, set_debug);
static DEVICE_ATTR(firmware, S_IWUSR|S_IRUGO, show_firmware, store_firmware);
static DEVICE_ATTR(object, S_IWUSR|S_IRUGO, show_object, store_object);

/* static DEVICE_ATTR(suspend, S_IRUGO, test_suspend, NULL); */
/* static DEVICE_ATTR(resume, S_IRUGO, test_resume, NULL);  */
#ifdef MXT_FACTORY_TEST
static DEVICE_ATTR(tsp_firm_update, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_mxt_update_show, NULL);		/* firmware update */
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_mxt_firm_status_show, NULL);	/* firmware update status return */
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR, threshold_show, threshold_store);	/* touch threshold return, store */
static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_mxt_firm_version_show, NULL);	/* firmware version resturn in phone driver version */
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_mxt_firm_version_read_show, NULL);		/* firmware version resturn in TSP panel version */
#endif
#if ENABLE_NOISE_TEST_MODE
static DEVICE_ATTR(set_refer0, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_refer0_mode_show, NULL);
static DEVICE_ATTR(set_delta0, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_delta0_mode_show, NULL);
static DEVICE_ATTR(set_refer1, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_refer1_mode_show, NULL);
static DEVICE_ATTR(set_delta1, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_delta1_mode_show, NULL);
static DEVICE_ATTR(set_refer2, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_refer2_mode_show, NULL);
static DEVICE_ATTR(set_delta2, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_delta2_mode_show, NULL);
static DEVICE_ATTR(set_refer3, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_refer3_mode_show, NULL);
static DEVICE_ATTR(set_delta3, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_delta3_mode_show, NULL);
static DEVICE_ATTR(set_refer4, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_refer4_mode_show, NULL);
static DEVICE_ATTR(set_delta4, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_delta4_mode_show, NULL);
static DEVICE_ATTR(set_threshould, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, set_threshold_mode_show, NULL);
#endif


static struct attribute *maxTouch_attributes[] = { 
		&dev_attr_deltas.attr,
		&dev_attr_references.attr,
		&dev_attr_device_info.attr,
		&dev_attr_object_info.attr,
		&dev_attr_report_id.attr,
		&dev_attr_stat.attr,
		&dev_attr_debug.attr,
		&dev_attr_firmware.attr,
		&dev_attr_object.attr,
		/*	&dev_attr_suspend.attr, */
		/*	&dev_attr_resume.attr, */
		NULL,
};

static struct attribute_group maxtouch_attr_group = { 
	.attrs = maxTouch_attributes,
};

#if defined(MXT_FACTORY_TEST) || (ENABLE_NOISE_TEST_MODE)
static struct attribute *maxTouch_facotry_attributes[] = { 
#ifdef MXT_FACTORY_TEST
	&dev_attr_tsp_firm_update.attr,
		&dev_attr_tsp_firm_update_status.attr,
		&dev_attr_tsp_threshold.attr,
		&dev_attr_tsp_firm_version_phone.attr,
		&dev_attr_tsp_firm_version_panel.attr,
#endif

#if ENABLE_NOISE_TEST_MODE
		&dev_attr_set_refer0.attr,
		&dev_attr_set_delta0.attr,
		&dev_attr_set_refer1.attr,
		&dev_attr_set_delta1.attr,
		&dev_attr_set_refer2.attr,
		&dev_attr_set_delta2.attr,
		&dev_attr_set_refer3.attr,
		&dev_attr_set_delta3.attr,
		&dev_attr_set_refer4.attr,
		&dev_attr_set_delta4.attr,
		&dev_attr_set_threshould.attr,
#endif

		NULL,
};

static struct attribute_group maxtouch_factory_attr_group = { 
	.attrs = maxTouch_facotry_attributes,
};
#endif

/* This function sends a calibrate command to the maXTouch chip.
* While calibration has not been confirmed as good, this function sets
* the ATCHCALST and ATCHCALSTHR to zero to allow a bad cal to always recover
* Returns WRITE_MEM_OK if writing the command to touch chip was successful.
*/
unsigned char not_yet_count = 0;
extern gen_acquisitionconfig_t8_config_t			acquisition_config;
extern int mxt_acquisition_config(struct mxt_data *mxt);


static int calibrate_chip(struct mxt_data *mxt)
{
	u8 data = 1u;
	u8 cal_thr = 0u;
	int ret ;
	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] %s\n", __func__);

	facesup_message_flag  = 0;
	not_yet_count = 0;
	mxt_time_point = 0;

	/* Read calibration threshold */
	if (mxt->set_mode_for_ta) {
		/* Read TA Configurations */
		mxt_read_byte(mxt->client, MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER3, &cal_thr);
	}
	else {
		mxt_read_byte(mxt->client,	MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER0, &cal_thr);
	}

	/* change calibration suspend settings to zero until calibration confirmed good */
	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALST, 0);
	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALSTHR, 0);
	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALTHR, 0);
	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALRATIO, 0);

	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_NUMTOUCH, T9_NUMTOUCH); 

	/* TSP, Touchscreen threshold */
	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR, cal_thr);
	
	klogi_if("[TSP] reset acq atchcalst=%d, atchcalsthr=%d\n", 0, 0 );
	/* restore settings to the local structure so that when we confirm the */
	/* cal is good we can correct them in the chip */
	/* this must be done before returning */
	/* send calibration command to the chip */
	/* change calibration suspend settings to zero until calibration confirmed good */
	ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_CALIBRATE, data);
	if ( ret < 0 ) {
		pr_err("[TSP][ERROR] line : %d\n", __LINE__);
		return -1;
	} else {
		/* set flag for calibration lockup recovery if cal command was successful */
		/* set flag to show we must still confirm if calibration was good or bad */
		cal_check_flag = 1u;
		recal_comp_flag = 1u;
	}

	msleep(60);
 
	return ret;
}

static void check_chip_palm(struct mxt_data *mxt)
{
	uint8_t data_buffer[100] = { 0 };
	uint8_t try_ctr = 0;
	uint8_t data_byte = 0xF3; /* dianostic command to get touch flags */
	uint16_t diag_address;
	uint8_t tch_ch = 0, atch_ch = 0;
	uint8_t check_mask;
	uint8_t i;
	uint8_t j;
	uint8_t x_line_limit;
	struct i2c_client *client;

	client = mxt->client;
	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] %s\n", __func__);

	/* we have had the first touchscreen or face suppression message 
	 * after a calibration - check the sensor state and try to confirm if
	 * cal was good or bad */

	/* get touch flags from the chip using the diagnostic object */
	/* write command to command processor to get touch flags - 0xF3 Command required to do this */
	mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, 0xf3);
	
	
	/* get the address of the diagnostic object so we can get the data we need */
	diag_address = MXT_BASE_ADDR(MXT_DEBUG_DIAGNOSTICS_T37);
	
	/* SAP_Sleep(5); */
	msleep(5);


	/* read touch flags from the diagnostic object - clear buffer so the while loop can run first time */
	memset( data_buffer , 0xFF, sizeof( data_buffer ) );

	/* wait for diagnostic object to update */
	while(!((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00))) {
		/* wait for data to be valid  */
		if(try_ctr > 50) {
			/* Failed! */
			pr_err("[TSP] Diagnostic Data did not update!!\n");
			break;
		}
		msleep(2); 
		try_ctr++; /* timeout counter */
		mxt_read_block(client, diag_address, 2, data_buffer);
		
		klogi_if("[TSP] Waiting for diagnostic data to update, try %d\n", try_ctr);
	}

	if(try_ctr > 50){
		pr_err("[TSP] %s, Diagnostic Data did not update over 50, force reset!! %d\n", __func__, try_ctr);
		/* forces a reset of the chipset */
		mxt_release_all_fingers(mxt);
		mxt_release_all_keys(mxt);
		reset_chip(mxt, RESET_TO_NORMAL);
		msleep(150); /*need 250ms*/
		return;
	}

	/* data is ready - read the detection flags */
	mxt_read_block(client, diag_address, 82, data_buffer);

	/* data array is 20 x 16 bits for each set of flags, 2 byte header, 40 bytes for touch flags 40 bytes for antitouch flags*/

	/* count up the channels/bits if we recived the data properly */
	if((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00)) {

		/* mode 0 : 16 x line, mode 1 : 17 etc etc upto mode 4.*/
		//x_line_limit = 16 + cte_config.mode;
		x_line_limit = 16 + T28_MODE;
		if(x_line_limit > 20) { 
			/* hard limit at 20 so we don't over-index the array */
			x_line_limit = 20;
		}

		/* double the limit as the array is in bytes not words */
		x_line_limit = x_line_limit << 1;

		/* count the channels and print the flags to the log */
		for(i = 0; i < x_line_limit; i+=2) {  /*check X lines - data is in words so increment 2 at a time */
		
			/* print the flags to the log - only really needed for debugging */
			//printk("[TSP] Detect Flags X%d, %x%x, %x%x \n", i>>1,data_buffer[3+i],data_buffer[2+i],data_buffer[43+i],data_buffer[42+i]);

			/* count how many bits set for this row */
			for(j = 0; j < 8; j++) { 
				/* create a bit mask to check against */
				check_mask = 1 << j;

				/* check detect flags */
				if(data_buffer[2+i] & check_mask) { 
					tch_ch++;
				}
				if(data_buffer[3+i] & check_mask) { 
					tch_ch++;
				}

				/* check anti-detect flags */
				if(data_buffer[42+i] & check_mask) { 
					atch_ch++;
				}
				if(data_buffer[43+i] & check_mask) { 
					atch_ch++;
				}
			}
		}


		/* print how many channels we counted */
		klogi_if("[TSP] Flags Counted channels: t:%d a:%d \n", tch_ch, atch_ch);

		/* send page up command so we can detect when data updates next time,
		 * page byte will sit at 1 until we next send F3 command */
		data_byte = 0x01;
		/* write_mem(command_processor_address + DIAGNOSTIC_OFFSET, 1, &data_byte); */
		mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, data_byte);
		
		if ((tch_ch > 0 ) && ( atch_ch  > 0)) {
			facesup_message_flag = 1;			
		} else if ((tch_ch > 0 ) && ( atch_ch  == 0)) {
			facesup_message_flag = 2;
		} else if ((tch_ch == 0 ) && ( atch_ch > 0)) {
			facesup_message_flag = 3;
		}else {
			facesup_message_flag = 4;
		}

		/* 12.06.12. Debugged
		if ((tch_ch < 70) || ((tch_ch >= 70) && ((tch_ch - atch_ch) > 25))) palm_check_timer_flag = true;
		*/
		if ((tch_ch > 15) || (atch_ch >= 5)) palm_check_timer_flag = true;
		
		klogi_if("[TSP] Touch suppression State: %d \n", facesup_message_flag);
	}
}

static void check_chip_channel(struct mxt_data *mxt)
{
	uint8_t data_buffer[100] = { 0 };
	uint8_t try_ctr = 0;
	uint8_t data_byte = 0xF3; /* dianostic command to get touch flags */
	uint16_t diag_address;
	uint8_t tch_ch = 0, atch_ch = 0;
	uint8_t check_mask;
	uint8_t i;
	uint8_t j;
	uint8_t x_line_limit;
	struct i2c_client *client;

	client = mxt->client;
	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] %s\n", __func__);

	/* we have had the first touchscreen or face suppression message 
	 * after a calibration - check the sensor state and try to confirm if
	 * cal was good or bad */

	/* get touch flags from the chip using the diagnostic object */
	/* write command to command processor to get touch flags - 0xF3 Command required to do this */
	mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, 0xf3);
	
	
	/* get the address of the diagnostic object so we can get the data we need */
	diag_address = MXT_BASE_ADDR(MXT_DEBUG_DIAGNOSTICS_T37);
	
	/* SAP_Sleep(5); */
	msleep(5);


	/* read touch flags from the diagnostic object - clear buffer so the while loop can run first time */
	memset( data_buffer , 0xFF, sizeof( data_buffer ) );

	/* wait for diagnostic object to update */
	while(!((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00))) {
		/* wait for data to be valid  */
		if(try_ctr > 50) {
			/* Failed! */
			pr_err("[TSP] Diagnostic Data did not update!!\n");
				break;
		}
		msleep(2); 
		try_ctr++; /* timeout counter */
		/* read_mem(diag_address, 2,data_buffer); */
		mxt_read_block(client, diag_address, 2, data_buffer);
		
		klogi_if("[TSP] Waiting for diagnostic data to update, try %d\n", try_ctr);
	}

	if(try_ctr > 50){
		pr_err("[TSP] %s, Diagnostic Data did not update over 50, force reset!! %d\n", __func__, try_ctr);
		/* forces a reset of the chipset */
		mxt_release_all_fingers(mxt);
		mxt_release_all_keys(mxt);
		reset_chip(mxt, RESET_TO_NORMAL);
		msleep(150); /*need 250ms*/
		return;
	}

	/* data is ready - read the detection flags */
	/* read_mem(diag_address, 82,data_buffer); */
	mxt_read_block(client, diag_address, 82, data_buffer);

	/* data array is 20 x 16 bits for each set of flags, 2 byte header, 40 bytes for touch flags 40 bytes for antitouch flags*/

	/* count up the channels/bits if we recived the data properly */
	if((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00)) {

		/* mode 0 : 16 x line, mode 1 : 17 etc etc upto mode 4.*/
		x_line_limit = 16 + T28_MODE;
		if(x_line_limit > 20) { 
			/* hard limit at 20 so we don't over-index the array */
			x_line_limit = 20;
		}

		/* double the limit as the array is in bytes not words */
		x_line_limit = x_line_limit << 1;

		/* count the channels and print the flags to the log */
		for(i = 0; i < x_line_limit; i+=2) {  /*check X lines - data is in words so increment 2 at a time */
			/* count how many bits set for this row */
			for(j = 0; j < 8; j++) { 
				/* create a bit mask to check against */
				check_mask = 1 << j;

				/* check detect flags */
				if(data_buffer[2+i] & check_mask) { 
					tch_ch++;
				}
				if(data_buffer[3+i] & check_mask) { 
					tch_ch++;
				}

				/* check anti-detect flags */
				if(data_buffer[42+i] & check_mask) { 
					atch_ch++;
				}
				if(data_buffer[43+i] & check_mask) { 
					atch_ch++;
				}
			}
		}


		/* print how many channels we counted */
		klogi_if("[TSP] Flags Counted channels: t:%d a:%d \n", tch_ch, atch_ch);

		chk_touch_cnt = tch_ch;
		chk_antitouch_cnt = atch_ch;
		/* send page up command so we can detect when data updates next time,
		 * page byte will sit at 1 until we next send F3 command */
		data_byte = 0x01;
		/* write_mem(command_processor_address + DIAGNOSTIC_OFFSET, 1, &data_byte); */
		mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, data_byte);
	}
	return;
}

static void check_chip_calibration(struct mxt_data *mxt)
{
	uint8_t data_buffer[100] = { 0 };
	uint8_t try_ctr = 0;
	uint8_t data_byte = 0xF3; /* dianostic command to get touch flags */
	uint16_t diag_address;
	uint8_t tch_ch = 0, atch_ch = 0;
	uint8_t check_mask;
	uint8_t i;
	uint8_t j;
	uint8_t x_line_limit;
	uint8_t CAL_THR;
	uint8_t num_of_antitouch;
	struct i2c_client *client;
	uint8_t finger_cnt = 0;

	client = mxt->client;
	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] %s\n", __func__);

	/* Read preset configurations */
	mxt_read_byte(mxt->client,  MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER1, &CAL_THR);		
	mxt_read_byte(mxt->client,  MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER2, &num_of_antitouch);
	
	/* we have had the first touchscreen or face suppression message 
	 * after a calibration - check the sensor state and try to confirm if
	 * cal was good or bad */

	/* get touch flags from the chip using the diagnostic object */
	/* write command to command processor to get touch flags - 0xF3 Command required to do this */
	mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, 0xf3);
	
	
	/* get the address of the diagnostic object so we can get the data we need */
	diag_address = MXT_BASE_ADDR(MXT_DEBUG_DIAGNOSTICS_T37);
	
	/* SAP_Sleep(5); */
	msleep(10);


	/* read touch flags from the diagnostic object - clear buffer so the while loop can run first time */
	memset( data_buffer , 0xFF, sizeof( data_buffer ) );


	/* read_mem(diag_address, 2,data_buffer); */
	mxt_read_block(client, diag_address, 3, data_buffer);

	
	/* wait for diagnostic object to update */
	while(!((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00))) {

		if(data_buffer[0] == 0xF3) {
			if( data_buffer[1] == 0x01) {
				/* Page down */
				data_byte = 0x02;
				mxt_write_byte(mxt->client,
				MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, data_byte);			
				/* SAP_Sleep(5); */
				msleep(10);
					
			}
		}
		else {
			msleep(10); 			
		}
		/* wait for data to be valid  */
		if(try_ctr > 3) {
			/* Failed! */
			pr_err("[TSP] Diagnostic Data did not update!!\n");
				break;
		}

		try_ctr++; /* timeout counter */

		/* read_mem(diag_address, 2,data_buffer); */
		mxt_read_block(client, diag_address, 3, data_buffer);
		
		klogi_if("[TSP] Waiting for diagnostic data to update, try %d\n", try_ctr);
	}

	if(try_ctr > 3){
		//pr_err("[TSP] %s, Diagnostic Data did not update over 3, force reset!! %d\n", __func__, try_ctr);
		pr_err("[TSP] %s, Diagnostic Data did not update over 3 !! %d\n", __func__, try_ctr);
#if 0
		/* forces a reset of the chipset */
		mxt_release_all_fingers(mxt);
		mxt_release_all_keys(mxt);
		reset_chip(mxt, RESET_TO_NORMAL);
		msleep(150); /*need 250ms*/
#endif
		/* send page up command so we can detect when data updates next time,
		 * page byte will sit at 1 until we next send F3 command */
		data_byte = 0x01;
		/* write_mem(command_processor_address + DIAGNOSTIC_OFFSET, 1, &data_byte); */
		mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, data_byte);

		return;
	}

	/* data is ready - read the detection flags */
	/* read_mem(diag_address, 82,data_buffer); */
	mxt_read_block(client, diag_address, 82, data_buffer);

	/* data array is 20 x 16 bits for each set of flags, 2 byte header, 40 bytes for touch flags 40 bytes for antitouch flags*/

	/* count up the channels/bits if we recived the data properly */
	if((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00)) {

		/* mode 0 : 16 x line, mode 1 : 17 etc etc upto mode 4.*/
		x_line_limit = 16 + T28_MODE;
		if(x_line_limit > 20) { 
			/* hard limit at 20 so we don't over-index the array */
			x_line_limit = 20;
		}

		/* double the limit as the array is in bytes not words */
		x_line_limit = x_line_limit << 1;

		/* count the channels and print the flags to the log */
		for(i = 0; i < x_line_limit; i+=2) {  /*check X lines - data is in words so increment 2 at a time */
			/* count how many bits set for this row */
			for(j = 0; j < 8; j++) { 
				/* create a bit mask to check against */
				check_mask = 1 << j;

				/* check detect flags */
				if(data_buffer[2+i] & check_mask) { 
					tch_ch++;
				}
				if(data_buffer[3+i] & check_mask) { 
					tch_ch++;
				}

				/* check anti-detect flags */
				if(data_buffer[42+i] & check_mask) { 
					atch_ch++;
				}
				if(data_buffer[43+i] & check_mask) { 
					atch_ch++;
				}
			}
		}


		/* print how many channels we counted */
		klogi_if("[TSP] Flags Counted channels: t:%d a:%d \n", tch_ch, atch_ch);

		/* send page up command so we can detect when data updates next time,
		 * page byte will sit at 1 until we next send F3 command */
		data_byte = 0x01;
		/* write_mem(command_processor_address + DIAGNOSTIC_OFFSET, 1, &data_byte); */
		mxt_write_byte(mxt->client,
			MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6) + MXT_ADR_T6_DIAGNOSTICS, data_byte);
		
		for (i = 0 ; i < MXT_MAX_NUM_TOUCHES ; ++i) { 
			if ( mtouch_info[i].pressure == -1 )
				continue;
			finger_cnt++;
		}

		if (cal_check_flag != 1) {
			klogi_if("[TSP] check_chip_calibration just return!! finger_cnt = %d\n", finger_cnt);
			return;
		}

		if(recal_comp_flag) {
#ifdef MXT_ADV_PARM_DEFENCE
			if ((atch_ch > 0) || ((tch_ch == 0) && (atch_ch == 0))) {
				klogi_if("[TSP] Bad Re-Calibreation status. just return!!! = atch_ch : %d, tch_ch : %d, atch_ch : %d\n", atch_ch, tch_ch, atch_ch);
				/* Restore finger data */
				mxt_release_all_fingers(mxt);
				mxt_release_all_keys(mxt);
				/* Force reset chip */
				mxt_force_reset(mxt);				
				/* 100ms timer disable */
				timer_flag = DISABLE;
				timer_ticks = 0;
				ts_100ms_timer_stop(mxt);
				return;
			}
			else {
				recal_comp_flag = 0;
			}
#else
			recal_comp_flag = 0;
#endif
		}
		
		/* process counters and decide if we must re-calibrate or if cal was good */      
		if ((tch_ch > 15) || (atch_ch >= num_of_antitouch) || ((tch_ch + atch_ch) > 30)) { 
			klogi_if("[TSP] maybe palm, re-calibrate!! \n");
			calibrate_chip(mxt);
			/* 100ms timer disable */
			timer_flag = DISABLE;
			timer_ticks = 0;
			ts_100ms_timer_stop(mxt);
		} else if((tch_ch > 0) && (tch_ch <= 15) && (atch_ch == 0)) {
			klogi_if("[TSP] calibration maybe good\n");
			if ((finger_cnt >= 2) && (tch_ch <= 5)) {
				klogi_if("[TSP]finger_cnt = %d, re-calibrate!! \n", finger_cnt);
				mxt_release_all_fingers(mxt);
				mxt_release_all_keys(mxt);
				calibrate_chip(mxt);
				/* 100ms timer disable */
				timer_flag = DISABLE;
				timer_ticks = 0;
				ts_100ms_timer_stop(mxt);
			} else {
				cal_maybe_good(mxt);
				not_yet_count = 0;
			}
		} else if (atch_ch > ((finger_cnt * num_of_antitouch) + 2)) { 
			klogi_if("[TSP] calibration was bad (finger : %d, max_antitouch_num : %d)\n", finger_cnt, finger_cnt*num_of_antitouch);
			calibrate_chip(mxt);
			/* 100ms timer disable */
			timer_flag = DISABLE;
			timer_ticks = 0;
			ts_100ms_timer_stop(mxt);
		} else if((tch_ch + CAL_THR /*10*/ ) <= atch_ch) { 
			klogi_if("[TSP] calibration was bad (CAL_THR : %d)\n",CAL_THR);
			/* cal was bad - must recalibrate and check afterwards */
			calibrate_chip(mxt);
			/* 100ms timer disable */
			timer_flag = DISABLE;
			timer_ticks = 0;
			ts_100ms_timer_stop(mxt);

		} else if((tch_ch == 0 ) && (atch_ch >= 2)) { 
			klogi_if("[TSP] calibration was bad, tch_ch = %d, atch_ch = %d)\n", tch_ch, atch_ch);
			/* cal was bad - must recalibrate and check afterwards */
			calibrate_chip(mxt);
			/* 100ms timer disable */
			timer_flag = DISABLE;
			timer_ticks = 0;
			ts_100ms_timer_stop(mxt);
		}else { 
			cal_check_flag = 1u;
			if (timer_flag == DISABLE) {
				/* 100ms timer enable */
				
				timer_flag = ENABLE;
				timer_ticks = 0;
				ts_100ms_timer_start(mxt);
			}
			not_yet_count++;
			klogi_if("[TSP] calibration was not decided yet, not_yet_count = %d\n", not_yet_count);
			if((tch_ch == 0) && (atch_ch == 0)) {
				not_yet_count = 0;
			} else if(not_yet_count >= 3) {
				klogi_if("[TSP] not_yet_count over 3, re-calibrate!! \n");
				not_yet_count =0;
				calibrate_chip(mxt);
				/* 100ms timer disable */
				timer_flag = DISABLE;
				timer_ticks = 0;
				ts_100ms_timer_stop(mxt);
			}
		}
	}
}

static void cal_maybe_good(struct mxt_data *mxt)
{
	int ret;
	u8 T9_TCHTHR_TA;
	
	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] %s\n", __func__);

	/* Check if the timer is enabled */
	if (mxt_time_point != 0) {

		/* Check if the timer timedout of 0.3seconds */
		if ((jiffies_to_msecs(jiffies) - mxt_time_point) >= 300) {
			pr_info("[TSP] time from touch press after calibration started = %d\n", (jiffies_to_msecs(jiffies) - mxt_time_point));

			/* Cal was good - don't need to check any more */
			mxt_time_point = 0;
			cal_check_flag = 0;

			/* Disable the timer */
			timer_flag = DISABLE;
			timer_ticks = 0;
			ts_100ms_timer_stop(mxt);
			
			/* Write back the normal acquisition config to chip. */
			ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALST, T8_ATCHCALST);
			ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHCALSTHR, T8_ATCHCALSTHR);
			ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALTHR, T8_ATCHFRCCALTHR);
			ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_GEN_ACQUIRECONFIG_T8) + MXT_ADR_T8_ATCHFRCCALRATIO, T8_ATCHFRCCALRATIO);
			ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_NUMTOUCH, T9_NUMTOUCH);

			/* TSP, Touchscreen threshold */	
			if (mxt->set_mode_for_ta) {
				/* Read TA Configurations */
				ret = mxt_read_byte(mxt->client, MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_USER3, &T9_TCHTHR_TA);
				ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR, T9_TCHTHR_TA);
			}
			else {
				ret = mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_TCHTHR, T9_TCHTHR);
			}

			if (ret < 0) {
				pr_err("[TSP] Calibration configuration write error!!\n");
			} 
			else {
				mxt->check_auto_cal = 5;
				pr_info("[TSP] Calibration success!! \n");

				if (metal_suppression_chk_flag == true) {
					/* after 20 seconds, metal coin checking disable */
					cancel_delayed_work(&mxt->timer_dwork);
					schedule_delayed_work(&mxt->timer_dwork, 2000);
				}
			}
		}
		else { 
			cal_check_flag = 1u;
		}
	}
	else { 
		cal_check_flag = 1u;
	}
}

#if TS_100S_TIMER_INTERVAL
/******************************************************************************/
/* 0512 Timer Rework by LBK                            */
/******************************************************************************/
static void ts_100ms_timeout_handler(unsigned long data)
{
	struct mxt_data *mxt = (struct mxt_data*)data;
	mxt->p_ts_timeout_tmr=NULL;	
	queue_work(ts_100s_tmr_workqueue, &mxt->tmr_work);
}

static void ts_100ms_timer_start(struct mxt_data *mxt)
{	
	if(mxt->p_ts_timeout_tmr != NULL)	del_timer(mxt->p_ts_timeout_tmr);
	mxt->p_ts_timeout_tmr = NULL;
		
	mxt->ts_timeout_tmr.expires = jiffies + HZ/10;	/* 100ms */
	mxt->p_ts_timeout_tmr = &mxt->ts_timeout_tmr;
	add_timer(&mxt->ts_timeout_tmr);
}

static void ts_100ms_timer_stop(struct mxt_data *mxt)
{
	if(mxt->p_ts_timeout_tmr) del_timer(mxt->p_ts_timeout_tmr);
		mxt->p_ts_timeout_tmr = NULL;
}

static void ts_100ms_timer_init(struct mxt_data *mxt)
{
	init_timer(&(mxt->ts_timeout_tmr));
	mxt->ts_timeout_tmr.data = (unsigned long)(mxt);
   	mxt->ts_timeout_tmr.function = ts_100ms_timeout_handler;		
	mxt->p_ts_timeout_tmr=NULL;
}

static void ts_100ms_tmr_work(struct work_struct *work)
{
	struct mxt_data *mxt = container_of(work, struct mxt_data, tmr_work);

	timer_ticks++;

	klogi_if("[TSP] 100ms T %d\n", timer_ticks);

	disable_irq(mxt->client->irq);
	/* Palm but Not touch message */
	if(facesup_message_flag ){
	 	klogi_if("[TSP] facesup_message_flag = %d\n", facesup_message_flag);
	 	check_chip_palm(mxt);
	}

	if ((timer_flag == ENABLE) && (timer_ticks<3)) {
		ts_100ms_timer_start(mxt);
		palm_check_timer_flag = false;
	} else {
		if (palm_check_timer_flag 
			&& ((facesup_message_flag == 1) || (facesup_message_flag == 2)) 
			&& (palm_release_flag == false)) {
			klogi_if("[TSP] calibrate_chip\n");
			calibrate_chip(mxt);	
			palm_check_timer_flag = false;
		}
		timer_flag = DISABLE;
		timer_ticks = 0;
	}
	enable_irq(mxt->client->irq);
}

#endif

/******************************************************************************/
/* Initialization of driver                                                   */
/******************************************************************************/

static int  mxt_identify(struct i2c_client *client,
			 struct mxt_data *mxt)
{ 
	u8 buf[7];
	int error;
	int identified;

	identified = 0;

retry_i2c:
	/* Read Device info to check if chip is valid */
	error = mxt_read_block(client, MXT_ADDR_INFO_BLOCK, 7, (u8 *)buf);

	if (error < 0) { 
		mxt->read_fail_counter++;
		if (mxt->read_fail_counter == 1) { 
			if (debug >= DEBUG_INFO)
				pr_info("[TSP] Warning: To wake up touch-ic in deep sleep, retry i2c communication!");
			msleep(30);  /* delay 25ms */
			goto retry_i2c;
		}
		dev_err(&client->dev, "[TSP] Failure accessing maXTouch device\n");
		return -EIO;
	}

	mxt->device_info.family_id  = buf[0];
	mxt->device_info.variant_id = buf[1];
	mxt->device_info.major	    = ((buf[2] >> 4) & 0x0F);
	mxt->device_info.minor      = (buf[2] & 0x0F);
	mxt->device_info.build	    = buf[3];
	mxt->device_info.x_size	    = buf[4];
	mxt->device_info.y_size	    = buf[5];
	mxt->device_info.num_objs   = buf[6];
	mxt->device_info.num_nodes  = mxt->device_info.x_size *
		mxt->device_info.y_size;

	/* Check Family Info */
	if (mxt->device_info.family_id == MAXTOUCH_FAMILYID) { 
		strcpy(mxt->device_info.family, maxtouch_family);
	} else { 
		dev_err(&client->dev,
			"[TSP] maXTouch Family ID [0x%x] not supported\n",
			mxt->device_info.family_id);
		identified = -ENXIO;
	}

	/* Check Variant Info */
	if ((mxt->device_info.variant_id == MXT224_CAL_VARIANTID) ||
		(mxt->device_info.variant_id == MXT224_UNCAL_VARIANTID)) { 
		strcpy(mxt->device_info.variant, mxt224_variant);
	} else { 
		dev_err(&client->dev,
			"[TSP] maXTouch Variant ID [0x%x] not supported\n",
			mxt->device_info.variant_id);
		identified = -ENXIO;
	}

	if (debug >= DEBUG_MESSAGES) 
		dev_info(
			&client->dev,
			"[TSP] Atmel %s.%s Firmware version [%d.%d] Build [%d]\n",
			mxt->device_info.family,
			mxt->device_info.variant,
			mxt->device_info.major,
			mxt->device_info.minor,
			mxt->device_info.build
			);
	if (debug >= DEBUG_MESSAGES) 
		dev_info(
			&client->dev,
			"[TSP] Atmel %s.%s Configuration [X: %d] x [Y: %d]\n",
			mxt->device_info.family,
			mxt->device_info.variant,
			mxt->device_info.x_size,
			mxt->device_info.y_size
			);
	if (debug >= DEBUG_MESSAGES) 
		dev_info(
			&client->dev,
			"[TSP] number of objects: %d\n",
			mxt->device_info.num_objs
			);

	return identified;
}

/*
* Reads the object table from maXTouch chip to get object data like
* address, size, report id.
*/
static int mxt_read_object_table(struct i2c_client *client,
				 struct mxt_data *mxt)
{ 
	u16	report_id_count;
	u8	buf[MXT_OBJECT_TABLE_ELEMENT_SIZE];
	u8	object_type;
	u16	object_address;
	u16	object_size;
	u8	object_instances;
	u8	object_report_ids;
	u16	object_info_address;
	u32	crc;
	u32     crc_calculated;
	int	i;
	int	error;

	u8	object_instance;
	u8	object_report_id;
	u8	report_id;
	int     first_report_id;

	struct mxt_object *object_table;

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver get configuration\n");

	object_table = kzalloc(sizeof(struct mxt_object) *
		mxt->device_info.num_objs,
		GFP_KERNEL);
	if (object_table == NULL) { 
		pr_warning("[TSP] maXTouch: Memory allocation failed!\n");
		return -ENOMEM;
	}

	mxt->object_table = object_table;

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver Memory allocated\n");

	object_info_address = MXT_ADDR_OBJECT_TABLE;

	report_id_count = 0;
	for (i = 0; i < mxt->device_info.num_objs; i++) { 
		if (debug >= DEBUG_TRACE)
			pr_info("[TSP] Reading maXTouch at [0x%04x]: ",
			object_info_address);

		error = mxt_read_block(client, object_info_address, MXT_OBJECT_TABLE_ELEMENT_SIZE, (u8 *)buf);

		if (error < 0) { 
			mxt->read_fail_counter++;
			dev_err(&client->dev,
				"[TSP] maXTouch Object %d could not be read\n", i);
			return -EIO;
		}
		object_type		=  buf[0];
		object_address		= (buf[2] << 8) + buf[1];
		object_size		=  buf[3] + 1;
		object_instances	=  buf[4] + 1;
		object_report_ids	=  buf[5];
		if (debug >= DEBUG_TRACE)
			pr_info("[TSP] Type=%03d, Address=0x%04x, "
			"Size=0x%02x, %d instances, %d report id's\n",
			object_type,
			object_address,
			object_size,
			object_instances,
			object_report_ids
			);

		if (object_type > MXT_MAX_OBJECT_TYPES) { 
			/* Unknown object type */
			dev_err(&client->dev,
				"[TSP] maXTouch object type [%d] not recognized\n",
				object_type);
			return -ENXIO;

		}

		/* Save frequently needed info. */
		if (object_type == MXT_GEN_MESSAGEPROCESSOR_T5) { 
			mxt->msg_proc_addr = object_address;
			mxt->message_size = object_size;
		}

		object_table[i].type            = object_type;
		object_table[i].chip_addr       = object_address;
		object_table[i].size            = object_size;
		object_table[i].instances       = object_instances;
		object_table[i].num_report_ids  = object_report_ids;
		report_id_count += object_instances * object_report_ids;

		object_info_address += MXT_OBJECT_TABLE_ELEMENT_SIZE;
	}

	mxt->rid_map =
		kzalloc(sizeof(struct report_id_map) * (report_id_count + 1),
		/* allocate for report_id 0, even if not used */
		GFP_KERNEL);
	if (mxt->rid_map == NULL) { 
		pr_warning("[TSP] maXTouch: Can't allocate memory!\n");
		return -ENOMEM;
	}

	mxt->last_message = kzalloc(mxt->message_size, GFP_KERNEL);
	if (mxt->last_message == NULL) { 
		pr_warning("[TSP] maXTouch: Can't allocate memory!\n");
		return -ENOMEM;
	}


	mxt->report_id_count = report_id_count;
	if (report_id_count > 254) { 	/* 0 & 255 are reserved */
		dev_err(&client->dev,
			"[TSP] Too many maXTouch report id's [%d]\n",
			report_id_count);
		return -ENXIO;
	}

	/* Create a mapping from report id to object type */
	report_id = 1; /* Start from 1, 0 is reserved. */

	/* Create table associating report id's with objects & instances */
	for (i = 0; i < mxt->device_info.num_objs; i++) { 
		for (object_instance = 0;
		object_instance < object_table[i].instances;
		object_instance++) { 
			first_report_id = report_id;
			for (object_report_id = 0;
			object_report_id < object_table[i].num_report_ids;
			object_report_id++) { 
				mxt->rid_map[report_id].object =
					object_table[i].type;
				mxt->rid_map[report_id].instance =
					object_instance;
				mxt->rid_map[report_id].first_rid =
					first_report_id;
				report_id++;
			}
		}
	}

	/* Read 3 byte CRC */
	error = mxt_read_block(client, object_info_address, 3, buf);
	if (error < 0) { 
		mxt->read_fail_counter++;
		dev_err(&client->dev, "[TSP] Error reading CRC\n");
	}

	crc = (buf[2] << 16) | (buf[1] << 8) | buf[0];

	calculate_infoblock_crc(&crc_calculated, mxt);

	if (debug >= DEBUG_TRACE) { 
		pr_info("[TSP] Reported info block CRC = 0x%6X\n\n", crc);
		pr_info("[TSP] Calculated info block CRC = 0x%6X\n\n",
			crc_calculated);
	}

	if (crc == crc_calculated) { 
		mxt->info_block_crc = crc;
	} else { 
		mxt->info_block_crc = 0;
		pr_warning("[TSP] maXTouch: info block CRC invalid!\n");
	}


	mxt->delta	= NULL;
	mxt->reference	= NULL;
	mxt->cte	= NULL;

	if (debug >= DEBUG_VERBOSE) { 

		dev_info(&client->dev, "[TSP] maXTouch: %d Objects\n",
			mxt->device_info.num_objs);

		for (i = 0; i < mxt->device_info.num_objs; i++) { 
			dev_info(&client->dev, "[TSP] Type:\t\t\t[%d]: %s\n",
				object_table[i].type,
				object_type_name[object_table[i].type]);
			dev_info(&client->dev, "\tAddress:\t0x%04X\n",
				object_table[i].chip_addr);
			dev_info(&client->dev, "\tSize:\t\t%d Bytes\n",
				object_table[i].size);
			dev_info(&client->dev, "\tInstances:\t%d\n",
				object_table[i].instances);
			dev_info(&client->dev, "\tReport Id's:\t%d\n",
				object_table[i].num_report_ids);
		}
	}
	return 0;
}

u8 mxt_valid_interrupt(void)
{ 
	/* TO_CHK: how to implement this function? */
	return 1;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#ifdef MXT_SLEEP_POWEROFF
static bool tsp_sleep_mode_flag = false;
#endif
static void mxt_early_suspend(struct early_suspend *h)
{ 
	struct	mxt_data *mxt = container_of(h, struct mxt_data, early_suspend);
	u8 cmd_sleep[2] = {0,0};
	u16 addr;
	u8 i;

	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] mxt_early_suspend has been called!");
#if defined(MXT_FACTORY_TEST) || defined(MXT_FIRMUP_ENABLE)
	/*start firmware updating : not yet finished*/
	while (mxt->firm_status_data == 1) { 
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] mxt firmware is Downloading : mxt suspend must be delayed!");
		msleep(1000);
	}
#endif

	cancel_delayed_work(&mxt->config_dwork);
	metal_suppression_chk_flag = false;
	cancel_delayed_work(&mxt->timer_dwork);
	cancel_delayed_work(&mxt->initial_dwork);
	disable_irq(mxt->client->irq);
	ts_100ms_timer_stop(mxt);
	mxt_release_all_fingers(mxt);
	mxt_release_all_keys(mxt);

	/* global variable initialize */
	timer_flag = DISABLE;
	timer_ticks = 0;
	mxt_time_point = 0;
	coin_check_flag = false;
	coin_check_count = 0;
	time_after_autocal_enable = 0;
	for(i=0;i < 10; i++) {
		tch_vct[i].cnt = 0;
	}
	mxt->mxt_status = false;

#ifdef MXT_SLEEP_POWEROFF
	if (mxt->set_mode_for_ta) {	/* if TA -> TSP go to sleep mode */
		/*
		* a setting of zeros to IDLEACQINT and ACTVACQINT
		* forces the chip set to enter Deep Sleep mode.
		*/
		addr = get_object_address(MXT_GEN_POWERCONFIG_T7, 0, mxt->object_table, mxt->device_info.num_objs);
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] addr: 0x%02x, buf[0]=0x%x, buf[1]=0x%x", addr, cmd_sleep[0], cmd_sleep[1]);
		mxt_write_block(mxt->client, addr, 2, (u8 *)cmd_sleep);
		tsp_sleep_mode_flag = true;

		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Entering sleep completed.");				
	} else {
	if (mxt->pdata->suspend_platform_hw != NULL)
		mxt->pdata->suspend_platform_hw(mxt->pdata);
		tsp_sleep_mode_flag = false;
		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Power off completed.");
	}
#else
		/*
		* a setting of zeros to IDLEACQINT and ACTVACQINT
		* forces the chip set to enter Deep Sleep mode.
		*/
	addr = get_object_address(MXT_GEN_POWERCONFIG_T7, 0, mxt->object_table, mxt->device_info.num_objs);
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] addr: 0x%02x, buf[0]=0x%x, buf[1]=0x%x", addr, cmd_sleep[0], cmd_sleep[1]);
	mxt_write_block(mxt->client, addr, 2, (u8 *)cmd_sleep);

	msleep(150);  /*typical value is 150ms*/
#endif
}

static void mxt_late_resume(struct early_suspend *h)
{ 
	struct	mxt_data *mxt = container_of(h, struct mxt_data, early_suspend);
	int cnt; 

	if (debug >= DEBUG_VERBOSE)
		pr_info("[TSP] mxt_late_resume has been called!");

#ifdef MXT_SLEEP_POWEROFF
	if (tsp_sleep_mode_flag == true) {
		for (cnt = 10; cnt > 0; cnt--) { 
			if (mxt_power_config(mxt) >= 0)
				break;
		}

		if (cnt == 0) {
			pr_err("[TSP] mxt_power_config failed, reset IC!!!");
			reset_chip(mxt, RESET_TO_NORMAL);
			msleep(100);
			if(mxt->set_mode_for_ta && !work_pending(&mxt->ta_work))
				schedule_work(&mxt->ta_work);
		} else {
			msleep(30);
		}

		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Exit sleep completed.");

		/* when sleep mode resume, Is TA detached? */
		if (mxt->set_mode_for_ta == 0 && !work_pending(&mxt->ta_work)) {
			schedule_work(&mxt->ta_work);
		} else {
			calibrate_chip(mxt);
		}
	}	else {
	if (mxt->pdata->resume_platform_hw != NULL)
		mxt->pdata->resume_platform_hw(mxt->pdata);

		msleep(100);  /*typical value is 200ms*/

		if (debug >= DEBUG_INFO)
			pr_info("[TSP] Power on completed.");

		if (mxt->set_mode_for_ta && !work_pending(&mxt->ta_work)) {
			schedule_work(&mxt->ta_work);
		} else {
			calibrate_chip(mxt);
		}
	}
#else
	for (cnt = 10; cnt > 0; cnt--) { 
		if (mxt_power_config(mxt) < 0)
			continue;
		if (reset_chip(mxt, RESET_TO_NORMAL) == 0)  /* soft reset */
			break;
	}
	if (cnt == 0) { 
		pr_err("[TSP] mxt_late_resume failed!!!");
		return;
	}

	msleep(80);  /*typical value is 150ms*/
	calibrate_chip(mxt);

	if (mxt->set_mode_for_ta && !work_pending(&mxt->ta_work))
		schedule_work(&mxt->ta_work);
#endif
	/* AMP Hyst on Calibrate time */
	mxt_write_byte(mxt->client, MXT_BASE_ADDR(MXT_TOUCH_MULTITOUCHSCREEN_T9) + MXT_ADR_T9_AMPHYST, T9_AMPHYST);

	metal_suppression_chk_flag = true;
	mxt->mxt_status = true;
	enable_irq(mxt->client->irq);
}
#endif

static int mxt_ftm_mode(struct i2c_client *client) {
	struct input_dev *input;
	char phys_name[32];
	int error;
	int i; 

	input = input_allocate_device();
	if (!input) { 
		dev_err(&client->dev, "[TSP] insufficient memory\n");
		error = -ENOMEM;
	}

	snprintf(
		phys_name,
		sizeof(phys_name),
		"%s/input0",
		dev_name(&client->dev)
		);

	input->name = "kttech_touchscreen";
	input->phys = phys_name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	__set_bit(BTN_TOUCH, input->keybit);
	__set_bit(EV_ABS, input->evbit);

	/* multi touch */
	input_set_abs_params(input, ABS_MT_POSITION_X, 0, 540, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, 960, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, MXT_MAX_NUM_TOUCHES-1, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 30, 0, 0);
	__set_bit(EV_SYN, input->evbit);
	__set_bit(EV_KEY, input->evbit);

	/* touch key */
	for (i = 0; i < NUMOF3KEYS; i++) { 
		__set_bit(tsp_3keycodes[i], input->keybit);
	}

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver setting client data\n");

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver input register device\n");

	error = input_register_device(input);
	if (error < 0) { 
		dev_err(&client->dev,
			"[TSP] Failed to register input device\n");
		return -EPERM;
	}
	return 0;
}

static int __devinit mxt_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{ 
	struct mxt_data          *mxt;
	struct mxt_platform_data *pdata;
	struct input_dev         *input;
	int error = 0;
	int rc = 0;
	int i;
#ifdef MXT_FIRMUP_ENABLE
	u8 unverified = 0;
#endif
	/* 
	u8  udata[8]; 
	*/

#ifdef MXT_FIRMUP_ENABLE
	/* mXT224 Latest Firmware version [2.0] Build [0xaa]*/
	u8 last_major = 0x02;
	u8 last_minor = 0x00;
	u8 last_build = 0xaa;
#endif

	/* FTM mode supprt handler */
	if(get_kttech_ftm_mode() == 2) {
		pr_info("[TSP] maXTouch into FTM mode.\n");
		rc = mxt_ftm_mode(client);
		if(rc < 0) 
			pr_err("[TSP] maXTouch into FTM mode failed!\n");
		return 0;
	}

	if (debug >= DEBUG_INFO)
		pr_info("[TSP] mXT224: mxt_probe\n");

	if (client == NULL)
		pr_err("[TSP] maXTouch: client == NULL\n");
	else if (client->adapter == NULL)
		pr_err("[TSP] maXTouch: client->adapter == NULL\n");
	else if (&client->dev == NULL)
		pr_err("[TSP] maXTouch: client->dev == NULL\n");
	else if (&client->adapter->dev == NULL)
		pr_err("[TSP] maXTouch: client->adapter->dev == NULL\n");
	else if (id == NULL)
		pr_err("[TSP] maXTouch: id == NULL\n");
	else
		goto param_check_ok;
	return	-EINVAL;

param_check_ok:
	if (debug >= DEBUG_INFO) { 
		pr_info("[TSP] maXTouch driver\n");
		pr_info("\t \"%s\"\n",		client->name);
		pr_info("\taddr:\t0x%04x\n",	client->addr);
		pr_info("\tirq:\t%d\n",	client->irq);
		pr_info("\tflags:\t0x%04x\n",	client->flags);
		pr_info("\tadapter:\"%s\"\n",	client->adapter->name);
	}
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] Parameters OK\n");;

	/* Allocate structure - we need it to identify device */
	mxt = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	input = input_allocate_device();
	if (!mxt || !input) { 
		dev_err(&client->dev, "[TSP] insufficient memory\n");
		error = -ENOMEM;
		goto err_after_kmalloc;
	}

	/* Initialize Platform data */
	pdata = client->dev.platform_data;
	if (pdata == NULL) { 
		dev_err(&client->dev, "[TSP] platform data is required!\n");
		error = -EINVAL;
		goto err_after_kmalloc;
	}
	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] Platform OK: pdata = 0x%08x\n", (unsigned int) pdata);
	mxt->pdata = pdata;

	mxt->read_fail_counter = 0;
	mxt->message_counter   = 0;
	mxt->set_mode_for_ta   = 0;

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver identifying chip\n");

	if (debug >= DEBUG_INFO)
		pr_info("\tboard-revision:\t\"%d\"\n",	mxt->pdata->board_rev);

	mxt->client = client;
	mxt->input  = input;

	/* Initialize Platform PIN Configuration */
	if (mxt->pdata->init_platform_hw != NULL)
		mxt->pdata->init_platform_hw(mxt->pdata);

	/* mxt224 regulator config for MSM8x60 */
	mxt->pdata->reg_lvs3 = regulator_get(NULL, mxt->pdata->reg_lvs3_name);

	if (IS_ERR(mxt->pdata->reg_lvs3)) { 
		error = PTR_ERR(mxt->pdata->reg_lvs3);
		pr_err("[TSP] [%s: %s]unable to get regulator %s: %d\n",
			__func__,
			mxt->pdata->platform_name,
			mxt->pdata->reg_lvs3_name,
			error);
	}

	mxt->pdata->reg_l4 = regulator_get(NULL, mxt->pdata->reg_l4_name);

	if (IS_ERR(mxt->pdata->reg_l4)) { 
		error = PTR_ERR(mxt->pdata->reg_l4);
		pr_err("[TSP] [%s: %s]unable to get regulator %s: %d\n",
			__func__,
			mxt->pdata->platform_name,
			mxt->pdata->reg_l4_name,
			error);
	}

	mxt->pdata->reg_mvs0 = regulator_get(NULL, mxt->pdata->reg_mvs0_name);

	if (IS_ERR(mxt->pdata->reg_mvs0)) { 
		error = PTR_ERR(mxt->pdata->reg_mvs0);
		pr_err("[TSP] [%s: %s]unable to get regulator %s: %d\n",
			__func__,
			mxt->pdata->platform_name,
			mxt->pdata->reg_mvs0_name,
			error);
	}

	/* TSP Power on */
	/* L4 Regulator Set Voltage */
	rc = regulator_set_voltage(mxt->pdata->reg_l4,
		mxt->pdata->reg_l4_level,
		mxt->pdata->reg_l4_level);

	if (rc) { 
		dev_err(&client->dev,"Regulator Set Voltage (L4) failed! (%d)\n", rc);
		regulator_put(mxt->pdata->reg_lvs3);
		goto err_after_get_regulator;
	}
	
	/* Enable Regulator */
	/* L4 */
	error = regulator_enable(mxt->pdata->reg_l4);
	msleep(10);
	/* LVS3 */
	error = regulator_enable(mxt->pdata->reg_lvs3);
	msleep(10);
	/* MVS0 */
	error = regulator_enable(mxt->pdata->reg_mvs0);
	msleep(10);

	/* Request GPIO */
	if (gpio_request(mxt->pdata->reset_gpio, "ts_reset") < 0) {
		dev_err(&client->dev,"GPIO Request failed! (GPIO : %d)\n", mxt->pdata->reset_gpio);
		goto err_after_get_regulator;
	}

	if (gpio_request(mxt->pdata->irq_gpio, "ts_int") < 0) {
		dev_err(&client->dev,"GPIO Request failed! (GPIO : %d)\n", mxt->pdata->irq_gpio);
		goto err_after_get_regulator;
	}
	
	/* Controller Reset */
	rc += gpio_direction_output(mxt->pdata->reset_gpio, 1);
	msleep(10);    
	rc += gpio_direction_output(mxt->pdata->reset_gpio, 0);
	msleep(10);
	rc += gpio_direction_output(mxt->pdata->reset_gpio, 1);
	msleep(10);

	if (rc < 0) { 
		dev_err(&client->dev,"GPIO Configuration failed! (%d)\n", rc);
		goto err_after_get_regulator;
	}

	if (error < 0){ 
		dev_err(&client->dev, "[TSP] ATMEL Chip could not set regulator. error = %d\n", error);
#ifdef MXT_FIRMUP_ENABLE
		unverified = 1;
#else
		goto err_after_get_regulator;
#endif
	}

	msleep(250);

	error = mxt_identify(client, mxt);
	if (error < 0) { 
		dev_err(&client->dev, "[TSP] ATMEL Chip could not be identified. error = %d\n", error);
#ifdef MXT_FIRMUP_ENABLE
		unverified = 1;
#else
		goto err_after_get_regulator;
#endif
	}

	error = mxt_read_object_table(client, mxt);
	if (error < 0){ 
		dev_err(&client->dev, "[TSP] ATMEL Chip could not read obkect table. error = %d\n", error);
#ifdef MXT_FIRMUP_ENABLE
		unverified = 1;
#else
		goto err_after_get_regulator;
#endif
	}

	i2c_set_clientdata(client, mxt);
	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver setting drv data\n");

#ifdef MXT_FIRMUP_ENABLE /*auto firmware upgrade check */
	if ((mxt->device_info.major < last_major) || (mxt->device_info.minor < last_minor) || (mxt->device_info.build < last_build) || unverified) { 
		pr_warning("[TSP] Touch firm up is needed to last version :[%d.%d] , build : [%d] ", last_major, last_minor, last_build);
		mxt->firm_status_data = 1;		/* start firmware updating */
		error = set_mxt_auto_update_exe(&client->dev);
		if (error < 0)
			goto err_after_get_regulator;
	}
#endif

	/* Chip is valid and active. */
	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver allocating input device\n");

#ifndef MXT_THREADED_IRQ
	INIT_DELAYED_WORK(&mxt->dwork, mxt_worker);
#endif
	INIT_WORK(&mxt->ta_work, mxt_ta_worker);

	INIT_DELAYED_WORK(&mxt->config_dwork, mxt_palm_recovery);

	INIT_DELAYED_WORK(&mxt->timer_dwork, mxt_metal_suppression_off); 

	INIT_DELAYED_WORK(&mxt->initial_dwork, mxt_boot_delayed_initial); 

#ifdef MXT_FACTORY_TEST
	INIT_DELAYED_WORK(&mxt->firmup_dwork, set_mxt_update_exe);
#endif

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver init spinlock\n");

	/* Register callbacks */
	/* To inform tsp , charger connection status*/
	mxt->callbacks.inform_charger = mxt_inform_charger_connection;
	if (mxt->pdata->register_cb)
		mxt->pdata->register_cb(&mxt->callbacks);

	init_waitqueue_head(&mxt->msg_queue);
#ifndef init_MUTEX
        sema_init(&mxt->msg_sem, 1);
#else
        init_MUTEX(&mxt->msg_sem);
#endif
	spin_lock_init(&mxt->lock);


	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver creating device name\n");

	snprintf(
		mxt->phys_name,
		sizeof(mxt->phys_name),
		"%s/input0",
		dev_name(&client->dev)
		);

	input->name = "kttech_touchscreen";
	input->phys = mxt->phys_name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	if (debug >= DEBUG_INFO) { 
		pr_info("[TSP] maXTouch name: \"%s\"\n", input->name);
		pr_info("[TSP] maXTouch phys: \"%s\"\n", input->phys);
		pr_info("[TSP] maXTouch driver setting abs parameters\n");
	}
	__set_bit(BTN_TOUCH, input->keybit);
	__set_bit(EV_ABS, input->evbit);

	/* multi touch */
	input_set_abs_params(input, ABS_MT_POSITION_X,  0, mxt->pdata->max_x, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y,  0, mxt->pdata->max_y, 0, 0);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 127, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, MXT_MAX_NUM_TOUCHES-1, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 30, 0, 0);

	__set_bit(EV_SYN, input->evbit);
	__set_bit(EV_KEY, input->evbit);

	/* touch key */
	for (i = 0; i < NUMOF3KEYS; i++) { 
		__set_bit(tsp_3keycodes[i], input->keybit);
	}

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver setting client data\n");

	input_set_drvdata(input, mxt);

	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver input register device\n");

	error = input_register_device(mxt->input);
	if (error < 0) { 
		dev_err(&client->dev,
			"[TSP] Failed to register input device\n");
		goto err_after_get_regulator;
	}
	if (debug >= DEBUG_TRACE)
		pr_info("[TSP] maXTouch driver allocate interrupt\n");

#ifndef MXT_TUNNING_ENABLE
#if 0
	/* pre-set configuration before soft reset */
	mxt_read_byte(mxt->client,  MXT_BASE_ADDR(MXT_USER_INFO_T38)+ MXT_ADR_T38_CFG_CTRL, &udata[0]);

	if (debug >= DEBUG_MESSAGES)
		pr_info("\t[TSP] udata[0] = :\t\"%d\"\n", udata[0]); /* temporary */
	if (udata[0] == MXT_CFG_OVERWRITE) {  /* for manual tuning */
		error = mxt_config_settings(mxt);
		if (error < 0)
			goto err_after_interrupt_register;
	}
#else
	error = mxt_config_settings(mxt);
	if (error < 0)
		goto err_after_interrupt_register;
#endif
	/* backup to nv memory */
	backup_to_nv(mxt);
	/* forces a reset of the chipset */
	//reset_chip(mxt, RESET_TO_NORMAL);
	//msleep(250); /*need 250ms*/
	//calibrate_chip(mxt);

#endif
	for (i = 0; i < MXT_MAX_NUM_TOUCHES ; i++)	/* _SUPPORT_MULTITOUCH_ */
		mtouch_info[i].pressure = -1;

#ifndef MXT_THREADED_IRQ
	/* Schedule a worker routine to read any messages that might have
	* been sent before interrupts were enabled. */
	cancel_delayed_work(&mxt->dwork);
	schedule_delayed_work(&mxt->dwork, 0);
#endif


	/* Allocate the interrupt */
	mxt->irq = client->irq;
	mxt->valid_irq_counter = 0;
	mxt->invalid_irq_counter = 0;
	mxt->irq_counter = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	mxt->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	mxt->early_suspend.suspend = mxt_early_suspend;
	mxt->early_suspend.resume = mxt_late_resume;
	register_early_suspend(&mxt->early_suspend);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	error = sysfs_create_group(&client->dev.kobj, &maxtouch_attr_group);
	if (error) { 
		unregister_early_suspend(&mxt->early_suspend);
		pr_err("[TSP] fail sysfs_create_group\n");
		goto err_after_interrupt_register;
	}

#ifdef MXT_FACTORY_TEST
	error = sysfs_create_group(&client->dev.kobj, &maxtouch_factory_attr_group);
	if (error) { 
		if (unverified) { 
			pr_err("[TSP] fail sysfs_create_group 1\n");
			goto err_after_attr_group;
		} else { 
			unregister_early_suspend(&mxt->early_suspend);
			pr_err("[TSP] fail sysfs_create_group 2\n");
			goto err_after_attr_group;
		}
	}
#endif

#if	TS_100S_TIMER_INTERVAL
		INIT_WORK(&mxt->tmr_work, ts_100ms_tmr_work);
		
		ts_100s_tmr_workqueue = create_singlethread_workqueue("ts_100_tmr_workqueue");
		if (!ts_100s_tmr_workqueue)
		{
			printk(KERN_ERR "unabled to create touch tmr work queue \r\n");
			error = -1;
			goto err_after_attr_group;
		}
		ts_100ms_timer_init(mxt);
#endif

	wake_lock_init(&mxt->wakelock, WAKE_LOCK_SUSPEND, "touch");
	mxt->mxt_status = true;

	/* after 5sec, start touch working */
	schedule_delayed_work(&mxt->initial_dwork, 500);
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] mxt probe ok\n");
	return 0;

err_after_attr_group:
	sysfs_remove_group(&client->dev.kobj, &maxtouch_attr_group);

err_after_interrupt_register:
	if (mxt->irq)
		free_irq(mxt->irq, mxt);
//err_after_input_register:
	input_free_device(input);

err_after_get_regulator:
	regulator_disable(mxt->pdata->reg_lvs3);
	regulator_disable(mxt->pdata->reg_l4);
	regulator_disable(mxt->pdata->reg_mvs0);

	regulator_put(mxt->pdata->reg_lvs3);
	regulator_put(mxt->pdata->reg_l4);
	regulator_put(mxt->pdata->reg_mvs0);
err_after_kmalloc:
	if (mxt != NULL) { 
		kfree(mxt->rid_map);
		kfree(mxt->delta);
		kfree(mxt->reference);
		kfree(mxt->cte);
		kfree(mxt->object_table);
		kfree(mxt->last_message);
		/* if (mxt->pdata->exit_platform_hw != NULL) */
		/*	mxt->pdata->exit_platform_hw(); */
	}
	kfree(mxt);

	return error;
}

static int __devexit mxt_remove(struct i2c_client *client)
{ 
	struct mxt_data *mxt;

	mxt = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	wake_lock_destroy(&mxt->wakelock);
	unregister_early_suspend(&mxt->early_suspend);
#endif	/* CONFIG_HAS_EARLYSUSPEND */
	/* Close down sysfs entries */
	sysfs_remove_group(&client->dev.kobj, &maxtouch_attr_group);

	/* Release IRQ so no queue will be scheduled */
	if (mxt->irq)
		free_irq(mxt->irq, mxt);
#ifndef MXT_THREADED_IRQ
	cancel_delayed_work_sync(&mxt->dwork);
#endif
	input_unregister_device(mxt->input);
	/* Should dealloc deltas, references, CTE structures, if allocated */

	if (mxt != NULL) { 
		kfree(mxt->rid_map);
		kfree(mxt->delta);
		kfree(mxt->reference);
		kfree(mxt->cte);
		kfree(mxt->object_table);
		kfree(mxt->last_message);
	}
	kfree(mxt);

	i2c_set_clientdata(client, NULL);
	if (debug >= DEBUG_TRACE)
		dev_info(&client->dev, "[TSP] Touchscreen unregistered\n");

	return 0;
}

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev); 
	struct mxt_data *mxt = i2c_get_clientdata(client);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(mxt->irq);

	return 0;
}

static int mxt_resume(struct device *dev)
{ 
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *mxt = i2c_get_clientdata(client);

	if (device_may_wakeup(&client->dev))
		disable_irq_wake(mxt->irq);

	return 0;
}
static SIMPLE_DEV_PM_OPS(mxt_pm, mxt_suspend, mxt_resume);
#else
#define mxt_suspend NULL
#define mxt_resume NULL
#endif

static const struct i2c_device_id mxt_idtable[] = { 
	{ "kttech_o3_tsp", 0,},
	{  }
};

MODULE_DEVICE_TABLE(i2c, mxt_idtable);

static struct i2c_driver mxt_driver = { 
	.driver = { 
		.name	= "kttech_o3_tsp",
		.owner  = THIS_MODULE,
#ifndef CONFIG_HAS_EARLYSUSPEND 
                .pm     = &mxt_pm,
#endif
	},

	.id_table	= mxt_idtable,
	.probe		= mxt_probe,
	.remove		= __devexit_p(mxt_remove),
};

static int __init mxt_init(void)
{ 
	int err;
	err = i2c_add_driver(&mxt_driver);

	return err;
}

static void __exit mxt_cleanup(void)
{ 
	i2c_del_driver(&mxt_driver);
}


module_init(mxt_init);
module_exit(mxt_cleanup);

MODULE_AUTHOR("KT Tech");
MODULE_DESCRIPTION("Driver for Atmel mXT224 Touchscreen Controller");

MODULE_LICENSE("GPL");
