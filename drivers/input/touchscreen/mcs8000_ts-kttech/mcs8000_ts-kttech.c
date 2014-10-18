/*
 * driver/input/touch/screen/mcs8000_ts-kttech.c
 *
 * Author: Jhoonkim <jhoonkim@kttech.co.kr>
 *
 * Copyright (C) 2012-2011 KT Tech, Inc
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

#include "mcs8000_ts-kttech.h"
#ifdef SET_DOWNLOAD_BY_GPIO
#include "mcs8000_download-kttech.h"
#endif

/*for debugging, enable DEBUG_INFO */
static int debug = DEBUG_INFO;

/* Workqueue Struct */
struct workqueue_struct *initial_dwork_wq = NULL;
struct workqueue_struct *debug_dwork_wq = NULL;
struct workqueue_struct *esd_detect_dwork_wq = NULL;
struct workqueue_struct *enable_esd_detect_dwork_wq = NULL;
#ifndef MELFAS_THREADED_IRQ
struct workqueue_struct *melfas_ts_report_position_dwork_wq = NULL;
#endif
#ifdef ENABLE_INFORM_CHERGER
struct workqueue_struct *ta_work_wq = NULL;
#endif

/* For TSP Update Logo Display */
extern void load_arbg8888_qhd_image_tsp(void);


#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif

int tsp_3keycodes[NUMOF3KEYS] = { 
	KEY_MENU,
	KEY_HOME,
	KEY_BACK
};

#ifdef CONFIG_KTTECH_TSP_PLAT_O3
char *tsp_3keyname[NUMOF3KEYS + 1] ={
	"Menu",
	"Home",
	"Back",
	"Null",
};

static u16 tsp_keystatus;
static u16 tsp_key_status_pressed;
#endif

struct multi_touch_info
{
	int id;
	int action;
	int fingerX;
	int fingerY;
	int width;
	int strength;
} touch_msg_info[MELFAS_MAX_TOUCH];

#if 0	// For I2C recovery code.
static void MCS8000_I2C_PORT_CONFIG(void)
{
	gpio_tlmm_config(GPIO_CFG(MCS8000_I2C_SCL, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(MCS8000_I2C_SDA, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	msleep(10);
}

static void MCS8000_I2C_PORT_DECONFIG(void)
{
	gpio_tlmm_config(GPIO_CFG(MCS8000_I2C_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(MCS8000_I2C_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(MCS8000_I2C_SCL, 0);
	gpio_set_value(MCS8000_I2C_SDA, 0);    
	msleep(10);
}
#endif

int melfas_read_bytes(struct i2c_client *client, uint8_t addr, uint8_t *buf, uint16_t length)
{ 
	struct melfas_ts_data *ts;
	int ret, i;

	ts = i2c_get_clientdata(client);

	for(i=0; i<I2C_RETRY_CNT; i++){
		buf[0] = addr;
		/* Send to read address */
		ret = i2c_master_send(client, buf, 1);
		if(ret < 0) {
			dev_err(&ts->client->dev, "[TSP] I2C master-send failed.(%d)\n", ret);
		}	
		else {
			/* Receive to read data */			
			ret = i2c_master_recv(client, buf, length);
			if(ret != length) {
				dev_err(&ts->client->dev, "[TSP] I2C master-receive failed.(%d)\n", ret);
			}
			else
				break; 
		}
	}
	return ret;
}

int melfas_write_bytes(struct i2c_client *client, uint8_t addr, uint8_t *val, uint16_t length)
{ 
	struct melfas_ts_data *ts;
	int ret, i;

	struct {
		uint8_t	i2c_addr;
		uint8_t buf[256];
	} i2c_block_transfer;

	ts = i2c_get_clientdata(client);

	i2c_block_transfer.i2c_addr = addr;
	
	for(i=0; i < length; i++)
		i2c_block_transfer.buf[i] = *val++;

	for(i=0; i<I2C_RETRY_CNT; i++){
		/* Send to write address */
		ret = i2c_master_send(client, (u8 *)&i2c_block_transfer, length+1);
		if(ret < 0) 
			dev_err(&ts->client->dev, "[TSP] I2C master-send failed.(%d)\n", ret);
	}
	return ret;
}

int melfas_write_byte(struct i2c_client *client, uint8_t addr, uint8_t val)
{ 
	struct melfas_ts_data *ts;
	uint8_t buf[2];
	int ret, i;

	ts = i2c_get_clientdata(client);

	buf[0] = addr;
	buf[1] = val; 

	for(i=0; i<I2C_RETRY_CNT; i++){
		/* Send to write address */
		ret = i2c_master_send(client, buf, 2);
		if(ret < 0) 
			dev_err(&ts->client->dev, "[TSP] I2C master-send failed.(%d)\n", ret);
	}
	return ret;
}

#ifdef ENABLE_INFORM_CHERGER
static void melfas_inform_charger_connection(struct melfas_callbacks *cb, int mode)
{ 
	struct melfas_ts_data *ts = container_of(cb, struct melfas_ts_data, callbacks);

	ts->set_mode_for_ta = !!mode;

	if (!work_pending(&ts->ta_work))
		queue_work(ta_work_wq, &ts->ta_work);

	return;
}
#endif

static void melfas_ta_worker(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, ta_work);

	dev_info(&ts->client->dev, "[TSP] TA Worker started.\n");

	disable_irq(ts->client->irq);

	if (ts->set_mode_for_ta) { 
		/* TODO : Connected TA */
	}
	else {
		/* TODO : Connected VBATT */		
	}
	enable_irq(ts->client->irq);

	return;
}

static void debug_dworker(struct work_struct *work)
{ 
	struct melfas_ts_data *ts;
	ts = container_of(work, struct melfas_ts_data, debug_dwork.work);

	/* Restore Debugging information */
	debug = DEBUG_INFO;

	/* Print Debugging information */
	dev_info(&ts->client->dev, "[TSP] IRQ Count %d, Failed IRQ Count: %d\n", ts->irq_counter, ts->failed_counter);

	return;
}

/* Disable ESD Detection logic */
static void enable_esd_detection_dworker(struct work_struct *work)
{ 
	struct melfas_ts_data *ts;	
	uint8_t buf_ts[TS_READ_REGS_LEN];
	
	ts = container_of(work, struct melfas_ts_data, enable_esd_detect_dwork.work);

	buf_ts[0] = TS_REG_CMD_SET_ESD_DETECTION;
	buf_ts[1] = 0x0;
	
	if(melfas_write_bytes(ts->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(&ts->client->dev, "[TSP] enable ESD Detection set failed.\n");
	}

	if(melfas_read_bytes(ts->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(&ts->client->dev, "[TSP] enable ESD Detection set result read failed.\n");
	}

	if(buf_ts[0] != 0x1) {
		dev_err(&ts->client->dev, "[TSP] enable ESD Detection set failed.(%d)\n", buf_ts[0]);
		return;
	}
	else
		dev_info(&ts->client->dev, "[TSP] enable ESD Detection set succeed.(%d)\n", buf_ts[0]);

	ts->esd_enable_counter++;
	ts->disabled_esd_detection = 0;

	if(ts->esd_enable_counter > 10)
		ts->esd_enable_counter = 0;
	
	return;
}

/* Disable ESD Detection logic */
static void disable_esd_detection(struct melfas_ts_data *ts)
{ 
	uint8_t buf_ts[TS_READ_REGS_LEN];

	buf_ts[0] = TS_REG_CMD_SET_ESD_DETECTION;
	buf_ts[1] = 0x1;

	if(melfas_write_bytes(ts->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(&ts->client->dev, "[TSP] Disable ESD Detection set failed.\n");
	}

 	if(melfas_read_bytes(ts->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(&ts->client->dev, "[TSP] Disable ESD Detection set result read failed.\n");
	}

 	if(buf_ts[0] != 0x1) {
		dev_err(&ts->client->dev, "[TSP] Disable ESD Detection set failed.(%d)\n", buf_ts[0]);
		return;
 	}
	else
		dev_info(&ts->client->dev, "[TSP] Disable ESD Detection set succeed.(%d)\n", buf_ts[0]);

	ts->disabled_esd_detection = 1;
	
	return;
}


static int melfas_init_panel(struct melfas_ts_data *ts)
{
	uint8_t buf = 0x00;
	int i;
	int error = -1;

	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)
	{
		touch_msg_info[i].id = i;
		touch_msg_info[i].strength = 0;
		touch_msg_info[i].fingerX = 0;
		touch_msg_info[i].fingerY = 0;
		touch_msg_info[i].width = 0;
		
		REPORT_MT(touch_msg_info[i].id,
			touch_msg_info[i].fingerX,
			touch_msg_info[i].fingerY,
			touch_msg_info[i].strength,
			touch_msg_info[i].width);
	}

	for (i = 0; i < I2C_RETRY_CNT; i++) {
		/* Check I2C Communications */
		if (error < 0)
			error = i2c_master_send(ts->client, &buf, 1);
	}

	/* Release All keys */
	input_report_key(ts->input, KEY_HOME, KEY_RELEASE);
	input_report_key(ts->input, KEY_MENU, KEY_RELEASE);
	input_report_key(ts->input, KEY_BACK, KEY_RELEASE);
	input_report_key(ts->input, BTN_TOUCH, 0);

	/* Sync keys */
	input_sync(ts->input);
	
	return error;
}

/* ESD Detection logic */
static void esd_detect_dworker(struct work_struct *work)
{ 
	struct melfas_ts_data *ts;
	ts = container_of(work, struct melfas_ts_data, esd_detect_dwork.work);

	ts->esd_counter++;

	/* Recovery TSP */
	if (ts->pdata->suspend_platform_hw != NULL)
		ts->pdata->suspend_platform_hw(ts->pdata);
	
	if (ts->pdata->resume_platform_hw != NULL)
		ts->pdata->resume_platform_hw(ts->pdata);
	
	melfas_init_panel(ts);

	/* Print Debugging information */
	dev_info(&ts->client->dev, "[TSP] ESD Detection logic. TSP Reset completed.");

	/* Limit exceed. Please. Check the TSP Panel */
	if(ts->esd_counter > ESD_RETRY_COUNTER_LIMIT) {
		dev_warn(&ts->client->dev, "[TSP] ESD Detection limit exceed. ESD Detection disabled.");
			
		/* Disable ESD Detection */
		disable_esd_detection(ts);
			
		/* After 120 Sec Enable ESD Detection */
		if(ts->irq_counter > 100) {
			/* Boot Succeed, Working time */
			if(!delayed_work_pending(&ts->enable_esd_detect_dwork))
				queue_delayed_work(enable_esd_detect_dwork_wq, &ts->enable_esd_detect_dwork, (ENABLE_ESD_DETECT_LIMIT_TIME * ts->esd_enable_counter));
				dev_warn(&ts->client->dev, "[TSP] ESD Detection %d ms after enable",
					(ENABLE_ESD_DETECT_LIMIT_TIME * ts->esd_enable_counter));			
		}
		else {
			dev_warn(&ts->client->dev, "[TSP] ESD Detection disabled. Check to TSP panel!");
		}
		ts->esd_counter = 0;
	}

	return;
}

#ifdef MELFAS_USE_INPUT_MODE
void input_mode_init(struct melfas_ts_data *ts)
{ 
	uint32_t mode = 0;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	buf_ts[0] = TS_REG_CMD_SET_EDIT_MODE;
	buf_ts[1] = (mode & 0xff);

	/* Save to val */
	ts->input_mode = mode;	

	if(melfas_write_bytes(ts->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(&ts->client->dev, "[TSP] Input mode set failed.\n");
	}

 	if(melfas_read_bytes(ts->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(&ts->client->dev, "[TSP] Input mode set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(&ts->client->dev, "[TSP] Set Input mode Register set failed.(%d:%d)\n", buf_ts[0],buf_ts[1]);
	else
		dev_info(&ts->client->dev, "[TSP] Set Input mode Register set succeed.(%d:%d)\n", buf_ts[0],buf_ts[1]);

	return;
}
#endif

#ifdef INIT_SET_CORRECTION_POS
void set_correction_x_pos_init(struct melfas_ts_data *ts) {
	uint32_t pos = 0;
	uint32_t variation = 0;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(ts->correcton_initialized_x != 1) {
		ts->correction_x_variant = INIT_CORR_X_VARIANT;
		ts->correction_x = INIT_CORR_XPOS;
		ts->correcton_initialized_x = 1;
	} 

	variation = ts->correction_x_variant;
	pos = ts->correction_x;
	
	buf_ts[0] = TS_REG_CMD_SET_CORRPOS_X;
	buf_ts[1] = (variation & 0xff);
	buf_ts[2] = (pos & 0xff);

	if(melfas_write_bytes(ts->client, TS_REG_VNDR_CMDID, buf_ts, 3) < 0) {
		dev_err(&ts->client->dev, "[TSP] Set TS X pos correction set failed.\n");
	}

 	if(melfas_read_bytes(ts->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(&ts->client->dev, "[TSP] Set TS X pos correction set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(&ts->client->dev, "[TSP] Set TS X pos correction failed.(%d)\n", buf_ts[0]);
	else
		dev_info(&ts->client->dev, "[TSP] Set TS X pos correction succeed.(%d : %d, %d)\n", buf_ts[0], variation, pos);

	return;
}

void set_correction_y_pos_init(struct melfas_ts_data *ts) {
	uint32_t pos = 0;
	uint32_t variation = 0;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(ts->correcton_initialized_y != 1) {
		ts->correction_y_variant = INIT_CORR_Y_VARIANT;
		ts->correction_y = INIT_CORR_YPOS;
		ts->correcton_initialized_y = 1;
	} 

	variation = ts->correction_y_variant;
	pos = ts->correction_y;
	
	buf_ts[0] = TS_REG_CMD_SET_CORRPOS_Y;
	buf_ts[1] = (variation & 0xff);
	buf_ts[2] = (pos & 0xff);

	if(melfas_write_bytes(ts->client, TS_REG_VNDR_CMDID, buf_ts, 3) < 0) {
		dev_err(&ts->client->dev, "[TSP] Set TS Y pos correction set failed.\n");
	}

 	if(melfas_read_bytes(ts->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(&ts->client->dev, "[TSP] Set TS Y pos correction set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(&ts->client->dev, "[TSP] Set TS Y pos correction failed.(%d)\n", buf_ts[0]);
	else
		dev_info(&ts->client->dev, "[TSP] Set TS Y pos correction succeed.(%d : %d, %d)\n", buf_ts[0], variation, pos);

	return;
}
#endif

#ifdef INIT_SET_FRAMERATE
void set_framerate_HZ_init(struct melfas_ts_data *ts)
{ 
	uint32_t framerate = 0;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(ts->framerate_initialized != 1) {
		ts->framerate = INIT_FRAMERATE;
		ts->framerate_initialized = 1;
	} 

	framerate = ts->framerate;
	
	buf_ts[0] = TS_REG_CMD_SET_FRAMERATE;
	buf_ts[1] = (framerate & 0xff);

	if(melfas_write_bytes(ts->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(&ts->client->dev, "[TSP] TS Framerate set failed.\n");
	}

 	if(melfas_read_bytes(ts->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(&ts->client->dev, "[TSP] TS Framerate set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(&ts->client->dev, "[TSP] TS Framerate register set failed.(%d)\n", buf_ts[0]);
	else
		dev_info(&ts->client->dev, "[TSP] Set TS X pos correction succeed.(%d : %d)\n", buf_ts[0], framerate);

	return;
}
#endif

#ifdef CONFIG_KTTECH_TSP_PLAT_O3
void Process_Touch_KEY(struct melfas_ts_data *ts, struct multi_touch_info *mtouch_info, int i){
	u8 status = 0;

	/* Key Status Calculation */
	if (debug >= DEBUG_MESSAGES) 
		pr_info("[TSP_KEY] X : %d, Y : %d, Num : %d\n",  mtouch_info[i].fingerX, mtouch_info[i].fingerY, i);

	/* check MENU key */
	if(((TOUCH_3KEY_MENU_CENTER-TOUCH_3KEY_MENU_WITDTH/2) <= mtouch_info[i].fingerX) && 
		(mtouch_info[i].fingerX <= (TOUCH_3KEY_MENU_CENTER+TOUCH_3KEY_MENU_WITDTH/2))) {
		tsp_keystatus = TOUCH_3KEY_MENU;
	}  
	/* check HOME key */
	else if(((TOUCH_3KEY_HOME_CENTER-TOUCH_3KEY_HOME_WITDTH/2) <= mtouch_info[i].fingerX) && 
		(mtouch_info[i].fingerX <= (TOUCH_3KEY_HOME_CENTER+TOUCH_3KEY_HOME_WITDTH/2))) {
		tsp_keystatus = TOUCH_3KEY_HOME;
	}
	/* check BACK key */    
	else if(((TOUCH_3KEY_BACK_CENTER-TOUCH_3KEY_BACK_WITDTH/2) <= mtouch_info[i].fingerX) &&
		(mtouch_info[i].fingerX <= (TOUCH_3KEY_BACK_CENTER+TOUCH_3KEY_BACK_WITDTH/2))) {
		tsp_keystatus = TOUCH_3KEY_BACK;
	}
	else {	
		if (mtouch_info[i].action == 0) {	/* if released */
			REPORT_MT(i, mtouch_info[i].fingerX, mtouch_info[i].fingerY, mtouch_info[i].action, mtouch_info[i].width);
			mtouch_info[i].action  = -1;
		}
		tsp_keystatus = TOUCH_KEY_NULL;
	}

	if(mtouch_info[i].action > 0) {	/* Key pressed */		
		status = KEY_PRESS;
	}	
	else {
		status = KEY_RELEASE;
	}
	
	/* defence code, if there is any Pressed key, force release!! */
	if(tsp_keystatus == TOUCH_KEY_NULL) {
		switch (tsp_key_status_pressed) {
		case TOUCH_3KEY_MENU:
			input_report_key(ts->input, KEY_MENU, KEY_RELEASE);
			break;
		case TOUCH_3KEY_HOME:
			input_report_key(ts->input, KEY_HOME, KEY_RELEASE);
			break;
		case TOUCH_3KEY_BACK:
			input_report_key(ts->input, KEY_BACK, KEY_RELEASE);
			break;
		default:
			break;		
		}
		tsp_key_status_pressed = 0;
	}

	switch (tsp_keystatus) {
	case TOUCH_3KEY_MENU:
		input_report_key(ts->input, KEY_MENU, status);
		break;
	case TOUCH_3KEY_HOME:
		input_report_key(ts->input, KEY_HOME, status);
		break;
	case TOUCH_3KEY_BACK:
		input_report_key(ts->input, KEY_BACK, status);
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

	return;
}
#endif

#ifndef MELFAS_THREADED_IRQ
static void melfas_ts_report_position_dworker(struct work_struct *work)
#else
static void melfas_ts_report_position(struct melfas_ts_data *ts)
#endif
{
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
	uint8_t read_num = 0;
	uint8_t touchAction = 0, touchType = 0, fingerID = 0;
	uint8_t chkpress = 0;

#ifndef MELFAS_THREADED_IRQ
	struct melfas_ts_data *ts;
	ts = container_of(work, struct melfas_ts_data, melfas_ts_report_position_dwork.work);
#endif

	/* 1. Read event packet size */
	ret = melfas_read_bytes(ts->client, TS_READ_EVENT_PACKET_SIZE, buf, 1);
	
	if(ret < 0) {
		dev_err(&ts->client->dev, "[TSP] TS_READ_EVENT_PACKET_SIZE failed.(%d)\n", ret);
	}
	else {
		/* 2. Save to read TS position num */
		read_num = buf[0];

		if (read_num > TS_READ_REGS_LEN) {
			read_num = TS_READ_REGS_LEN;
		}

		/* 3. Read TS position data */
		ret = melfas_read_bytes(ts->client, TS_READ_START_ADDR, buf, read_num);

		if(ret < 0) {
			dev_err(&ts->client->dev, "[TSP] TS_READ_START_ADDR failed.(%d)\n", ret);
		}
	}
	
	/* I2C Errror Handler */
	if(ret < 0) {
		/* Incrase of failed counter. */
		ts->failed_counter++;
		
		dev_warn(&ts->client->dev, "[TSP] I2C Communication Failed! Try to reset TSP!(%d)\n", ts->failed_counter);

		/* Recovery TSP */
		if (ts->pdata->suspend_platform_hw != NULL)
			ts->pdata->suspend_platform_hw(ts->pdata);

		if (ts->pdata->resume_platform_hw != NULL)
			ts->pdata->resume_platform_hw(ts->pdata);
	
		melfas_init_panel(ts);
		
		return;
	}
	else {
		for(i=0; i<read_num; i=i+6)	{
			/* 4-1. Process ABS touch data */
			touchAction = ((buf[i] & 0x80) == 0x80);
			touchType = (buf[i] & 0x60)>>5;
			fingerID = (buf[i] & 0x0F);

			if (debug >= DEBUG_TRACE) {
				dev_info(&ts->client->dev, "[TSP] A(%d), T(%d), F(%d), B1(%x), B2(%x), B3(%x), B4(%x), B5(%x)\n",
					touchAction, touchType, fingerID, buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5]);
			}

			/* Handling ESD Workaround*/
			if(touchType == TOUCH_TYPE_NONE && touchAction == 0) {
				dev_warn(&ts->client->dev, "[TSP] Running ESD Detection logic. Try to reset TSP!\n");
				
				/* Handling ESD after 0.5 sec */
				if(!delayed_work_pending(&ts->esd_detect_dwork))
					queue_delayed_work(esd_detect_dwork_wq, &ts->esd_detect_dwork, 0);

				return;
			}

			if(touchType == TOUCH_TYPE_SCREEN) {
				fingerID--;
				if(fingerID < MELFAS_MAX_TOUCH) {
					touch_msg_info[fingerID].id = fingerID;
					touch_msg_info[fingerID].action = touchAction;
					touch_msg_info[fingerID].fingerX = (buf[i + 1] & 0x0F) << 8 | buf[i + 2];
					touch_msg_info[fingerID].fingerY = (buf[i + 1] & 0xF0) << 4 | buf[i + 3];
					touch_msg_info[fingerID].width = buf[i + 4];
					touch_msg_info[fingerID].strength = buf[i + 5];

					/* Handling ESD Workaround at running TSP */
					if((touch_msg_info[fingerID].fingerX == 0) && (touch_msg_info[fingerID].fingerY == 0)) {
						dev_warn(&ts->client->dev, "[TSP] Running ESD Detection logic. Try to reset TSP!\n");

						/* Handling ESD after 0.5 sec */
						if(!delayed_work_pending(&ts->esd_detect_dwork)) {
							queue_delayed_work(esd_detect_dwork_wq, &ts->esd_detect_dwork, 0);						
							return;
						}
					}

					if (debug >= DEBUG_TRACE) {
						dev_info(&ts->client->dev, "[TSP] I(%d), X(%d), Y(%d), Z(%d), W(%d)\n",
							touch_msg_info[fingerID].id, 
							touch_msg_info[fingerID].fingerX, 
							touch_msg_info[fingerID].fingerY, 
							touch_msg_info[fingerID].strength, 
							touch_msg_info[fingerID].width);
					}
				}
			}
			/* 4-2. Process key touch data */
			if(touchType == TOUCH_TYPE_KEY)	{
#ifdef CONFIG_KTTECH_TSP_PLAT_O6
				if (fingerID == TOUCH_2KEY_BACK)
					input_report_key(ts->input, KEY_BACK, touchAction ? KEY_PRESS : KEY_RELEASE);
				if (fingerID == TOUCH_2KEY_MENU)
					input_report_key(ts->input, KEY_MENU, touchAction ? KEY_PRESS : KEY_RELEASE);

				input_sync(ts->input);
#endif
			}
		}

		/* 5. Determine Key ABS touch data */
		if(touchType == TOUCH_TYPE_SCREEN) {
			for(i=0; i<MELFAS_MAX_TOUCH; i++) {
#ifdef CONFIG_KTTECH_TSP_PLAT_O3
				if((TOUCH_3KEY_AREA_Y_BOTTOM > touch_msg_info[i].fingerY) && 
					(TOUCH_3KEY_AREA_Y_TOP < touch_msg_info[i].fingerY)) {
					/* Touch key processing */
					Process_Touch_KEY(ts, touch_msg_info, i);
				}
				else {
					/* if released */
					if (touch_msg_info[i].action == 0) {
						touch_msg_info[i].strength = 0;
						touch_msg_info[i].width = 0;
					}
					else {
						/* Touch position processing */
						REPORT_MT(touch_msg_info[i].id,
							touch_msg_info[i].fingerX,
							touch_msg_info[i].fingerY,
							touch_msg_info[i].strength,
							touch_msg_info[i].width);
						chkpress++;
					}
				}
#else
				/* if released */
				if (touch_msg_info[i].action == 0) {
					touch_msg_info[i].strength = 0;
					touch_msg_info[i].width = 0;
				}
				else {
					/* Touch position processing */
					REPORT_MT(touch_msg_info[i].id,
						touch_msg_info[i].fingerX,
						touch_msg_info[i].fingerY,
						touch_msg_info[i].strength,
						touch_msg_info[i].width);
					chkpress++;
				}
#endif
			}
			input_report_key(ts->input, BTN_TOUCH, !!chkpress);

			/* Touch report input sync */
			input_sync(ts->input);
		}
	}
	
	return;
}

static irqreturn_t mellfas_ts_threaded_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;

	/* Incrase IRQ counter */
	ts->irq_counter++;

#ifndef MELFAS_THREADED_IRQ	
	queue_delayed_work(melfas_ts_report_position_dwork_wq, &ts->melfas_ts_report_position_dwork, 0);
#else
#ifdef MELFAS_TS_USE_LOCK
	/* Acquire the lock. */
	if (down_interruptible(&ts->msg_sem)) { 
		dev_warn(&ts->client->dev, "[TSP] mellfas_ts_threaded_irq_handler Interrupted "
			"while waiting for msg_sem!\n");
	}
	else {
		melfas_ts_report_position(ts);
		/* Release the lock. */
		up(&ts->msg_sem);
	}

#else
	melfas_ts_report_position(ts);
#endif
#endif /* MELFAS_THREADED_IRQ */

	return IRQ_HANDLED;
}

static irqreturn_t home_key_irq(int irq, void *_ts) 
{

	struct	melfas_ts_data *ts = _ts;
	int home_key_status;

	home_key_status = gpio_get_value(ts->irq_key);

	dev_info(&ts->client->dev, "[TSP] Home Key Status : %d\n", home_key_status);
	
	if(home_key_status) {	/* Home Key Released */
		if(ts->ts_has_suspended == 0) {
			/* Recovery TSP */
			disable_irq(ts->client->irq);
				
			if (ts->pdata->suspend_platform_hw != NULL) {
				ts->ts_has_suspended = 1;
				ts->pdata->suspend_platform_hw(ts->pdata);
			}

			msleep(50);
	
			if (ts->pdata->resume_platform_hw != NULL) {
				ts->ts_has_suspended = 0;
				ts->pdata->resume_platform_hw(ts->pdata);
			}

			enable_irq(ts->client->irq);

			/* Release Home key and other keys */
			melfas_init_panel(ts);

#ifdef INIT_SET_CORRECTION_POS
			/* Initialize TS configuration */
			set_correction_x_pos_init(ts);
			set_correction_y_pos_init(ts);
#endif

#ifdef INIT_SET_FRAMERATE
			set_framerate_HZ_init(ts);
#endif

#ifdef MELFAS_USE_INPUT_MODE
			/* Set Input Mode */
			input_mode_init(ts);
#endif
			/* ESD Detection */
			if(ts->disabled_esd_detection == 1)
				disable_esd_detection(ts);
			else {
				if(delayed_work_pending(&ts->enable_esd_detect_dwork))
					cancel_delayed_work_sync(&ts->enable_esd_detect_dwork); 

				queue_delayed_work(enable_esd_detect_dwork_wq, &ts->enable_esd_detect_dwork, 100);
			}

			dev_info(&ts->client->dev, "[TSP] Old IRQ Count %d, Failed IRQ Count: %d\n", ts->irq_counter, ts->failed_counter);

			/* Enable Debugging information */
			debug = DEBUG_INFO; //DEBUG_TRACE;
		
			/* after 1sec, Disable debuging information */
			if(!delayed_work_pending(&ts->debug_dwork))
				queue_delayed_work(debug_dwork_wq, &ts->debug_dwork, INIT_LOW_DEBUG_TIME);

		}

		input_report_key(ts->input, KEY_HOME, KEY_RELEASE);
	}
	else {	/* Home Key Pressed */
		input_report_key(ts->input, KEY_HOME, KEY_PRESS);
	}

	input_sync(ts->input);

	return IRQ_HANDLED;
}

static ssize_t set_correction_x_pos(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t pos, variation;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(sscanf(buf, "%d %d", &variation, &pos) != 2) {
		dev_err(dev, "[TSP] Invalid value, eg) 1 5, 2 5.\n");
		return count;
	}

	buf_ts[0] = TS_REG_CMD_SET_CORRPOS_X;
	buf_ts[1] = (variation & 0xff);
	buf_ts[2] = (pos & 0xff);

	/* Save to val */
	data->correction_x_variant = (variation & 0xff);	
	data->correction_x = (pos & 0xff);	

	if(melfas_write_bytes(data->client, TS_REG_VNDR_CMDID, buf_ts, 3) < 0) {
		dev_err(dev, "[TSP] TS Framerate set failed.\n");
	}

 	if(melfas_read_bytes(data->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(dev, "[TSP] TS Framerate set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(dev, "[TSP] Set TS X pos Register set failed.(%d)\n", buf_ts[0]);
	else
		dev_info(dev, "[TSP] Set TS X pos Register set succeed.(%d)\n", buf_ts[0]);

	return count;
}

#ifdef MELFAS_USE_INPUT_MODE
static ssize_t show_input_mode(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "[TSP] Input mode : %d\n", data->input_mode);

	return count;
}

static ssize_t set_input_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t mode;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	mode = *buf;

	buf_ts[0] = TS_REG_CMD_SET_EDIT_MODE;
	buf_ts[1] = (mode & 0xff);

	/* Save to val */
	data->input_mode = mode;	

	if(melfas_write_bytes(data->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(dev, "[TSP] Input mode set failed.\n");
	}

	if(melfas_read_bytes(data->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(dev, "[TSP] Input mode set Register result read failed.\n");
	}

	if(buf_ts[0] != 0x1)
		dev_err(dev, "[TSP] Set Input mode Register set failed.(%d:%d)\n", buf_ts[0],buf_ts[1]);
	else
		dev_info(dev, "[TSP] Set Input mode Register set succeed.(%d:%d)\n", buf_ts[0],buf_ts[1]);

	return count;
}
#endif

static ssize_t show_correction_x_pos(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "X-variation : %d, pos : %d\n", data->correction_x_variant, data->correction_x);

	return count;
}

static ssize_t set_correction_y_pos(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t pos, variation;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(sscanf(buf, "%d %d", &variation, &pos) != 2) {
		dev_err(dev, "[TSP] Invalid value, eg) 1 5, 2 5.\n");
		return count;
	}

	buf_ts[0] = TS_REG_CMD_SET_CORRPOS_Y;
	buf_ts[1] = (variation & 0xff);
	buf_ts[2] = (pos & 0xff);

	/* Save to val */
	data->correction_y_variant = (variation & 0xff);	
	data->correction_y = (pos & 0xff);	

	if(melfas_write_bytes(data->client, TS_REG_VNDR_CMDID, buf_ts, 3) < 0) {
		dev_err(dev, "[TSP] TS Framerate set failed.\n");
	}

 	if(melfas_read_bytes(data->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(dev, "[TSP] TS Framerate set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(dev, "[TSP] Set TS Y pos Register set failed.(%d)\n", buf_ts[0]);
	else
		dev_info(dev, "[TSP] Set TS Y pos Register set succeed.(%d)\n", buf_ts[0]);

	return count;

}

static ssize_t show_correction_y_pos(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "Y-variation : %d, pos : %d\n", data->correction_y_variant, data->correction_y);

	return count;
}

#if 0
static ssize_t set_merge_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t merge_level;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(sscanf(buf, "%d", &merge_level) != 1) {
		dev_err(dev, "[TSP] Invalid value, eg) 10 to 60.\n");
		return count;
	}

	buf_ts[0] = TS_REG_CMD_SET_MERGE_LEVEL;
	buf_ts[1] = (merge_level & 0xff);

	if(merge_level < 10)
		merge_level = 10;

	if(merge_level > 60)
		merge_level = 60;
	
	/* Save to val */
	data->merge_level = merge_level;		

	if(melfas_write_bytes(data->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(dev, "[TSP] TS Merge Level set failed.\n");
	}

 	if(melfas_read_bytes(data->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(dev, "[TSP] TS Merge Level set result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(dev, "[TSP] TS Merge Level set failed.(%d)\n", buf_ts[0]);
	else
		dev_info(dev, "[TSP] TS Merge Level set succeed.(%d)\n", buf_ts[0]);

	return count;

}

static ssize_t show_merge_level(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "Merge Level : %d\n", data->merge_level);

	return count;
}
#endif

#ifdef INIT_SET_FRAMERATE
static ssize_t set_framerate_HZ(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t val;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(sscanf(buf, "%d", &val) != 1) {
		dev_err(dev, "[TSP] Invalid value, eg) 50(Hz).\n");
		return count;
	}

	buf_ts[0] = TS_REG_CMD_SET_FRAMERATE;
	buf_ts[1] = (val & 0xff);

	/* Save to Frame_rate val */
	data->framerate = (val & 0xff);
 	
	if(melfas_write_bytes(data->client, TS_REG_VNDR_CMDID, buf_ts, 2) < 0) {
		dev_err(dev, "[TSP] TS Framerate set failed.\n");
	}

 	if(melfas_read_bytes(data->client, TS_REG_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		dev_err(dev, "[TSP] TS Framerate set Register result read failed.\n");
	}

 	if(buf_ts[0] != 0x1)
		dev_err(dev, "[TSP] TS Framerate register set failed.(%d)\n", buf_ts[0]);
	else
		dev_info(dev, "[TSP] TS Framerate set register succeed.(%d)\n", buf_ts[0]);

	return count;
}

static ssize_t show_framerate_HZ(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "Framerate : %d(Hz)\n", data->framerate);

	return count;
}
#endif

static ssize_t set_debug_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	uint32_t val;

	if(sscanf(buf, "%d", &val) != 1) {
		dev_err(dev, "[TSP] Invalid value, eg) INFO(1), VERBOSE(2), MESSAGES(5), RAW(8), TRACE(10).\n");
		return count;
	}

	debug = val;
	dev_info(dev, "[TSP] Set current debug level : %d\n", debug);  
	
	return count;
}

static ssize_t show_debug_level(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	uint32_t count;

	count = sprintf(buf, "Current debug level : %d\n", debug);

	return count;
}

static ssize_t set_enable_key_wake(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t val;

	if(sscanf(buf, "%d", &val) != 1) {
		if(!(val == 0 || val == 1))
			dev_err(dev, "[TSP] Invalid value, eg) 0 or 1.\n");
			return count;
	}

	if(data->enable_key_wake == val)
		return count;

	if(val)
		enable_irq_wake(MSM_GPIO_TO_INT(data->irq_key));
	else
		disable_irq_wake(MSM_GPIO_TO_INT(data->irq_key));

	data->enable_key_wake = val;
	
	dev_info(dev, "[TSP] Set enable key wake : %d\n", val);  
	
	return count;
}

static ssize_t show_enable_key_wake(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "Current enable key wake : %d\n", data->enable_key_wake);

	return count;
}

static ssize_t store_firmware(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t val;
	uint8_t buff[6];

	if(sscanf(buf, "%d", &val) != 1) {
			dev_err(dev, "[TSP] Invalid value, eg) 1.\n");
			return count;
	}

	wake_lock(&data->wakelock);
	disable_irq(data->client->irq);

	/* Set Platform data */
	mcsdl_init_pdata(data->pdata);

	/* Start Download Firmware */
	val = mcsdl_download_binary_file(dev);

	if(val < 0)
		return count;

	/* Re-Initialize Platform H/W Configuration */
	if (data->pdata->init_platform_hw != NULL)
		data->pdata->init_platform_hw(data->pdata);
	
	/* Check TSP F/W version and download new F/W */	
	val = melfas_read_bytes(data->client, TS_REG_FW_VER, buff, 6);
	
	/* Print out of current H/W, F/W version */
	dev_info(&data->client->dev, "[TSP] Updated TSP F/W Version information.\n");
	dev_info(&data->client->dev, "[TSP] F/W version  [0x%x]\n", buff[0]);
	dev_info(&data->client->dev, "[TSP] H/W Revision [0x%x]\n", buff[1]);
	dev_info(&data->client->dev, "[TSP] Core F/W version [0x%x], Priv F/W Version [0x%x], Pub F/W Version [0x%x]\n", 
		buff[3], buff[4], buff[5]);	

	enable_irq(data->client->irq);
	wake_unlock(&data->wakelock);
	
	return count;
}

static ssize_t show_firmware(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count, error;
	uint8_t buff[6];

	/* Check TSP F/W version and download new F/W */	
	error = melfas_read_bytes(data->client, TS_REG_FW_VER, buff, 6);

	count = sprintf(buf, "MCS8000 : Firmware version [FW:0x%dx,CORE:0x%x,PRIV:0x%x,PUB:0x%x]\n",
				buff[0], buff[3], buff[4], buff[5]);
	
	return count;
}

static ssize_t show_version(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count, error;
	uint8_t buff[6];

	/* Check TSP F/W version and download new F/W */	
	error = melfas_read_bytes(data->client, TS_REG_FW_VER, buff, 6);	
	
	count = sprintf(buf, "%d\n", buff[5]);
	
	return count;
}

static ssize_t show_irq_count(struct device *dev, struct device_attribute *attr, char *buf)
{ 
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	uint32_t count;

	count = sprintf(buf, "MCS8000 : IRQ succeed count : %d, failed count : %d\n",
			data->irq_counter, data->failed_counter);
	
	return count;
}	


static ssize_t master_jig_test(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *data = dev_get_drvdata(dev);
	int r, t;
	char  *bufp;
	int16_t cmdata;
	uint8_t buff[6];

	bufp = buf;
	
	dev_info(&data->client->dev, "[TSP] Entering TSP self test mode.\n");

	buff[0] = UNIVCMD_ENTER_TEST_MODE;
	if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 1) < 0) {
			dev_err(dev, "[TSP] TS Enter test mode failed.\n");
	}

	while(gpio_get_value(data->pdata->irq_gpio)) {
		udelay(100);
	} // Low

	if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
		dev_err(dev, "[TSP] TS Enter test mode, result read failed.\n");
	}

	buff[0] = UNIVCMD_GET_PIXEL_CM_ABS;
	if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 1) < 0) {
		dev_err(dev, "[TSP] TS Enter test CM ABS failed.\n");
	}

	while(gpio_get_value(data->pdata->irq_gpio)) {
		udelay(100);
	} // Low	
	
	if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
		dev_err(dev, "[TSP] TS Enter test CM ABS, result read failed.\n");
	}	

	bufp += sprintf(bufp, "\n[TSP] CM ABS Result : \n");

	for (r = 0; r < TSP_CH_RX; r++) { //Model Dependent
		for (t = 0; t < TSP_CH_TX; t++) { //Model Dependent
			buff[0] = UNIVCMD_GET_PIXEL_CM_ABS;
			buff[1] = t; //Exciting CH.
			buff[2] = r; //Sensing CH.

			if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 3) < 0) {
				dev_err(dev, "[TSP] TS Enter test CM ABS, write CH failed.\n");
			}

			while(gpio_get_value(data->pdata->irq_gpio)) {
				udelay(100);
			} // Low

			if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
				dev_err(dev, "[TSP] TS Enter test CM ABS, write CH result read failed.\n");
			}
			
			if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT, buff, 1)) {
				dev_err(dev, "[TSP] TS Enter test CM ABS, write CH result read failed.\n");
			}

			cmdata = *(uint16_t*) buff;

			bufp += sprintf(bufp, "%5d,\t", cmdata);
		}
		bufp += sprintf(bufp, "\n");
	}

	bufp += sprintf(bufp, "\n[TSP] CM JITTER Result : \n");

	buff[0] = UNIVCMD_GET_PIXEL_CM_JITTER;	
	if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 1) < 0) {
		dev_err(dev, "[TSP] TS Enter test CM ABS failed.\n");
	}
	
	while(gpio_get_value(data->pdata->irq_gpio)) {
		udelay(100);
	} // Low

	if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
			dev_err(dev, "[TSP] TS Enter test CM JITTER, result read failed.\n");
	}	

	for (r = 0; r < TSP_CH_RX; r++) { //Model Dependent
		for (t = 0; t < TSP_CH_TX; t++) { //Model Dependent
			buff[0] = UNIVCMD_GET_PIXEL_CM_JITTER;
			buff[1] = t; //Exciting CH.
			buff[2] = r; //Sensing CH.

			if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 3) < 0) {
				dev_err(dev, "[TSP] TS Enter test CM JITTER, write CH failed.\n");
			}

			while(gpio_get_value(data->pdata->irq_gpio)) {
				udelay(100);
			} // Low

			if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
				dev_err(dev, "[TSP] TS Enter test CM JITTER, write CH result read failed.\n");
			}
					
			if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT, buff, 1)) {
				dev_err(dev, "[TSP] TS Enter test CM JITTER, write CH result read failed.\n");
			}
					
			cmdata = *(uint16_t*) buff;

			bufp += sprintf(bufp, "%5d,\t", cmdata);
		}
		bufp += sprintf(bufp, "\n");
	}

	bufp += sprintf(bufp, "\n[TSP] CM DELTA Result : \n");	

	buff[0] = UNIVCMD_TEST_CM_DELTA;
	if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 1) < 0) {
		dev_err(dev, "[TSP] TS Enter test CM ABS failed.\n");
	}	

	while(gpio_get_value(data->pdata->irq_gpio)) {
		udelay(100);
	} // Low


	if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
		dev_err(dev, "[TSP] TS Enter test CM JITTER, result read failed.\n");
	}	
			
	for (r = 0; r < TSP_CH_RX; r++) { //Model Dependent
		for (t = 0; t < TSP_CH_TX; t++) { //Model Dependent
			buff[0] = UNIVCMD_GET_PIXEL_CM_DELTA;
			buff[1] = t; //Exciting CH.
			buff[2] = r; //Sensing CH.
			
			if(melfas_write_bytes(data->client, UNIVERSAL_CMD, buff, 3) < 0) {
				dev_err(dev, "[TSP] TS Enter test CM JITTER, write CH failed.\n");
			}
			
			while(gpio_get_value(data->pdata->irq_gpio)) {
				udelay(100);
			} // Low
			
			if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT_SIZE, buff, 1)) {
				dev_err(dev, "[TSP] TS Enter test CM JITTER, write CH result read failed.\n");
			}
							
			if(!melfas_read_bytes(data->client, UNIVERSAL_CMD_RESULT, buff, 1)) {
				dev_err(dev, "[TSP] TS Enter test CM JITTER, write CH result read failed.\n");
			}
							
			cmdata = *(uint16_t*) buff;
			
			bufp += sprintf(bufp, "%5d,\t", cmdata);
		}
		bufp += sprintf(bufp, "\n");
	}

	return strlen(buf);
}

static DEVICE_ATTR(correction_x_pos, 0644, show_correction_x_pos, set_correction_x_pos);
static DEVICE_ATTR(correction_y_pos, 0644, show_correction_y_pos, set_correction_y_pos);
#if 0
static DEVICE_ATTR(merge_level, 0644, show_merge_level, set_merge_level);
static DEVICE_ATTR(framerate, 0644, show_framerate_HZ, set_framerate_HZ);
#endif
static DEVICE_ATTR(debug_level, 0644, show_debug_level, set_debug_level);
static DEVICE_ATTR(irq_count, 0444, show_irq_count, NULL);
static DEVICE_ATTR(enable_key_wake, 0644, show_enable_key_wake, set_enable_key_wake);
static DEVICE_ATTR(store_firmware, 0644, NULL, store_firmware);
static DEVICE_ATTR(show_firmware, 0444, show_firmware, NULL);
static DEVICE_ATTR(master_jig_test, 0444, master_jig_test, NULL);
#ifdef MELFAS_USE_INPUT_MODE
static DEVICE_ATTR(input_mode, 0644, show_input_mode, set_input_mode);
#endif
static DEVICE_ATTR(version, 0444, show_version, NULL);

static struct attribute *melfas_attributes[] = { 
	&dev_attr_correction_x_pos.attr,
	&dev_attr_correction_y_pos.attr,
#if 0
	&dev_attr_merge_level.attr,
#endif
#ifdef INIT_SET_FRAMERATE
	&dev_attr_framerate.attr,
#endif
	&dev_attr_debug_level.attr,
	&dev_attr_irq_count.attr,
	&dev_attr_enable_key_wake.attr,
	&dev_attr_store_firmware.attr,
	&dev_attr_show_firmware.attr,
	&dev_attr_master_jig_test.attr,
#ifdef MELFAS_USE_INPUT_MODE
	&dev_attr_input_mode.attr,
#endif
	&dev_attr_version.attr,
	NULL,
};

static struct attribute_group melfas_attr_group = { 
	.attrs = melfas_attributes,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);

	/* Set Suspend mode */
	ts->ts_has_suspended = 1;

	/* Cancel Workqueue */
	if(delayed_work_pending(&ts->enable_esd_detect_dwork))
		cancel_delayed_work_sync(&ts->enable_esd_detect_dwork); 

	if(delayed_work_pending(&ts->esd_detect_dwork))
		cancel_delayed_work_sync(&ts->esd_detect_dwork); 

#ifdef ENABLE_INFORM_CHERGER
	if(delayed_work_pending(&ts->ta_work))
		cancel_delayed_work_sync(&ts->ta_work); 
#endif

#ifndef MELFAS_THREADED_IRQ	
	if(delayed_work_pending(&ts->melfas_ts_report_position_dwork))
		cancel_delayed_work_sync(&ts->melfas_ts_report_position_dwork); 
#endif

	/* Disable pending log */
	if(delayed_work_pending(&ts->debug_dwork))
		cancel_delayed_work_sync(&ts->debug_dwork); 

	disable_irq(ts->client->irq);

	if (ts->irq_key) {
		if(ts->enable_key_wake == 0) {
			disable_irq(MSM_GPIO_TO_INT(ts->irq_key));
		}
	}
	
#ifdef MELFAS_SLEEP_POWEROFF
	if (ts->pdata->suspend_platform_hw != NULL)
		ts->pdata->suspend_platform_hw(ts->pdata);
#else
		/* TODO : Sleep configuration */
#endif 
	dev_info(&ts->client->dev,"[TSP] Suspend completed.\n");

	return;
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);

#ifdef MELFAS_SLEEP_POWEROFF
	if (ts->pdata->resume_platform_hw != NULL)
		ts->pdata->resume_platform_hw(ts->pdata);
#else
	/* TODO : resume configuration */
#endif 
	melfas_init_panel(ts);

	dev_info(&ts->client->dev, "[TSP] Old IRQ Count %d, Failed IRQ Count: %d\n", ts->irq_counter, ts->failed_counter);

	/* Enable Debugging information */
	debug = DEBUG_INFO; //DEBUG_TRACE;
	
	/* after 1sec, Disable debuging information */
	queue_delayed_work(debug_dwork_wq, &ts->debug_dwork, INIT_LOW_DEBUG_TIME);

	/* Initialized ESD  counter */
	ts->esd_counter = 0;

	/* IRQ Enable */
	enable_irq(ts->client->irq);

	if (ts->irq_key) {
		if(ts->enable_key_wake == 0) {
			enable_irq(MSM_GPIO_TO_INT(ts->irq_key));
		}
	}

	/* Set Suspend mode */
	ts->ts_has_suspended = 0;

#ifdef INIT_SET_FRAMERATE
	/* Initialize TS configuration */
	set_framerate_HZ_init(ts);
#endif

#ifdef INIT_SET_CORRECTION_POS
	/* Pos correction */
	set_correction_x_pos_init(ts);
	set_correction_y_pos_init(ts);
#endif

#ifdef ENABLE_INFORM_CHERGER
	if (ts->set_mode_for_ta && !work_pending(&ts->ta_work))
		queue_work(ta_work_wq, &ts->ta_work);
#endif
#ifdef MELFAS_USE_INPUT_MODE
	/* Set Input Mode */
	input_mode_init(ts);
#endif

	/* ESD Detection Enable */
	if(ts->disabled_esd_detection == 1)
		disable_esd_detection(ts);
	else {
		if(delayed_work_pending(&ts->enable_esd_detect_dwork))
			cancel_delayed_work_sync(&ts->enable_esd_detect_dwork); 
			
		queue_delayed_work(enable_esd_detect_dwork_wq, &ts->enable_esd_detect_dwork, 100);
	}
	
	dev_info(&ts->client->dev,"[TSP] Resume completed.\n");

	return;
}
#endif

/* boot initial delayed work */
static void melfas_boot_delayed_initial(struct work_struct *work)
{ 
	int error;
	struct melfas_ts_data *ts;
	ts = container_of(work, struct melfas_ts_data, initial_dwork.work);

	/* Allocate the interrupt data */
	ts->irq = ts->client->irq;
	ts->irq_counter = 0;
	ts->failed_counter = 0;
	ts->esd_counter = 0;
	ts->esd_enable_counter = 0;
	ts->disabled_esd_detection = 0;
	ts->input_mode = 0;
	ts->irq_key = ts->pdata->key_gpio;
	ts->ts_has_suspended = 0;

	if (debug >= DEBUG_MESSAGES) 
		dev_info(&ts->client->dev, "[TSP] %s\n", __func__);

	if (ts->irq) { 
	/* Try to request IRQ with falling edge first. This is
	     not always supported. If it fails, try with any edge. */
		error = request_threaded_irq(ts->irq,
			NULL,
			mellfas_ts_threaded_irq_handler,
			//IRQF_TRIGGER_FALLING, // | IRQF_ONESHOT,  // QC BUG : Can't use IRQF_TRIGGER_LOW, IRQF_ONESHOT
			IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			ts->client->dev.driver->name,
			ts);
		if (error < 0) { 
			error = request_threaded_irq(ts->irq,
				NULL,
				mellfas_ts_threaded_irq_handler,
				IRQF_DISABLED,
				ts->client->dev.driver->name,
				ts);
		}

		if (error < 0) { 
			dev_err(&ts->client->dev,
				"[TSP] failed to allocate irq %d\n", ts->irq);
		}
		else {		
			if (debug > DEBUG_INFO)
				dev_info(&ts->client->dev, "[TSP] touchscreen, irq %d\n", ts->irq);
		}
	}
	if(ts->irq_key) {
		error = request_threaded_irq(MSM_GPIO_TO_INT(ts->irq_key),
			NULL,
			home_key_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, // | IRQF_ONESHOT,
			ts->client->dev.driver->name,
			ts);
		if (error < 0) { 
			error = request_threaded_irq(MSM_GPIO_TO_INT(ts->irq_key),
				NULL,
				home_key_irq,
				IRQF_DISABLED,
				ts->client->dev.driver->name,
				ts);
		}			

		if (error < 0) { 
			dev_err(&ts->client->dev,
				"[TSP] failed to allocate home key irq %d\n", ts->irq_key);
		}

		if(ENABLE_HOME_KEY_WAKE)
			error = irq_set_irq_wake(MSM_GPIO_TO_INT(ts->irq_key), 1);
	
		if (error < 0) { 
			dev_err(&ts->client->dev,
				"[TSP] failed to enable wakeup src %d\n", ts->irq_key);
		}
		else {
			if (debug > DEBUG_INFO)
				dev_info(&ts->client->dev, "[TSP] touchscreen, key %d\n", ts->irq_key);
		}
	}

	msleep(5);
	
#if 1 // 12.04.06. Debugged. Workaround
	gpio_tlmm_config(GPIO_CFG(ts->irq_key, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	msleep(5);
	gpio_tlmm_config(GPIO_CFG(ts->irq_key, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	msleep(5);
#endif

	return;	
}

static int melfas_ftm_mode_probe(struct i2c_client *client) {
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

	/* Fill of input drv data :  touch information*/
	input_set_abs_params(input, ABS_MT_POSITION_X,  0, 540, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y,  0, 960, 0, 0);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH-1, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 30, 0, 0);
	__set_bit(EV_SYN, input->evbit);
	__set_bit(EV_KEY, input->evbit);

	/* Fill of input drv data : touch key */
	for (i = 0; i < NUMOF3KEYS; i++) { 
		__set_bit(tsp_3keycodes[i], input->keybit);
	}

	error = input_register_device(input);
	if (error < 0) { 
		dev_err(&client->dev,
			"[TSP] Failed to register input device.\n");
	}
	if (debug >= DEBUG_TRACE)
		dev_info(&client->dev, "[TSP] Succeed of register input device.\n");

	return 0;
}

static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data          *ts;
	struct melfas_ts_platform_data *pdata;
	struct input_dev               *input;
	int error = 0;
	int i;
	uint8_t buf[6];

	/* FTM mode supprt handler */
	if(get_kttech_ftm_mode() == 2) {
		pr_info("[TSP] Detected FTM boot mode.\n");
		error = melfas_ftm_mode_probe(client);
		if(error < 0) 
			pr_err("[TSP] TSP into FTM mode failed!\n");
		return 0;
	}

	if (client == NULL)
		pr_err("[TSP] Mach parameters error! : client == NULL\n");
	else if (client->adapter == NULL)
		pr_err("[TSP] Mach parameters error! : client->adapter == NULL\n");
	else if (&client->dev == NULL)
		pr_err("[TSP] Mach Parameters Error! : client->dev == NULL\n");
	else if (&client->adapter->dev == NULL)
		pr_err("[TSP] Mach Parameters Error! : client->adapter->dev == NULL\n");
	else if (id == NULL)
		pr_err("[TSP] Mach Parameters Error! : id == NULL\n");
	else
		goto param_check_ok;
	return -EINVAL;

param_check_ok:
	if (debug >= DEBUG_INFO) { 
		pr_info("[TSP] KT Tech O3/O6 Touch Screen Driver.\n");
		pr_info("\tname:\t \"%s\"\n", client->name);
		pr_info("\taddr:\t 0x%04x\n", client->addr);
		pr_info("\tirq:\t %d\n",	client->irq);
		pr_info("\tflags:\t 0x%04x\n", client->flags);
		pr_info("\tadapter: \"%s\"\n", client->adapter->name);
	}
	if (debug >= DEBUG_INFO)
		pr_info("[TSP] Parameters check OK.\n");;	

	/* Allocate structure - we need it to identify device */	
	ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
	input = input_allocate_device();
	if (!ts || !input)
	{
		dev_err(&client->dev, "[TSP] insufficient memory\n");
		error = -ENOMEM;
		goto err_input_dev_alloc_failed;
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

	ts->pdata = pdata;

	/* Initialize counter & valueables */
	ts->read_fail_counter = 0;
	ts->message_counter   = 0;
	ts->ts_has_suspended  = 0;

	/* Get KT Tech board rev */
	ts->pdata->board_rev = get_kttech_hw_version();

	if (debug >= DEBUG_INFO)
		pr_info("\tboard-revision:\t\"%d\"\n",	ts->pdata->board_rev);	

	ts->client = client;
	ts->input  = input;

	/* Initialize Platform H/W Configuration */
	if (ts->pdata->init_platform_hw != NULL)
		ts->pdata->init_platform_hw(ts->pdata);

	/* Check I2C communication */
	i2c_set_clientdata(client, ts);

	if (debug >= DEBUG_TRACE)
		dev_info(&client->dev, "[TSP] Setting i2c client data.\n");

	for(i=0; i<5; i++) {
		error = melfas_init_panel(ts);
		if(error < 0) {
			dev_err(&client->dev, "[TSP] Could not I2C communication. error = %d\n", error);
			dev_info(&client->dev, "[TSP] MCS8000 : Disabled regulator control. Re-initialize device.\n");
			
			/* Regulator disabled */
			regulator_disable(ts->pdata->reg_lvs3);
			regulator_disable(ts->pdata->reg_l4);
			regulator_disable(ts->pdata->reg_mvs0);
			regulator_put(ts->pdata->reg_lvs3);
			regulator_put(ts->pdata->reg_l4);
			regulator_put(ts->pdata->reg_mvs0);

			/* Initialize regulator valueables. */
			ts->pdata->reg_lvs3 = NULL;
			ts->pdata->reg_l4 = NULL;
			ts->pdata->reg_mvs0 = NULL;

			/* Initialize Platform H/W Configuration */
			if (ts->pdata->init_platform_hw != NULL)
				ts->pdata->init_platform_hw(ts->pdata);

			msleep(50);
		}
		else
			break; 
	}

	/* Finalize can't detected TSP. disable device. */
	if(error < 0) {
		dev_err(&client->dev, "[TSP] Could not I2C communication. error = %d\n", error);
#ifdef SET_RECOVERY_DOWNLOAD_BY_GPIO
		goto try_to_dl_recovery_fw;
#else
		goto err_communication_i2c;
#endif
	}
			
	/* Check TSP F/W version and download new F/W */	
	error = melfas_read_bytes(ts->client, TS_REG_FW_VER, buf, 6);

	/* Print out of current H/W, F/W version */
	dev_info(&client->dev, "[TSP] F/W version [0x%x]\n", buf[0]);
	dev_info(&client->dev, "[TSP] H/W Revision [0x%x]\n", buf[1]);
	dev_info(&client->dev, "[TSP] Core F/W version [0x%x], Priv F/W Version [0x%x], Pub F/W Version [0x%x]\n", 
		buf[3], buf[4], buf[5]);

	if(error < 0) {
		dev_err(&client->dev, "[TSP] Could not get TSP version. error = %d\n", error);
#ifdef SET_RECOVERY_DOWNLOAD_BY_GPIO
try_to_dl_recovery_fw:
#endif
		/* Set Platform data */
		mcsdl_init_pdata(pdata);

		if(ts->pdata->board_rev < 3)
			error = mcsdl_download_binary_legacy_data();
		else
			error = mcsdl_download_binary_data();
		
		dev_info(&client->dev, "[TSP] Try to download of recovery F/W!, completed. (%d)\n", error);		
		
		/* Re-Initialize Platform H/W Configuration */
		if (ts->pdata->init_platform_hw != NULL)
			ts->pdata->init_platform_hw(ts->pdata);

		/* Final TS Initialize panel */
		error = melfas_init_panel(ts);
		if(error < 0) {
			dev_err(&client->dev, "[TSP] Could not I2C communication. error = %d\n", error);
			goto err_communication_i2c;
		}
		
		/* Check TSP F/W version and download new F/W */	
		error = melfas_read_bytes(ts->client, TS_REG_FW_VER, buf, 6);

		/* Print out of current H/W, F/W version */
		dev_info(&client->dev, "[TSP] Recovery TSP F/W Version information.\n");
		dev_info(&client->dev, "[TSP] F/W version  [0x%x]\n", buf[0]);
		dev_info(&client->dev, "[TSP] H/W Revision [0x%x]\n", buf[1]);
		dev_info(&client->dev, "[TSP] Core F/W version [0x%x], Priv F/W Version [0x%x], Pub F/W Version [0x%x]\n", 
			buf[3], buf[4], buf[5]);	
	}
#ifdef SET_DOWNLOAD_BY_GPIO
	else if ((ts->pdata->board_rev < 3) && (buf[3] < CORE_FW_VERSION || buf[5] != PUB_FW_VERSION_LEGACY)) {
		/* For ES3 Tsp */
		
		/* Set Platform data */
		mcsdl_init_pdata(pdata);

		error = mcsdl_download_binary_legacy_data();

		dev_info(&client->dev, "[TSP] Try to download of Legacy version F/W!, completed. (%d)\n", error);	
		
		/* Re-Initialize Platform H/W Configuration */
		if (ts->pdata->init_platform_hw != NULL)
			ts->pdata->init_platform_hw(ts->pdata);

		/* Check TSP F/W version and download new F/W */	
		error = melfas_read_bytes(ts->client, TS_REG_FW_VER, buf, 6);

		/* Print out of current H/W, F/W version */
		dev_info(&client->dev, "[TSP] Updated Legacy TSP F/W Version information.\n");
		dev_info(&client->dev, "[TSP] F/W version  [0x%x]\n", buf[0]);
		dev_info(&client->dev, "[TSP] H/W Revision [0x%x]\n", buf[1]);
		dev_info(&client->dev, "[TSP] Core F/W version [0x%x], Priv F/W Version [0x%x], Pub F/W Version [0x%x]\n", 
			buf[3], buf[4], buf[5]);	
	}
	/* If TSP has lower F/W version then driver's F/W version, downloaded new F/W */
	//else if ((ts->pdata->board_rev >= 3) && ((buf[3] < CORE_FW_VERSION) || (buf[5] < PUB_FW_VERSION) || (buf[0] != buf[5]))) {
	else if ((ts->pdata->board_rev >= 3) && ((buf[3] < CORE_FW_VERSION) || (buf[0] < PUB_FW_VERSION))) {
#if 1
		/* Display upgrade Logo */
		load_arbg8888_qhd_image_tsp();
		
		/* Set Platform data */
		mcsdl_init_pdata(pdata);

		error = mcsdl_download_binary_data();

		dev_info(&client->dev, "[TSP] Try to download of new version F/W!, completed. (%d)\n", error);	
		
		/* Re-Initialize Platform H/W Configuration */
		if (ts->pdata->init_platform_hw != NULL)
			ts->pdata->init_platform_hw(ts->pdata);

		/* Check TSP F/W version and download new F/W */	
		error = melfas_read_bytes(ts->client, TS_REG_FW_VER, buf, 6);

		/* Print out of current H/W, F/W version */
		dev_info(&client->dev, "[TSP] Updated TSP F/W Version information.\n");
		dev_info(&client->dev, "[TSP] F/W version  [0x%x]\n", buf[0]);
		dev_info(&client->dev, "[TSP] H/W Revision [0x%x]\n", buf[1]);
		dev_info(&client->dev, "[TSP] Core F/W version [0x%x], Priv F/W Version [0x%x], Pub F/W Version [0x%x]\n", 
			buf[3], buf[4], buf[5]);	
#endif
	}
#endif

	/* Register callbacks for Charge detection */
#ifdef ENABLE_INFORM_CHERGER
	ts->callbacks.inform_charger = melfas_inform_charger_connection;
	if (ts->pdata->register_cb)
		ts->pdata->register_cb(&ts->callbacks);
#endif
	/* Create workqueues */
	initial_dwork_wq = create_singlethread_workqueue("tsp_initial_dwork_wq");
	if (!initial_dwork_wq)
			goto err_input_register_device_failed;

	debug_dwork_wq  = create_singlethread_workqueue("ts_debug_dwork_wq");
	if (!debug_dwork_wq)
			goto err_input_register_device_failed;

	esd_detect_dwork_wq = create_singlethread_workqueue("tsp_esd_detect_dwork_wq");
	if (!esd_detect_dwork_wq)
			goto err_input_register_device_failed;

	enable_esd_detect_dwork_wq = create_singlethread_workqueue("tsp_enable_esd_detect_dwork_wq");
	if (!enable_esd_detect_dwork_wq)
			goto err_input_register_device_failed;

#ifndef MELFAS_THREADED_IRQ
	melfas_ts_report_position_dwork_wq = create_singlethread_workqueue("tsp_melfas_ts_report_position_dwork_wq");
	if (!melfas_ts_report_position_dwork_wq)
		goto err_input_register_device_failed;
#endif
#ifdef ENABLE_INFORM_CHERGER
	ta_work_wq = create_singlethread_workqueue("tsp_ta_work_wq");
	if (!ta_work_wq)
		goto err_input_register_device_failed;
#endif

	/* Initialize of TSP worker threads */
	INIT_DELAYED_WORK(&ts->initial_dwork, melfas_boot_delayed_initial); 
	INIT_DELAYED_WORK(&ts->debug_dwork, debug_dworker);
	INIT_DELAYED_WORK(&ts->esd_detect_dwork, esd_detect_dworker); 
	INIT_DELAYED_WORK(&ts->enable_esd_detect_dwork, enable_esd_detection_dworker);
#ifndef MELFAS_THREADED_IRQ
	INIT_DELAYED_WORK(&ts->melfas_ts_report_position_dwork, melfas_ts_report_position_dworker);
#endif
	INIT_WORK(&ts->ta_work, melfas_ta_worker);
#ifndef init_MUTEX
        sema_init(&ts->msg_sem, 1);
#else
        init_MUTEX(&ts->msg_sem);
#endif

	/* Fill of input drv data : name */
	if (debug >= DEBUG_TRACE)
		dev_info(&client->dev, "[TSP] creating device name.\n");

	snprintf(
		ts->phys_name,
		sizeof(ts->phys_name),
		"%s/input0",
		dev_name(&client->dev)
		);

	input->name = "kttech_touchscreen";
	input->phys = ts->phys_name;
	input->id.bustype = BUS_I2C;
	input->dev.parent = &client->dev;

	if (debug >= DEBUG_INFO) { 
		dev_info(&client->dev, "[TSP] name: \"%s\"\n", input->name);
		dev_info(&client->dev, "[TSP] phys: \"%s\"\n", input->phys);
		dev_info(&client->dev, "[TSP] driver setting abs parameters\n");
	}
	__set_bit(BTN_TOUCH, input->keybit);
	__set_bit(EV_ABS, input->evbit);
	__set_bit(INPUT_PROP_DIRECT, input->propbit);

	/* Fill of input drv data :  touch information*/
	input_set_abs_params(input, ABS_MT_POSITION_X,  0, 540, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y,  0, 960, 0, 0);
	input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH-1, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 30, 0, 0);
	__set_bit(EV_SYN, input->evbit);
	__set_bit(EV_KEY, input->evbit);

	/* Fill of input drv data : touch key */
	for (i = 0; i < NUMOF3KEYS; i++) { 
		__set_bit(tsp_3keycodes[i], input->keybit);
	}

	/* Input set driver data */
	if (debug >= DEBUG_TRACE)
		dev_info(&client->dev, "[TSP] setting client data.\n");
	
	input_set_drvdata(input, ts);

	error = input_register_device(ts->input);
	if (error < 0) { 
		dev_err(&client->dev,
			"[TSP] Failed to register input device.\n");
		goto err_input_register_device_failed;
	}
	if (debug >= DEBUG_TRACE)
		dev_info(&client->dev, "[TSP] Succeed of register input device.\n");

	/* Initialized valueables */
	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)
		touch_msg_info[i].strength = -1;

	/* Configure of suspend information for android */
#if CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	ts->enable_key_wake = ENABLE_HOME_KEY_WAKE;
	register_early_suspend(&ts->early_suspend);
#endif

	/* Creat of sysfs group */
	error = sysfs_create_group(&client->dev.kobj, &melfas_attr_group);
	if (error) { 
		unregister_early_suspend(&ts->early_suspend);
		dev_err(&client->dev, "[TSP] fail sysfs_create_group\n");
		goto err_after_attr_group;
	}

	/* Register wake lock */
	wake_lock_init(&ts->wakelock, WAKE_LOCK_SUSPEND, "touch");

	/* after 0.5sec, start touch working */
	queue_delayed_work(initial_dwork_wq, &ts->initial_dwork, 50);

	if (debug >= DEBUG_INFO)
		dev_info(&client->dev, "[TSP] Touch screen driver probe Ok.\n");

	/* Final Initialize TS configuration */
	ts->framerate_initialized = 0;
	ts->correcton_initialized_x = 0;
	ts->correcton_initialized_y = 0;

#if 0
	set_correction_x_pos_init(ts);
	set_correction_y_pos_init(ts);
	set_framerate_HZ_init(ts);
#endif
	
	/* Driver initialize succeed */
	return 0;

err_after_attr_group:
	sysfs_remove_group(&client->dev.kobj, &melfas_attr_group);
	pr_err("[TSP] MCS8000 : Unregistered IRQ handler.\n");
	/* Free IRQ */
	if(ts->irq)
		free_irq(client->irq, ts);
	if(ts->irq_key)
		free_irq(ts->irq_key, ts);
err_input_register_device_failed:
	pr_err("[TSP] MCS8000 : Free input allocate device.(1)\n");

	input_free_device(ts->input);

	/* Destroy workqueues */
	if (initial_dwork_wq)
		destroy_workqueue(initial_dwork_wq);

	if (debug_dwork_wq)
		destroy_workqueue(debug_dwork_wq);

	if (esd_detect_dwork_wq)
		destroy_workqueue(esd_detect_dwork_wq);

	if (enable_esd_detect_dwork_wq)
		destroy_workqueue(enable_esd_detect_dwork_wq);
#ifndef MELFAS_THREADED_IRQ
	if (melfas_ts_report_position_dwork_wq)
		destroy_workqueue(melfas_ts_report_position_dwork_wq);
#endif
#ifdef ENABLE_INFORM_CHERGER
	if (ta_work_wq)
		destroy_workqueue(ta_work_wq);
#endif

err_communication_i2c:
	pr_err("[TSP] MCS8000 : Disabled regulator control.\n");
	/* Regulator disabled */
	regulator_disable(ts->pdata->reg_lvs3);
	regulator_disable(ts->pdata->reg_l4);
	regulator_disable(ts->pdata->reg_mvs0);
	regulator_put(ts->pdata->reg_lvs3);
	regulator_put(ts->pdata->reg_l4);
	regulator_put(ts->pdata->reg_mvs0);
err_input_dev_alloc_failed:
	input_free_device(ts->input);
	pr_err("[TSP] MCS8000 : Free input allocate device.(2)\n");
err_after_kmalloc:
	pr_err("[TSP] MCS8000 : Free input free malloc device.\n");
	kfree(ts);

	return error;
}

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	wake_lock_destroy(&ts->wakelock);
	unregister_early_suspend(&ts->early_suspend);
#endif
	/* Close down sysfs entries */
	sysfs_remove_group(&client->dev.kobj, &melfas_attr_group);

	if(ts->irq)
    		free_irq(client->irq, ts);
	if(ts->irq_key)
		free_irq(ts->irq_key, ts);
	
	input_unregister_device(ts->input);

	i2c_set_clientdata(client, NULL);

	/* Destroy workqueues */
	if (initial_dwork_wq)
		destroy_workqueue(initial_dwork_wq);

	if (debug_dwork_wq)
		destroy_workqueue(debug_dwork_wq);

	if (esd_detect_dwork_wq)
		destroy_workqueue(esd_detect_dwork_wq);

	if (enable_esd_detect_dwork_wq)
		destroy_workqueue(enable_esd_detect_dwork_wq);
#ifndef MELFAS_THREADED_IRQ
	if (melfas_ts_report_position_dwork_wq)
		destroy_workqueue(melfas_ts_report_position_dwork_wq);
#endif
#ifdef ENABLE_INFORM_CHERGER
	if (ta_work_wq)
		destroy_workqueue(ta_work_wq);
#endif

	/* Regulator disabled */
	regulator_disable(ts->pdata->reg_lvs3);
	regulator_disable(ts->pdata->reg_l4);
	regulator_disable(ts->pdata->reg_mvs0);
	regulator_put(ts->pdata->reg_lvs3);
	regulator_put(ts->pdata->reg_l4);
	regulator_put(ts->pdata->reg_mvs0);    

    kfree(ts);
	
	return 0;
}

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int melfas_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	disable_irq(client->irq);

	if (device_may_wakeup(&client->dev))
		enable_irq_wake(ts->irq);

	return 0;
}

static int melfas_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	if (device_may_wakeup(&client->dev))
		disable_irq_wake(ts->irq);

	return 0;
}
static SIMPLE_DEV_PM_OPS(melfas_pm, melfas_suspend, melfas_resume);
#else
#define melfas_ts_suspend NULL
#define melfas_ts_resume NULL
#endif

static const struct i2c_device_id melfas_ts_idtable[] =
{
	{ "kttech_o3_tsp", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, melfas_ts_idtable);

static struct i2c_driver melfas_ts_driver =
{
	.driver = {
		.name  = "kttech_o3_tsp",
		.owner = THIS_MODULE,
#ifndef CONFIG_HAS_EARLYSUSPEND 
                .pm     = &melfas_pm,
#endif
	},
	.id_table  = melfas_ts_idtable,
	.probe     = melfas_ts_probe,
	.remove    = __devexit_p(melfas_ts_remove),
};

static int __devinit melfas_ts_init(void)
{
	return i2c_add_driver(&melfas_ts_driver);
}

static void __exit melfas_ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);
	return;
}

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);

MODULE_DESCRIPTION("Melfas MCS8000 Touchscreen Controller");
MODULE_AUTHOR("Jhoon,Kim <jhoonkim@kttech.co.kr>");
MODULE_VERSION("0.2a");
MODULE_LICENSE("GPL");
