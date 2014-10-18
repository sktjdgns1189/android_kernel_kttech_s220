/*
 * linux/drivers/input/touchscreen/qt602240_kttech_v2.c
 *
 * KT Tech S200(O3) Platform Touch Screen Driver
 *
 * Copyright (C) 2011 KT Tech, Inc.
 * Author: Jhoonkim <jhoonkim@kttech.co.kr>
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

#include <asm/delay.h>
#include <asm/atomic.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/pmic.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/pmic8058-othc.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <mach/board.h>

#include "qt602240-kttech_v2.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#endif

/* ��ġ��ũ�� Tuning Revision ��� �� */
/* Ʃ�� ���� ���� ��� �ش� ������ ������ ��ü�Ѵ�. */
/* PP Tuning ������ ��� 
----------------------------------------------------------------------------------------
2011.05.11. Rev No. 1 : PP ITO (�϶� ������ ����) �÷ῡ ���� Default Configuration �ݿ�
2011.??.??. Rev No. ? : ���� �߰� Tuning�� �ݿ��� ��� �ش� �������� ���
----------------------------------------------------------------------------------------
*/
#define TSP_TUNING_REV_PP	1

/* MP Tuning ������ ��� 
----------------------------------------------------------------------------------------
2011.05.11. Rev No. 2 : MP ITO �÷ῡ ���� ���� ���� Tuning Data �ݿ�
2011.05.27. Rev No. 3 :���� Tunung Data �ݿ�, ����/������ ���� Config �ݿ�, Key ���� ó��
2011.05.31. Rev No. 4 :���� �� Touch ���� �ӵ� ����, Resume�� Calibration Ȱ��ȭ
----------------------------------------------------------------------------------------
*/
#define TSP_TUNING_REV_MP	4

/* ��ġ��ũ�� �ʱ�ȭ�� �������� ������ ���� ���� */
unsigned int init_ts_reg = 1;
unsigned int init_ts_p_data = 0;

/* ��ġ��ũ�� �ǽð� ��ǥ ������� ���� Level ���� */
/* ����� ���� 1 : ����ǥ ����� */
/* ����� ���� 2 : �޽��� ����� */
/* ����� ���� 3 : ����ƮID ����� */
/* ����� ���� 4 : ����ǥ ����Ʈ ������ ����� */
/* ����� ���� 5 : Calibration ���� ����� */
unsigned int debug_point_level = 0;

/* Android�� Early Suspend ���� */
#ifdef CONFIG_HAS_EARLYSUSPEND
static void qt602240_early_suspend(struct early_suspend *);  
static void qt602240_late_resume(struct early_suspend *); 
#endif

#ifdef TS_USE_WORK_QUEUE
struct workqueue_struct *qt602240_wq = NULL;
#endif
struct workqueue_struct *qt602240_cal_wq = NULL;
struct workqueue_struct *qt602240_autocal_wq = NULL;
struct workqueue_struct *qt602240_100ms_timer_wq = NULL;
struct workqueue_struct *qt602240_disable_freerun_wq = NULL;

/* ���� ����, Anti-Touch Calibration �Լ� ������
	 Data ����ü�� �ֱ⿡�� ������ ���꿡 ���� �ڵ� �ŷڼ� ������, 
	 Global ������ ����
 */
static unsigned int qt_timer_ticks=0;
static unsigned int qt_timer_flag=0;
static unsigned int touch_message_flag = 0;
static unsigned int cal_check_flag = 0;
static unsigned int touch_faceup_flag = 0;
static unsigned int recal_comp_flag = 0;
static unsigned int good_check_flag = 0;
static unsigned int doing_cal_flag = 0;
static unsigned int doing_cal_skip_cnt = 0;
static unsigned int finger_num = 0;

/* Charging State �����´�. ��Ƽ��� Configuration */
unsigned int qt602240_get_chg_state = QT602240_PWR_STATE_VBATT;
unsigned int qt602240_get_proximity_enable_state = 0;
unsigned int qt602240_get_proximity_enable_state_old = 0;
static unsigned int qt602240_get_chg_state_tmp = 0;

/* H/W Version ���� */
/* PP2�� Configuration���� ������ ���� �� ���� ������ ������ Ȱ��ȭ �Ѵ�. */
static int hw_ver = 0; //O3_PP2;
static int qt602240_btn_area_y_top = 0;

/* TSP Tunning Revision ���� */
static int tsp_tune_rev = 0;
static int tsp_fw_version = 0;

/* GSM ��� �׽�Ʈ���� ���Ǵ� Flag */
unsigned int qt602240_verification_ok = 0;

/* Suspend / Resume ���� ȸ�� �� */
atomic_t tsp_doing_suspend;

extern int msm_gpiomux_put(unsigned gpio);
extern int msm_gpiomux_get(unsigned gpio);

/******************************************************************************
*       QT602240 Object table init
* *****************************************************************************/
//General Object
gen_powerconfig_t7_config_t power_config = {0};			// Power config settings.
gen_acquisitionconfig_t8_config_t acquisition_config = {0};	// Acquisition config. 

//Touch Object
touch_multitouchscreen_t9_config_t touchscreen_config = {0};	// Multitouch screen config.
touch_keyarray_t15_config_t keyarray_config = {0};		// Key array config.
touch_proximity_t23_config_t proximity_config = {0};		// Proximity config. 

//Signal Processing Objects
proci_gripfacesuppression_t20_config_t gripfacesuppression_config = {0};	// Grip / face suppression config.
procg_noisesuppression_t22_config_t noise_suppression_config = {0};		// Noise suppression config.
proci_onetouchgestureprocessor_t24_config_t onetouch_gesture_config = {0};	// One-touch gesture config. 

//Support Objects
spt_gpiopwm_t19_config_t gpiopwm_config = {0};			// GPIO/PWM config
spt_selftest_t25_config_t selftest_config = {0};		// Selftest config.
spt_cteconfig_t28_config_t cte_config = {0};			// Capacitive touch engine config.

spt_comcconfig_t18_config_t comc_config = {0};			// Communication config settings.
gen_commandprocessor_t6_config_t command_config = {0};

/* qt602240�� ������Ʈ�� ���� �� �ִ� ���� �Ǵ� */
#ifndef KTTECH_FINAL_BUILD
static bool qt602240_object_readable(unsigned int type)
{
	switch (type) {
	case QT602240_GEN_MESSAGE:
	case QT602240_GEN_COMMAND:
	case QT602240_GEN_POWER:
	case QT602240_GEN_ACQUIRE:
	case QT602240_TOUCH_MULTI:
	case QT602240_TOUCH_KEYARRAY:
	case QT602240_TOUCH_PROXIMITY:
	case QT602240_PROCI_GRIPFACE:
	case QT602240_PROCG_NOISE:
	case QT602240_PROCI_ONETOUCH:
	case QT602240_SPT_COMMSCONFIG:
	case QT602240_SPT_GPIOPWM:
	case QT602240_SPT_SELFTEST:
	case QT602240_SPT_CTECONFIG:
	case QT602240_SPT_USERDATA:
		return true;
	default:
		return false;
	}
}
#endif

/* qt602240�� ������Ʈ�� ��� ��  �� �ִ� ���� �Ǵ� */
static bool qt602240_object_writable(unsigned int type)
{
	switch (type) {
	case QT602240_GEN_COMMAND:
	case QT602240_GEN_POWER:
	case QT602240_GEN_ACQUIRE:
	case QT602240_TOUCH_MULTI:
	case QT602240_TOUCH_KEYARRAY:
	case QT602240_TOUCH_PROXIMITY:
	case QT602240_PROCI_GRIPFACE:
	case QT602240_PROCG_NOISE:
	case QT602240_PROCI_ONETOUCH:
	case QT602240_SPT_GPIOPWM:
	case QT602240_SPT_SELFTEST:
	case QT602240_SPT_CTECONFIG:
		return true;
	default:
		return false;
	}
}

/* ������Ʈ�� ����Ʈ ID�� �޼����� �����ϱ� ���� ����� Functions */
static void qt602240_dump_message(struct device *dev, struct qt602240_message *message)
{	
	dev_info(dev, "reportid:\t0x%x\n", message->reportid);
	dev_info(dev, "message1:\t0x%x\n", message->message[0]);
	dev_info(dev, "message2:\t0x%x\n", message->message[1]);
	dev_info(dev, "message3:\t0x%x\n", message->message[2]);
	dev_info(dev, "message4:\t0x%x\n", message->message[3]);
	dev_info(dev, "message5:\t0x%x\n", message->message[4]);
	dev_info(dev, "message6:\t0x%x\n", message->message[5]);
	dev_info(dev, "message7:\t0x%x\n", message->message[6]);
	dev_info(dev, "checksum:\t0x%x\n", message->checksum);
}

/* ��Ʈ�δ��� CRC�� �˻��ϴ� Functions (��Ʈ�δ��� ������������ �Ǵ�) */
static int qt602240_check_bootloader(struct i2c_client *client,
				     unsigned int state)
{
	u8 val;

recheck:
	if (i2c_master_recv(client, &val, 1) != 1) {
		dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	switch (state) {
	case QT602240_WAITING_BOOTLOAD_CMD:
	case QT602240_WAITING_FRAME_DATA:
		val &= ~QT602240_BOOT_STATUS_MASK;
		break;
	case QT602240_FRAME_CRC_PASS:
		if (val == QT602240_FRAME_CRC_CHECK)
			goto recheck;
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(&client->dev, "Unvalid bootloader mode state\n");
		return -EINVAL;
	}

	return 0;
}

/* �߿�� ����� ��� ��Ʈ�δ��� Write Lock�� Ǯ���ִ� Functions */
static int qt602240_unlock_bootloader(struct i2c_client *client)
{
	u8 buf[2];

	buf[0] = QT602240_UNLOCK_CMD_LSB;
	buf[1] = QT602240_UNLOCK_CMD_MSB;

	if (i2c_master_send(client, buf, 2) != 2) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

/* �߿�� ����ϴ� �ڵ� */
static int qt602240_fw_write(struct i2c_client *client,
			     const u8 *data, unsigned int frame_size)
{
	if (i2c_master_send(client, data, frame_size) != frame_size) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

/* QT602240 I2C ���� Guard Functions */
static void QT602240_I2C_PORT_CONFIG(void)
{
	//printk("QT602240_I2C_PORT_CONFIG\n");
	gpio_tlmm_config(GPIO_CFG(QT602240_I2C_SCL, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(QT602240_I2C_SDA, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	msleep(10);
}


static void QT602240_I2C_PORT_DECONFIG(void)
{
	//printk("QT602240_I2C_PORT_DECONFIG\n");
	gpio_tlmm_config(GPIO_CFG(QT602240_I2C_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(QT602240_I2C_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(QT602240_I2C_SCL, 0);
	gpio_set_value(QT602240_I2C_SDA, 0);    
	msleep(10);
}

/* QT602240 I2C ���� Read/Write Functions */
static int __qt602240_read_reg(struct i2c_client *client,
			       u16 reg, u16 len, void *val)
{
	struct i2c_msg xfer[2];
	u8 buf[2];
	int retry;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 2;
	xfer[0].buf = buf;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = len;
	xfer[1].buf = val;

	for (retry = 0; retry <= QT602240_I2C_RETRY_TIMES; retry++) {
		if (i2c_transfer(client->adapter, xfer, 2) != 2) {
			dev_err(&client->dev, "%s: i2c transfer failed\n", __func__);
			QT602240_I2C_PORT_CONFIG();
			msleep(5);
		}
		else
			break;
	}

	if (retry > QT602240_I2C_RETRY_TIMES) {
		dev_err(&client->dev, "!!!! __qt602240_read_reg retry over %d\n",
			QT602240_I2C_RETRY_TIMES);
		return -EIO;
	}

	return 0;
}

static int qt602240_read_reg(struct i2c_client *client, u16 reg, u8 *val)
{
	return __qt602240_read_reg(client, reg, 1, val);
}

static int qt602240_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	u8 buf[3];
	int retry;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = val;

	for (retry = 0; retry <= QT602240_I2C_RETRY_TIMES; retry++) {
		if (i2c_master_send(client, buf, 3) != 3) {
			dev_err(&client->dev, "%s: i2c send failed\n", __func__);
			QT602240_I2C_PORT_CONFIG();
		}
		else
			break;
	}
  
	if (retry > QT602240_I2C_RETRY_TIMES) {
		dev_err(&client->dev, "!!!! qt602240_write_reg retry over %d\n",
			QT602240_I2C_RETRY_TIMES);
		return -EIO;
	}

	return 0;
}

/* QT602240 ������Ʈ �б�/���� �� �޼��� �б� ���� Functions */
static int qt602240_read_object_table(struct i2c_client *client,
					u16 reg, u8 *object_buf)
{
	return __qt602240_read_reg(client, reg, QT602240_OBJECT_SIZE,
					object_buf);
}

/* Object�� �޾ƿ��� �κ�, ������Ʈ type�� ��û�ϸ�, �ش� ������Ʈ�� �˻��ؼ� ����  
    Object�� �� ������ ��Ʈ�ѷ� �ʱ�ȭ �ÿ�, Information���� �޾ƿ�  data->info ����ü�� ����. 
    Start Address �� Objects�� ������ �޾Ƽ� �Ѱ��ش�. 
*/
static struct qt602240_object *qt602240_get_object(struct qt602240_data *data, u8 type)
{
	struct qt602240_object *object;
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		object = data->object_table + i;
		if (object->type == type)
			return object;
	}

	dev_err(&data->client->dev, "Invalid object type\n");
	return NULL;
}

/* QT602240���� Message�� �б� ���ؼ���, GEN_MESSAGE Ŀ�ǵ带 ��Ʈ�ѷ��� ������,
    �޽����� ���� ����� �Ѵ�. 
    �޽����� �����ϸ� �� �ڿ� Report ID�� ���ϵǸ� Report ID�� ��Ƽ��ġ��ũ������ Finger ��ȣ�� �ȴ�.
    struct qt602240_message {
	u8 reportid;
	u8 message[7];
	u8 checksum;
    }; 
*/
static int qt602240_read_message(struct qt602240_data *data, struct qt602240_message *message)
{
	struct qt602240_object *object;
	u16 reg;

	object = qt602240_get_object(data, QT602240_GEN_MESSAGE);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return __qt602240_read_reg(data->client, reg, sizeof(struct qt602240_message), message);
}

/* ������Ʈ ������ ����� Start Address�� �̿��Ͽ� �� ������Ʈ �����͸� ���� */
static int qt602240_read_object(struct qt602240_data *data,
				u8 type, u8 offset, u8 *val)
{
	struct qt602240_object *object;
	u16 reg;

	object = qt602240_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return __qt602240_read_reg(data->client, reg + offset, 1, val);
}

/* ������Ʈ ������ ����� Start Address�� �̿��Ͽ� �� ������Ʈ �����͸� ��� */
static int qt602240_write_object(struct qt602240_data *data, uint8_t type, uint8_t offset, uint8_t val)
{
	struct qt602240_object *object;
	u16 reg;

	object = qt602240_get_object(data, type);
	if (!object)
		return -EINVAL;

	reg = object->start_address;
	return qt602240_write_reg(data->client, reg + offset, val);
}

/* ������Ʈ Presets���� Write �ϱ� ���� Functions */
int qt602240_reg_write(struct qt602240_data *qt602240data,  uint8_t type, uint8_t *values)
{
	int i, count, retry;
	u8 buf[I2C_MAX_SEND_LENGTH];
	struct qt602240_object *object;
	struct i2c_client *client = qt602240data->client;
	
	object = qt602240_get_object(qt602240data, type);
	if (!object)
		dev_err(&client->dev,"error : 0x%x\n", object->type);
	
	if(object->size > ( I2C_MAX_SEND_LENGTH - 2 ))
		dev_err(&client->dev,"error : %s() data length error\n", __FUNCTION__);
	
	buf[0] = object->start_address & 0xff;
	buf[1] = (object->start_address >> 8) & 0xff;
	count = object->size + 3;        

	for (i = 0; i < object->size + 1; i++) {
		buf[i+2] = *(values+i);
	}

	for (retry = 0; retry <= QT602240_I2C_RETRY_TIMES; retry++) {
		if (i2c_master_send(client, buf, count) != count) {
			dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		}
		else
			break;
	}

	if (retry > QT602240_I2C_RETRY_TIMES) {
		dev_err(&client->dev, "error : qt602240_write_reg retry over %d\n",
			QT602240_I2C_RETRY_TIMES);
		return -EIO;
	}

	return 0;
}

/******************************************************************************
*       QT602240 Configuration Table Block
* *****************************************************************************/
int QT602240_Command_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) { 
		command_config.reset = 0x0;
		command_config.backupnv = 0x0;
		command_config.calibrate = 0x0;
		command_config.reportall = 0x0;
		command_config.reserve= 0x0;
		command_config.diagnostic = 0x0;
	}
	else if ( hw_ver >= O3_MP) {
		command_config.reset = 0x0;
		command_config.backupnv = 0x0;
		command_config.calibrate = 0x0;
		command_config.reportall = 0x0;
		command_config.reserve= 0x0;
		command_config.diagnostic = 0x0;	
	}
	
	return (qt602240_reg_write(data, QT602240_GEN_COMMAND, (void *) &command_config));
}

int QT602240_Powr_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		power_config.idleacqint = 64; //32;  //0x20;		// 0x20, 64ms in idle status
		power_config.actvacqint = 255; //16; //255; //0xff;		// free run in active status
		power_config.actv2idleto = 50; //20; //50; //0x32;	// 10s  
	}
	else if ( hw_ver >= O3_MP) {
		power_config.idleacqint = 32;  //0x20;		// 0x20, 64ms in idle status
		power_config.actvacqint = ACTVACQINT_VAL; //255; //16; //255; //0xff;		// free run in active status
		power_config.actv2idleto = 50; //20; //50; //0x32;	// 10s  		
	}
	
	return (qt602240_reg_write(data, QT602240_GEN_POWER, (void *) &power_config));
}

int QT602240_Acquisition_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		acquisition_config.chrgtime = 10; //0x0a;	// 0x8, 1500ns, Charge-transfer dwell time
		acquisition_config.reserved = 0x00;
		acquisition_config.tchdrift = 5; //0; //0x05;	// 1s, Touch drift time
		acquisition_config.driftst = 5;	// 1 cycle, Drift suspend time
		acquisition_config.tchautocal = 0x00;	// infinite Touch Automatic Calibration
		acquisition_config.sync = 0x00;		// disabled    
		acquisition_config.atchcalst = 0x09;	// 0x05, 1800ms, Anti-touch calibration time
		acquisition_config.atchcalsthr = 23; //35; //0x23;	// 0x14, Anti-touch Calibration suspend time
		acquisition_config.atchcalfrcthr = 5;
		acquisition_config.atchcalfrcratio = 0x0;
	}
	else if ( hw_ver >= O3_MP) {
		acquisition_config.chrgtime = 10; //0x0a;	// 0x8, 1500ns, Charge-transfer dwell time
		acquisition_config.reserved = 0x00;
		acquisition_config.tchdrift = 5; //0; //0x05;	// 1s, Touch drift time
		acquisition_config.driftst = 5;	// 1 cycle, Drift suspend time
		acquisition_config.tchautocal = 0x00; //0x00;	// infinite Touch Automatic Calibration
		acquisition_config.sync = 0x00;		// disabled    
		acquisition_config.atchcalst = ATCHCALST_VAL;	// 0x05, 1800ms, Anti-touch calibration time
		acquisition_config.atchcalsthr = ATCHCALSTHR_VAL; //0x23;	// 0x14, Anti-touch Calibration suspend time
		acquisition_config.atchcalfrcthr = 0;
		acquisition_config.atchcalfrcratio = 0x0;					
	}
	return (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config));
}

int QT602240_Multitouch_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		//0x80 :Scan en
		//0x8 : Disable vector change, 0x2: Enable reporting, 0x1 : Enable the multi-touch
		touchscreen_config.ctrl = 0x8f;		//Enable amplitude change : 0x0 << 3
		touchscreen_config.xorigin = 0x00;
		touchscreen_config.yorigin = 0x00;
		
		touchscreen_config.xsize = 19; //0x13;
		touchscreen_config.ysize = 11; //0x0b;
		touchscreen_config.akscfg = 0x00;
		touchscreen_config.blen = 16; //0x12;		// Gain of the analog circuits in front of the ADC [7:4]
		touchscreen_config.tchthr = 35; //50; //0x3c;		//0x27, touch Threshold value
		touchscreen_config.tchdi = 0x02;  
		touchscreen_config.orient = 0x01;	// 0x4 : Invert Y, 0x2 : Invert X, 0x1 : Switch
		
		touchscreen_config.mrgtimeout = 0x00;               
		touchscreen_config.movhysti = 3; //0x00;	// 0x1, Move hysteresis, initial
		touchscreen_config.movhystn = 1; //0x00;	// 0x1, Move hysteresis, next    
		touchscreen_config.movfilter = 13; //0x00;	// Filter Limit[6:4] , Adapt threshold [3:0]
		touchscreen_config.numtouch= 0x06;        
		touchscreen_config.mrghyst = 3; //10; // 0x00;	// Merge hysteresis
		touchscreen_config.mrgthr = 70; //10; //0x00;	// 0xa, Merge threshold
		touchscreen_config.amphyst = 6; //10; //0x00;	// Amplitude hysteresis
		touchscreen_config.xrange1= 0x1b;	// 2byte
		touchscreen_config.xrange2= 0x02;
		
		touchscreen_config.yrange1 = 0xff;	//2byte
		touchscreen_config.yrange2 = 0x03;    
		touchscreen_config.xloclip = 5; //0x00;
		touchscreen_config.xhiclip = 5; //0x00;
		touchscreen_config.yloclip = 19; //0x00;
		touchscreen_config.yhiclip = 25; //0x00;
		touchscreen_config.xedgectrl = 136; //0x00;
		touchscreen_config.xedgedist = 40; //40; //0x00;
		touchscreen_config.yedgectrl = 202; //0x00;
		touchscreen_config.yedgedist = 40; //0x00;
		touchscreen_config.jumplimit = 15; //0x00;	//18*8
		touchscreen_config.tchhyst = 0x00;
	}
	else if ( hw_ver >= O3_MP) {
		touchscreen_config.ctrl = 0x8b; //0x8f;		//Enable amplitude change : 0x0 << 3
		touchscreen_config.xorigin = 0x00;
		touchscreen_config.yorigin = 0x00;
		
		touchscreen_config.xsize = 19; //0x13;
		touchscreen_config.ysize = 11; //0x0b;
		touchscreen_config.akscfg = 0x00;
		touchscreen_config.blen = 32; //0x12;		// Gain of the analog circuits in front of the ADC [7:4]
		touchscreen_config.tchthr = TCHTHR_VAL_VBATT; //0x3c;		//0x27, touch Threshold value
		touchscreen_config.tchdi = 0x02; //0x03;  
		touchscreen_config.orient = 0x01;	// 0x4 : Invert Y, 0x2 : Invert X, 0x1 : Switch
		
		touchscreen_config.mrgtimeout = 0x00;               
		touchscreen_config.movhysti = 3; //0x00;	// 0x1, Move hysteresis, initial
		touchscreen_config.movhystn = 1; //0x00;	// 0x1, Move hysteresis, next    
		touchscreen_config.movfilter = 13; //0x00;	// Filter Limit[6:4] , Adapt threshold [3:0]
		touchscreen_config.numtouch= 0x06;        
		touchscreen_config.mrghyst = 3; //10; // 0x00;	// Merge hysteresis
		touchscreen_config.mrgthr = 50; //10; //0x00;	// 0xa, Merge threshold
		touchscreen_config.amphyst = 6; //10; //0x00;	// Amplitude hysteresis
		touchscreen_config.xrange1= 0x1b;	// 2byte
		touchscreen_config.xrange2= 0x02;
		
		touchscreen_config.yrange1 = 0xff;	//2byte
		touchscreen_config.yrange2 = 0x03;    
		touchscreen_config.xloclip = 5; //0x00;
		touchscreen_config.xhiclip = 5; //0x00;
		touchscreen_config.yloclip = 19; //0x00;
		touchscreen_config.yhiclip = 25; //0x00;
		touchscreen_config.xedgectrl = 136; //0x00;
		touchscreen_config.xedgedist = 70; //0x00;
		touchscreen_config.yedgectrl = 202; //0x00;
		touchscreen_config.yedgedist = 70;
		touchscreen_config.jumplimit = 15; //0x00;	//18*8
		touchscreen_config.tchhyst = 0x00;			
	}
	
	return (qt602240_reg_write(data, QT602240_TOUCH_MULTI, (void *) &touchscreen_config));
}

int QT602240_KeyArrary_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		keyarray_config.ctrl = 0x00;
		keyarray_config.xorigin = 0x00;
		keyarray_config.yorigin = 0x00;
		keyarray_config.xsize = 0x00;
		keyarray_config.ysize = 0x00;
		keyarray_config.akscfg = 0x00;
		keyarray_config.blen = 0x00;
		keyarray_config.tchthr = 0x00;
		keyarray_config.tchdi = 0x00;
		keyarray_config.reserved[0] = 0x00;
		keyarray_config.reserved[1] = 0x00;
	}
	else if ( hw_ver >= O3_MP) {
		keyarray_config.ctrl = 0x00;
		keyarray_config.xorigin = 0x00;
		keyarray_config.yorigin = 0x00;
		keyarray_config.xsize = 0x00;
		keyarray_config.ysize = 0x00;
		keyarray_config.akscfg = 0x00;
		keyarray_config.blen = 0x00;
		keyarray_config.tchthr = 0x00;
		keyarray_config.tchdi = 0x00;
		keyarray_config.reserved[0] = 0x00;
		keyarray_config.reserved[1] = 0x00;			
	}	
	
	return (qt602240_reg_write(data, QT602240_TOUCH_KEYARRAY, (void *) &keyarray_config));
}

int QT602240_GPIOPWM_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		gpiopwm_config.ctrl = 0x00;
		gpiopwm_config.reportmask = 0x00;
		gpiopwm_config.dir = 0x00;
		gpiopwm_config.intpullup = 0x00;
		gpiopwm_config.out = 0x00;
		gpiopwm_config.wake = 0x00;
		gpiopwm_config.pwm = 0x00;
		gpiopwm_config.period = 0x00;
		gpiopwm_config.duty[0] = 0x00;
		gpiopwm_config.duty[1] = 0x00;
		gpiopwm_config.duty[2] = 0x00;
		gpiopwm_config.duty[3] = 0x00;
		gpiopwm_config.trigger[0] = 0x00;
		gpiopwm_config.trigger[1] = 0x00;
		gpiopwm_config.trigger[2] = 0x00;
		gpiopwm_config.trigger[3] = 0x00;
	}
	else if ( hw_ver >= O3_MP) {
		gpiopwm_config.ctrl = 0x00;
		gpiopwm_config.reportmask = 0x00;
		gpiopwm_config.dir = 0x00;
		gpiopwm_config.intpullup = 0x00;
		gpiopwm_config.out = 0x00;
		gpiopwm_config.wake = 0x00;
		gpiopwm_config.pwm = 0x00;
		gpiopwm_config.period = 0x00;
		gpiopwm_config.duty[0] = 0x00;
		gpiopwm_config.duty[1] = 0x00;
		gpiopwm_config.duty[2] = 0x00;
		gpiopwm_config.duty[3] = 0x00;
		gpiopwm_config.trigger[0] = 0x00;
		gpiopwm_config.trigger[1] = 0x00;
		gpiopwm_config.trigger[2] = 0x00;
		gpiopwm_config.trigger[3] = 0x00;	
	}
	
	return (qt602240_reg_write(data, QT602240_SPT_GPIOPWM, (void *) &gpiopwm_config));
}

int QT602240_Grip_Face_Suppression_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		//int version = data->info->version;
		gripfacesuppression_config.ctrl = 0x07;		//0x4 : Disable Grip suppression, 0x2: Enable report, 0x1 : Enable
		gripfacesuppression_config.xlogrip = 0x00;
		gripfacesuppression_config.xhigrip = 0x00;
		gripfacesuppression_config.ylogrip = 0x00;
		gripfacesuppression_config.yhigrip = 0x00;
		gripfacesuppression_config.maxtchs = 0x00;
		gripfacesuppression_config.reserved = 0x00;
		gripfacesuppression_config.szthr1 = 80; //0x50;
		gripfacesuppression_config.szthr2 = 40; //0x28;
		gripfacesuppression_config.shpthr1 = 4; //0x04;
		gripfacesuppression_config.shpthr2 = 35; //0x0f;
		gripfacesuppression_config.supextto = 10; //0x0a;    
	}
	else if ( hw_ver >= O3_MP) {
#if 0 // Use Face-Up Detection
		gripfacesuppression_config.ctrl = 0x00;;		//0x4 : Disable Grip suppression, 0x2: Enable report, 0x1 : Enable
		gripfacesuppression_config.xlogrip = 0x00;
		gripfacesuppression_config.xhigrip = 0x00;
		gripfacesuppression_config.ylogrip = 0x00;
		gripfacesuppression_config.yhigrip = 0x00;
		gripfacesuppression_config.maxtchs = 0x00;
		gripfacesuppression_config.reserved = 0x00;
		gripfacesuppression_config.szthr1 = 30; //0x50;
		gripfacesuppression_config.szthr2 = 20; //0x28;
		gripfacesuppression_config.shpthr1 = 4; //0x04;
		gripfacesuppression_config.shpthr2 = 15; //0x0f;
		gripfacesuppression_config.supextto = 0; //0x0a;    	
#else
		gripfacesuppression_config.ctrl = 7;		//0x4 : Disable Grip suppression, 0x2: Enable report, 0x1 : Enable
		gripfacesuppression_config.xlogrip = 0x00;
		gripfacesuppression_config.xhigrip = 0x00;
		gripfacesuppression_config.ylogrip = 0x00;
		gripfacesuppression_config.yhigrip = 0x00;
		gripfacesuppression_config.maxtchs = 0x00;
		gripfacesuppression_config.reserved = 0x00;
		gripfacesuppression_config.szthr1 = 30; //0x50;
		gripfacesuppression_config.szthr2 = 20; //0x28;
		gripfacesuppression_config.shpthr1 = 4; //0x04;
		gripfacesuppression_config.shpthr2 = 15; //0x0f;
		gripfacesuppression_config.supextto = 0; //0x0a;    
#endif
	}	
	
	/* Write grip suppression config to chip. */
	return (qt602240_reg_write(data, QT602240_PROCI_GRIPFACE, (void *) &gripfacesuppression_config));
}

int QT602240_Noise_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		//int version = data->info->version;
		//0x8 : Enable Median filter, 0x4 : Enable Frequency hopping, 0x1 : Enable
		noise_suppression_config.ctrl = 0x05;    
		noise_suppression_config.reserved = 0x00;
		noise_suppression_config.reserved1 = 0x00; 
		noise_suppression_config.gcaful1 = 0x00;	// Upper limit for the GCAF
		noise_suppression_config.gcaful2 = 0x00;
		noise_suppression_config.gcafll1 = 0x00;	// Lower limit for the GCAF
		noise_suppression_config.gcafll2 = 0x00;
		noise_suppression_config.actvgcafvalid = 0x03;	// 0x0f, Minium number of samples in active mode
		noise_suppression_config.noisethr = 30; //35; //0x23;	// 0x0f, Threshold for the noise signal
		noise_suppression_config.reserved2 = 0x00;
		noise_suppression_config.freqhopscale = 0x00;	//0x1e;
		noise_suppression_config.freq[0] = 0; //0x05;
		noise_suppression_config.freq[1] = 5; //0x0f;
		noise_suppression_config.freq[2] = 15; //0x19;
		noise_suppression_config.freq[3] = 25; //0x23;
		noise_suppression_config.freq[4] = 35; //0x2d;
		noise_suppression_config.idlegcafvalid = 0x03;
	}
	else if ( hw_ver >= O3_MP) {
		noise_suppression_config.ctrl = 133;    
		noise_suppression_config.reserved = 0x00;
		noise_suppression_config.reserved1 = 0x00; 
		noise_suppression_config.gcaful1 = 0x00;	// Upper limit for the GCAF
		noise_suppression_config.gcaful2 = 0x00;
		noise_suppression_config.gcafll1 = 0x00;	// Lower limit for the GCAF
		noise_suppression_config.gcafll2 = 0x00;
		noise_suppression_config.actvgcafvalid = 0x03;	// 0x0f, Minium number of samples in active mode
		noise_suppression_config.noisethr = 58; //35; //0x23;	// 0x0f, Threshold for the noise signal
		noise_suppression_config.reserved2 = 0x00;
		noise_suppression_config.freqhopscale = 0x00;	//0x1e;
		noise_suppression_config.freq[0] = 9; //0x05;
		noise_suppression_config.freq[1] = 15; //0x0f;
		noise_suppression_config.freq[2] = 24; //0x19;
		noise_suppression_config.freq[3] = 34; //0x23;
		noise_suppression_config.freq[4] = 255; //0x2d;
		noise_suppression_config.idlegcafvalid = 0x03;			
	}	
		
	/* Write Noise suppression config to chip. */
	return (qt602240_reg_write(data, QT602240_PROCG_NOISE, (void *) &noise_suppression_config));
}

int QT602240_Proximity_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		/* Disable Proximity. */
		proximity_config.ctrl = 0x00;
		proximity_config.xorigin = 0x00;
		proximity_config.yorigin = 0x00;
		proximity_config.xsize = 0x00;
		proximity_config.ysize = 0x00;
		proximity_config.reserved_for_future_aks_usage = 0x00;
		proximity_config.blen = 0x00;
		proximity_config.tchthr1 = 0x00;
		proximity_config.tchthr2 = 0x00;
		proximity_config.fxddi = 0x00;
		proximity_config.average = 0x00;
		proximity_config.rate = 0x00;
		proximity_config.rate_reserved = 0x00;
		proximity_config.mvdthr = 0x00;
		proximity_config.mvdthr_reserved = 0x00;
	}
	else if ( hw_ver >= O3_MP) {
		proximity_config.ctrl = 0x00;
		proximity_config.xorigin = 0x00;
		proximity_config.yorigin = 0x00;
		proximity_config.xsize = 0x00;
		proximity_config.ysize = 0x00;
		proximity_config.reserved_for_future_aks_usage = 0x00;
		proximity_config.blen = 0x00;
		proximity_config.tchthr1 = 0x00;
		proximity_config.tchthr2 = 0x00;
		proximity_config.fxddi = 0x00;
		proximity_config.average = 0x00;
		proximity_config.rate = 0x00;
		proximity_config.rate_reserved = 0x00;
		proximity_config.mvdthr = 0x00;
		proximity_config.mvdthr_reserved = 0x00;				
	}	
	
	return (qt602240_reg_write(data, QT602240_TOUCH_PROXIMITY, (void *) &proximity_config));
}

int QT602240_One_Touch_Gesture_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		/* Disable one touch gestures. */
		onetouch_gesture_config.ctrl = 0x00;
		onetouch_gesture_config.numgest = 0x00;   
		onetouch_gesture_config.gesten1 = 0x00;		// 2byte
		onetouch_gesture_config.gesten2 = 0x00;
		onetouch_gesture_config.pressproc = 0x00;
		onetouch_gesture_config.tapto = 0x00;
		onetouch_gesture_config.flickto = 0x00;
		onetouch_gesture_config.dragto = 0x00;
		onetouch_gesture_config.spressto = 0x00;
		onetouch_gesture_config.lpressto = 0x00;
		onetouch_gesture_config.reppressto = 0x00;
		onetouch_gesture_config.flickthr1 = 0x00;		// 2byte
		onetouch_gesture_config.flickthr2 = 0x00;
		onetouch_gesture_config.dragthr1 = 0x00;		// 2byte    
		onetouch_gesture_config.dragthr2 = 0x00;
		onetouch_gesture_config.tapthr1 = 0x00;		// 2byte
		onetouch_gesture_config.tapthr2 = 0x00;
		onetouch_gesture_config.throwthr1 = 0x00;		// 2byte
		onetouch_gesture_config.throwthr2 = 0x00;
	}
	else if ( hw_ver >= O3_MP) {
		onetouch_gesture_config.ctrl = 0x00;
		onetouch_gesture_config.numgest = 0x00;   
		onetouch_gesture_config.gesten1 = 0x00;		// 2byte
		onetouch_gesture_config.gesten2 = 0x00;
		onetouch_gesture_config.pressproc = 0x00;
		onetouch_gesture_config.tapto = 0x00;
		onetouch_gesture_config.flickto = 0x00;
		onetouch_gesture_config.dragto = 0x00;
		onetouch_gesture_config.spressto = 0x00;
		onetouch_gesture_config.lpressto = 0x00;
		onetouch_gesture_config.reppressto = 0x00;
		onetouch_gesture_config.flickthr1 = 0x00;		// 2byte
		onetouch_gesture_config.flickthr2 = 0x00;
		onetouch_gesture_config.dragthr1 = 0x00;		// 2byte    
		onetouch_gesture_config.dragthr2 = 0x00;
		onetouch_gesture_config.tapthr1 = 0x00;		// 2byte
		onetouch_gesture_config.tapthr2 = 0x00;
		onetouch_gesture_config.throwthr1 = 0x00;		// 2byte
		onetouch_gesture_config.throwthr2 = 0x00;			
	}	
		
	return (qt602240_reg_write(data, QT602240_PROCI_ONETOUCH, (void *) &onetouch_gesture_config));
}

int QT602240_Selftest_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {
		selftest_config.ctrl = 0;
		selftest_config.cmd = 0;
	}
	else if ( hw_ver >= O3_MP) {
		selftest_config.ctrl = 0;
		selftest_config.cmd = 0;		
	}
			
	return (qt602240_reg_write(data, QT602240_SPT_SELFTEST, (void *) &selftest_config));
}

int QT602240_CTE_Config_Init(struct qt602240_data *data)
{
	if (hw_ver <= O3_PP2) {	
		/* Set CTE config */
		cte_config.ctrl = 1; //0x00;				// Reserved
		cte_config.cmd = 0x00; 
		
		cte_config.mode = 3; //0x00;
		cte_config.idlegcafdepth = 32; //8; //0x08;			// 0x20, Size of sampling window in idle acquisition mode
		cte_config.actvgcafdepth = 63; //32; //16; //0x10;		// 0x63, Size of sampling window in active acquisition mode
		cte_config.voltage = 60; //0x00;			// 0.01 * 10 + 2.7
	}
	else if ( hw_ver >= O3_MP) {
		/* Set CTE config */
		cte_config.ctrl = 1; //0x00;				// Reserved
		cte_config.cmd = 0x00; 
		
		cte_config.mode = 3; //0x00;
		cte_config.idlegcafdepth = 32; //8; //0x08;			// 0x20, Size of sampling window in idle acquisition mode
		cte_config.actvgcafdepth = 63; //32; //16; //0x10;		// 0x63, Size of sampling window in active acquisition mode
		cte_config.voltage = 0x00;			// 0.01 * 10 + 2.7			
	}
		
	/* Write CTE config to chip. */
	return (qt602240_reg_write(data, QT602240_SPT_CTECONFIG, (void *) &cte_config));
}

/* ��ġ ��ũ���� X,Y ����� Ȯ���ϰ� �����Ѵ�. */
static int qt602240_check_matrix_size(struct qt602240_data *data)
{
	const struct qt602240_platform_data *pdata = data->pdata;
	struct device *dev = &data->client->dev;
	int mode = -1;
	int error;
	u8 val;

	dev_dbg(dev, "Number of X lines: %d\n", pdata->x_line);
	dev_dbg(dev, "Number of Y lines: %d\n", pdata->y_line);

	switch (pdata->x_line) {
	case 0 ... 15:
		if (pdata->y_line <= 14)
			mode = 0;
		break;
	case 16:
		if (pdata->y_line <= 12)
			mode = 1;
		if (pdata->y_line == 13 || pdata->y_line == 14)
			mode = 0;
		break;
	case 17:
		if (pdata->y_line <= 11)
			mode = 2;
		if (pdata->y_line == 12 || pdata->y_line == 13)
			mode = 1;
		break;
	case 18:
		if (pdata->y_line <= 10)
			mode = 3;
		if (pdata->y_line == 11 || pdata->y_line == 12)
			mode = 2;
		break;
	case 19:
		if (pdata->y_line <= 9)
			mode = 4;
		if (pdata->y_line == 10 || pdata->y_line == 11)
			mode = 3;
		break;
	case 20:
		mode = 4;
	}

	if (mode < 0) {
		dev_err(dev, "Invalid X/Y lines\n");
		return -EINVAL;
	}

	error = qt602240_read_object(data, QT602240_SPT_CTECONFIG,
				QT602240_CTE_MODE, &val);
	if (error)
		return error;

	if (mode == val)
		return 0;

	/* Change the CTE configuration */
	qt602240_write_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_CTRL, 1);
	qt602240_write_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_MODE, mode);
	qt602240_write_object(data, QT602240_SPT_CTECONFIG,
			QT602240_CTE_CTRL, 0);

	return 0;
}

/* ��ġ ��ũ���� �⺻���� H/W �����͸� �����ϴ� Function */
static void qt602240_handle_pdata(struct qt602240_data *data)
{
	const struct qt602240_platform_data *pdata = data->pdata;
#if 0
	u8 voltage;
#endif

	/* Set touchscreen lines */
	qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_XSIZE,
			pdata->x_line);
			
	qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_YSIZE,
			pdata->y_line);

	/* Set touchscreen orient */
	qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_ORIENT,
			pdata->orient);

#if 0	// Don't Used Burst Length of F/W 2.0/1.6
	/* Set touchscreen burst length */
	qt602240_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_BLEN, pdata->blen);

	/* Set touchscreen threshold */
	qt602240_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_TCHTHR, pdata->threshold);
#endif

	/* Set touchscreen resolution */
	qt602240_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_XRANGE_LSB, (pdata->x_size - 1) & 0xff);
	qt602240_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_XRANGE_MSB, (pdata->x_size - 1) >> 8);
	qt602240_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_YRANGE_LSB, (pdata->y_size - 1) & 0xff);
	qt602240_write_object(data, QT602240_TOUCH_MULTI,
			QT602240_TOUCH_YRANGE_MSB, (pdata->y_size - 1) >> 8);

#if 0	// Don't Used Burst Length of F/W 2.0/1.6
	/* Set touchscreen voltage */
	if (data->info.version >= QT602240_VER_21 && pdata->voltage) {
		if (pdata->voltage < QT602240_VOLTAGE_DEFAULT) {
			voltage = (QT602240_VOLTAGE_DEFAULT - pdata->voltage) /
				QT602240_VOLTAGE_STEP;
			voltage = 0xff - voltage + 1;
		} else
			voltage = (pdata->voltage - QT602240_VOLTAGE_DEFAULT) /
				QT602240_VOLTAGE_STEP;

		qt602240_write_object(data, QT602240_SPT_CTECONFIG, QT602240_CTE_VOLTAGE, voltage);
	}
#endif
}

/* ������ Calibration�� �����ϴ� �Լ� */
uint8_t calibrate_chip(struct qt602240_data *data)
{
	int ret = 0;
	struct i2c_client *client = data->client;
  
	/* resume calibration must be performed with zero settings */
	acquisition_config.atchcalst = 0;
	acquisition_config.atchcalsthr = 0; 

	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev, "\n[%s] \n", __func__);
		dev_info(&client->dev, "reset acq atchcalst=%d, atchcalsthr=%d\n", acquisition_config.atchcalst, acquisition_config.atchcalsthr );
	}
	
	/* Write temporary acquisition config to chip. */
	if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
	{
		/* "Acquisition config write failed!\n" */
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev,"\n[ERROR] line : %d\n", __LINE__);
		}
		ret = -1; /* calling function should retry calibration call */
	}

	/* restore settings to the local structure so that when we confirm the 
	* cal is good we can correct them in the chip */
	/* this must be done before returning */
	acquisition_config.atchcalst = ATCHCALST_VAL;
	acquisition_config.atchcalsthr = ATCHCALSTHR_VAL;

	/* send calibration command to the chip */
	if(ret == 0)
	{
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev, "\n[%s] \n", __func__);
					dev_info(&client->dev,"Doing calibration command writed.\n");
				}
						
        /* change calibration suspend settings to zero until calibration confirmed good */
        	ret = qt602240_write_object(data, QT602240_GEN_COMMAND, QT602240_COMMAND_CALIBRATE, 1);
					//msleep(120);
        
	        /* set flag for calibration lockup recovery if cal command was successful */
	        if(ret == 0)
	        { 
		        /* set flag to show we must still confirm if calibration was good or bad */
		        cal_check_flag = 1;
	        }
	}
	recal_comp_flag = 1;
	return ret;
}

/* Resume�� Calibration ���и� ȸ�� �ϱ����� ����  */
static void calibrate_chip_wq(struct work_struct * p)
{
	int ret = 0;
	int delay;
	struct qt602240_data *data = container_of(p, struct qt602240_data, ts_event_cal_work.work);
	struct i2c_client *client = data->client;

	/* resume calibration must be performed with zero settings */
	acquisition_config.atchcalst = 0;
	acquisition_config.atchcalsthr = 0; 

	if (good_check_flag == 0 ) {
		if (doing_cal_flag == 0) {
			if (debug_point_level & DEBUG_LOW_CAL) { 
				dev_info(&client->dev, "\n[%s] \n", __func__);
				dev_info(&client->dev, "Starting Delayed Calibration...\n");
			}
			
			if (debug_point_level & DEBUG_LOW_CAL) { 
				dev_info(&client->dev, "\n[%s] \n", __func__);
				dev_info(&client->dev, "reset acq atchcalst=%d, atchcalsthr=%d\n", acquisition_config.atchcalst, acquisition_config.atchcalsthr );
			}
			
			/* Write temporary acquisition config to chip. */
			if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
			{
				/* "Acquisition config write failed!\n" */
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev,"\n[ERROR] line : %d\n", __LINE__);
				}
				ret = -1; /* calling function should retry calibration call */
			}
			
			/* restore settings to the local structure so that when we confirm the 
			* cal is good we can correct them in the chip */
			/* this must be done before returning */
			acquisition_config.atchcalst = ATCHCALST_VAL;
			acquisition_config.atchcalsthr = ATCHCALSTHR_VAL;

			/* send calibration command to the chip */
			if(ret == 0) // && doing_cal_flag == 0)
			{
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev, "\n[%s] \n", __func__);
					dev_info(&client->dev,"Doing delayed calibration command writed.\n");
				}
				/* change calibration suspend settings to zero until calibration confirmed good */
				ret = qt602240_write_object(data, QT602240_GEN_COMMAND, QT602240_COMMAND_CALIBRATE, 1);
				//msleep(120);
  		
				/* set flag for calibration lockup recovery if cal command was successful */
				if(ret == 0)
				{ 
					/* set flag to show we must still confirm if calibration was good or bad */
					cal_check_flag = 1;
				}
			}
		}
		else {
			if (debug_point_level & DEBUG_LOW_CAL) { 
				dev_info(&client->dev, "\n[%s] \n", __func__);
				dev_info(&client->dev, "Doing delayed calibration command skipped.\n");	
			}
						
			if(doing_cal_skip_cnt > 1) {
				doing_cal_flag = 0;
				doing_cal_skip_cnt = 0;			
			}
			
			doing_cal_skip_cnt++;			
		}
		
		delay = CAL_DELAY_MSEC;
		queue_delayed_work(qt602240_cal_wq, &data->ts_event_cal_work, round_jiffies_relative(msecs_to_jiffies(delay)));
	}
	else {
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev, "\n[%s] \n", __func__);
			dev_info(&client->dev, "Calibration has good. skipping delayed calibration.\n");		
		}
	}
}

/* Disable Auto-Calibration */
static void autocal_wq(struct work_struct * p)
{
	struct qt602240_data *data = container_of(p, struct qt602240_data, ts_event_autocal_work.work);
	struct i2c_client *client = data->client;

	/* resume calibration must be performed with zero settings */
	acquisition_config.tchautocal = 0;

	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev, "\n[%s] \n", __func__);
		dev_info(&client->dev, "Autocalibration Disabled.(%d)\n", acquisition_config.tchautocal);
	}

	/* Write temporary acquisition config to chip. */
	if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
	{
		/* "Acquisition config write failed!\n" */
		if (debug_point_level & DEBUG_LOW_CAL) { 
				dev_info(&client->dev,"\n[ERROR] line : %d\n", __LINE__);
		}
	}
}

/* 16ms Interrupt Mode Enable */
static void disable_freerun_wq(struct work_struct * p)
{
	struct qt602240_data *data = container_of(p, struct qt602240_data, disable_freerun_work.work);
	struct i2c_client *client = data->client;

	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev, "\n[%s] \n", __func__);
		dev_info(&client->dev, "Disable Freerun Mode. Interrupt time as %d msec.\n", ACTVACQINT_VAL);
	}
		
	qt602240_write_object(data, QT602240_GEN_POWER, QT602240_POWER_ACTVACQINT, ACTVACQINT_VAL);
}

#ifdef ENABLE_AUTOCAL
/* Enable Auto-Calibration */
static void autocal_enable(struct qt602240_data *data)
{
	struct i2c_client *client = data->client;

	/* resume calibration must be performed with zero settings */
	acquisition_config.tchautocal = 10; //5;

	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev, "\n[%s] \n", __func__);
		dev_info(&client->dev, "Autocalibration Enabled.(%d)\n", acquisition_config.tchautocal);
	}

	/* Write temporary acquisition config to chip. */
	if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
	{
		/* "Acquisition config write failed!\n" */
		if (debug_point_level & DEBUG_LOW_CAL) { 
				dev_info(&client->dev,"\n[ERROR] line : %d\n", __LINE__);
		}
	}	
}
#endif

static void tsp_100ms_timer_wq(struct work_struct * p)
{
	struct qt602240_data *data = container_of(p, struct qt602240_data, ts_100ms_timer_work.work);
	struct i2c_client *client = data->client;
	int delay;

	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev, "\n[%s] \n", __func__);
		dev_info(&client->dev, "Starting 100ms Timer : %d ticks, Timer Flag : %d \n", qt_timer_ticks, qt_timer_flag);
	}
	
	qt_timer_ticks++;

	//delay = 100;
	//delay = 10;
	delay = 5;
	
	if ((qt_timer_flag == 1) && (qt_timer_ticks < 10)) {	
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev, "\n[%s] \n", __func__);
			dev_info(&client->dev, "Queue Timer\n");
		}
		//queue_delayed_work(qt602240_100ms_timer_wq, &data->ts_100ms_timer_work, round_jiffies_relative(msecs_to_jiffies(100)));
		schedule_delayed_work(&data->ts_100ms_timer_work, round_jiffies_relative(msecs_to_jiffies(delay)));
	}
	else {
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev, "\n[%s] \n", __func__);
			dev_info(&client->dev, "Cancel Timer\n");
		}
		
		if (delayed_work_pending(&data->ts_100ms_timer_work))
			cancel_delayed_work_sync(&data->ts_100ms_timer_work);	
		
		qt_timer_ticks = 0;
		
		/* Over Timer Count */
		if(qt_timer_ticks >= 10)
			calibrate_chip(data);
	}
}

void force_release_all_keys(struct qt602240_data *data)
{
	
		struct input_dev *input_dev = data->input_dev;
		struct qt602240_finger  *finger = data->finger;
		unsigned int i;
			
		for(i=0; i < QT602240_MAX_FINGER; i++) {
			finger[i].status = 0;
			finger[i].pressure = -1;

#if 0
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, finger[i].pressure);
			input_report_abs(input_dev, ABS_MT_POSITION_X, finger[i].x_pos);
			input_report_abs(input_dev, ABS_MT_POSITION_Y,	finger[i].y_pos);

			/* Touch Position Data Report */
			input_mt_sync(input_dev);			
#endif
		}
		
		/* Ű ������ Ű Release ���� ���� */
		input_report_key(input_dev, KEY_BACK, 0);
		input_report_key(input_dev, KEY_HOME, 0);
		input_report_key(input_dev, KEY_MENU, 0);
		input_report_key(input_dev, BTN_TOUCH, 0);	
		input_sync(input_dev);	
}

/* Calibration�� ����� �Ǿ������� �˻� */
void check_chip_calibration(struct qt602240_data *data)
{
	struct qt602240_object *object;
	struct i2c_client *client = data->client;
	struct qt602240_finger  *finger = data->finger;
	unsigned int delay;
	uint8_t data_buffer[100] = { 0 };
	uint8_t try_ctr = 0;
	uint8_t data_byte = 0xF3; /* dianostic command to get touch flags */
	uint16_t diag_address;
	uint8_t tch_ch = 0, atch_ch = 0;
	uint8_t check_mask;
	uint8_t i;
	uint8_t j;
	uint8_t x_line_limit;

	/* Wq Calibration Ȱ��ȭ */	
	doing_cal_flag = 1;
		
	/* we have had the first touchscreen or face suppression message 
	 * after a calibration - check the sensor state and try to confirm if
	 * cal was good or bad */

	/* get touch flags from the chip using the diagnostic object */
	/* write command to command processor to get touch flags - 0xF3 Command required to do this */
	qt602240_write_object(data, QT602240_GEN_COMMAND, QT602240_COMMAND_DIAGNOSTIC, data_byte);
	/* get the address of the diagnostic object so we can get the data we need */
	object = qt602240_get_object(data, QT602240_DEBUG_DIAGNOSTIC);
	diag_address = object->start_address;

	msleep(20); 

	/* ���� Ʃ�� ������ ���� */
	if(qt602240_get_chg_state == QT602240_PWR_STATE_VBATT) {
		qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHTHR, TCHTHR_VAL_CAL_VBATT);
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev,"TCHTHR Changed for Check Calibration : (%d).\n", TCHTHR_VAL_CAL_VBATT);
		}
	}
	else if(qt602240_get_chg_state == QT602240_PWR_STATE_AC_USB) {
		qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHTHR, TCHTHR_VAL_CAL_ACUSB);
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev,"TCHTHR Changed for Check Calibration : (%d).\n", TCHTHR_VAL_CAL_ACUSB);
		}
	}

	/* read touch flags from the diagnostic object - clear buffer so the while loop can run first time */
	memset( data_buffer , 0xFF, sizeof( data_buffer ) );

	/* wait for diagnostic object to update */
	while(!((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00)))
	{
		/* wait for data to be valid  */
		if(try_ctr > 3)
		{
			/* Failed! */
			dev_err(&client->dev,"Diagnostic Data did not update!\n");
			break;
		}
		msleep(2);
		try_ctr++; /* timeout counter */
		__qt602240_read_reg(client, diag_address, 2, data_buffer);
		
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev,"Waiting for diagnostic data to update, try %d\n", try_ctr);
		}
	}

	/* data array is 20 x 16 bits for each set of flags, 2 byte header, 40 bytes for touch flags 40 bytes for antitouch flags*/

	/* count up the channels/bits if we recived the data properly */
	if((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00))
	{
		/* data is ready - read the detection flags */
		__qt602240_read_reg(client, diag_address, 82, data_buffer);

		/* mode 0 : 16 x line, mode 1 : 17 etc etc upto mode 4.*/
		x_line_limit = 16 + cte_config.mode;
		if(x_line_limit > 20)
		{
			/* hard limit at 20 so we don't over-index the array */
			x_line_limit = 20;
		}

		/* double the limit as the array is in bytes not words */
		x_line_limit = x_line_limit << 1;

		/* count the channels and print the flags to the log */
		for(i = 0; i < x_line_limit; i+=2) /* check X lines - data is in words so increment 2 at a time */
		{
			/* print the flags to the log - only really needed for debugging */
			//printk("[TSP] Detect Flags X%d, %x%x, %x%x \n", i>>1,data_buffer[3+i],data_buffer[2+i],data_buffer[43+i],data_buffer[42+i]);

			/* count how many bits set for this row */
			for(j = 0; j < 8; j++)
			{
				/* create a bit mask to check against */
				check_mask = 1 << j;

				/* check detect flags */
				if(data_buffer[2+i] & check_mask)
				{
					tch_ch++;
				}
				if(data_buffer[3+i] & check_mask)
				{
					tch_ch++;
				}

				/* check anti-detect flags */
				if(data_buffer[42+i] & check_mask)
				{
					atch_ch++;
				}
				if(data_buffer[43+i] & check_mask)
				{
					atch_ch++;
				}
			}
		}
		/* print how many channels we counted */
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev,"Flags Counted channels: t:%d a:%d \n", tch_ch, atch_ch);
		}
		
		/* send page up command so we can detect when data updates next time,
		 * page byte will sit at 1 until we next send F3 command */
		data_byte = 0x01;
		qt602240_write_object(data, QT602240_GEN_COMMAND, QT602240_COMMAND_DIAGNOSTIC, data_byte);

		if(recal_comp_flag) {
			if ((atch_ch > 0) || ((tch_ch == 0) && (atch_ch == 0))) {
					if (debug_point_level & DEBUG_LOW_CAL) { 
						dev_info(&client->dev,"Re-Calibration wad bad. Trying Re-Calibration.");
					}
				calibrate_chip(data);
			}
			else 
				recal_comp_flag = 0;
		}
		else {
			/* Case.0 : tch_ch�� 10���� �۰�, 0���� ũ��, atch_ch�� 0�� ���� Good Touch�� �Ǵ��Ѵ�.  */      
			if((tch_ch > 0) && (atch_ch == 0) && (tch_ch <= 15) && (finger_num == 1)) {
				/* Ÿ�̸Ӱ� Ȱ��ȭ �Ǿ��ִ��� Ȯ���Ѵ�.  */
				if(qt_timer_flag == 1) {
					/* Good Cal Point�� 0.1 Sec �̻� ���� ���������� ���;� Cal Check�� �Ϸ� �ȴ�. */
					if((qt_timer_ticks > 1)) {
						dev_info(&client->dev,"TS Calibration may be good. (%d)\n", good_check_flag);
						
						/* Cal ���� �� Flags �ʱ�ȭ */
						cal_check_flag = 0;
						good_check_flag = 1; 
						qt_timer_ticks = 0;
						qt_timer_flag = 0;
						touch_faceup_flag = 0;
						doing_cal_skip_cnt = 0;
						
						/* Cal ���� �� ��ġ ���� Release�� �� Ȱ��ȭ, �ʱ�ȭ�� ���� */
						for(i=0; i < QT602240_MAX_FINGER; i++) {
							finger[i].status = 0;
							finger[i].pressure = -1;
						}
						
						/* Cancel Timer */
						if (delayed_work_pending(&data->ts_100ms_timer_work))
							cancel_delayed_work_sync(&data->ts_100ms_timer_work);	
							
						/* Cancel Cal-Cmd Wq */
						if (delayed_work_pending(&data->ts_event_cal_work))
							cancel_delayed_work_sync(&data->ts_event_cal_work);
							
						/* ��ġ ���� ���� */
						if(qt602240_get_chg_state == QT602240_PWR_STATE_VBATT) {
							qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHTHR, TCHTHR_VAL_VBATT);
							if (debug_point_level & DEBUG_LOW_CAL) { 
								dev_info(&client->dev,"TCHTHR Changed for Check Calibration Completed: (%d).\n", TCHTHR_VAL_VBATT);
							}
						}
						else if(qt602240_get_chg_state == QT602240_PWR_STATE_AC_USB) {
							qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHTHR, TCHTHR_VAL_ACUSB);
							if (debug_point_level & DEBUG_LOW_CAL) { 
								dev_info(&client->dev,"TCHTHR Changed for Check Calibration Completed: (%d).\n", TCHTHR_VAL_ACUSB);
							}
						}
				
						/* Anti-Touch Calibration �� ���� */
						acquisition_config.atchcalst = ATCHCALST_VAL;
						acquisition_config.atchcalsthr = ATCHCALSTHR_VAL;
						
						if (debug_point_level & DEBUG_LOW_CAL) { 
							dev_info(&client->dev,"reset acq atchcalst=%d, atchcalsthr=%d\n", acquisition_config.atchcalst, acquisition_config.atchcalsthr );
						}
						/* Write normal acquisition config back to the chip. */
						if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
						{
							/* "Acquisition config write failed!\n" */
							dev_err(&client->dev, "\n[ERROR] Acquisition config write failed!, line : %d\n", __LINE__);
							// MUST be fixed
						}
						
						/* Free-Run Mode Disable */
						delay = 3000; // 3 Sec
						schedule_delayed_work(&data->disable_freerun_work, round_jiffies_relative(msecs_to_jiffies(delay)));
						
					}
					else {
						/* Good Check Flag ���� : ������ */
						good_check_flag++;
						
						/* ���� Time Ticks�� Good Check Flags�� ������ ǥ���Ѵ�.*/
						if (debug_point_level & DEBUG_LOW_CAL) { 
							dev_info(&client->dev,"TS Calibration may be good. Current Time Ticks : (%d), Good Check Flags : (%d).", qt_timer_ticks, good_check_flag);
						}
					}
				}
				else {
					/* Timer�� Disable �Ǿ��ְ�, Good Touch�� �߻��Ͽ��� ���, Timer�� �ʱ�ȭ �ϰ� ���� ��ƾ�� Ȱ��ȭ �Ѵ�. */
					if (debug_point_level & DEBUG_LOW_CAL) { 
						dev_info(&client->dev,"TS Calibration may be good. Check 0.5 Sec doing Good calibrations.");
					}
					/* Ÿ�̸� Ȱ��ȭ  */
					qt_timer_flag = 1;
					/* Timer Ticks �ʱ�ȭ */
					qt_timer_ticks = 0;
					/* Enable Good Check Flag : ��ġ�� Good Cal���� Ȱ��ȭ �ϰ�, Good Cal ������ Ȯ���Ѵ�. */
					good_check_flag = 0;
					/* Cal Ȯ���� �ϴ� ���� ���������� üũ�ؾ� �Ѵ�. */
					cal_check_flag = 1;
										
					delay = 0; //10;
					schedule_delayed_work(&data->ts_100ms_timer_work, round_jiffies_relative(msecs_to_jiffies(delay)));
				}
			}
			/* Case 1. atch_ch�� �ִ� ��� ������ ���� ��  */
			else if(atch_ch >= 8 && (((tch_ch+10) <= atch_ch) || ((tch_ch+atch_ch) > 30))) {
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev,"Calibration was bad. (atch_ch >= 8)\n");
				}
				/* cal was bad - must recalibrate and check afterwards */
				cal_check_flag=0;
				calibrate_chip(data);
				qt_timer_flag=0;
			}
			else if(finger_num > 2) {
				if (debug_point_level & DEBUG_LOW_CAL) {
					dev_info(&client->dev,"Calibration was bad. (more two finger) : %d\n", finger_num);
				}
				/* cal was bad - must recalibrate and check afterwards */
				cal_check_flag=0;
				calibrate_chip(data);
				qt_timer_flag=0;
			}
			/* Case 5. 0.5sec �̻� �������� Touch Point�� ���;� Cal�� �Ϸ� �Ѵ�.������ ���� � ������ �� ������ �� */
			else {
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev,"Calibration was not decided yet\n");
				}
				/* we cannot confirm if good or bad - we must wait for next touch  message to confirm */
				cal_check_flag = 1;
				qt_timer_flag = 0;
			}
		}
	}
	/* Wq Calibration ��Ȱ��ȭ */
	doing_cal_flag = 0;
}

/* x��ǥ�� �������� Buttons�� ������ �ľ��Ѵ�. */ 
int qt602240_btn_check(int x)
{
	int key = 0;
	
	/* check MENU key */
	if(QT602240_BTN_MENU_CENTER-QT602240_BTN_MENU_WITDTH/2 <= x && x <= QT602240_BTN_MENU_CENTER+QT602240_BTN_MENU_WITDTH/2)
	{
		key = KEY_MENU;
	}  
	/* check HOME key */
	else if(QT602240_BTN_HOME_CENTER-QT602240_BTN_HOME_WITDTH/2 <= x && x <= QT602240_BTN_HOME_CENTER+QT602240_BTN_HOME_WITDTH/2)
	{
		key = KEY_HOME;
	}
	/* check BACK key */    
	else if(QT602240_BTN_BACK_CENTER-QT602240_BTN_BACK_WITDTH/2 <= x && x <= QT602240_BTN_BACK_CENTER+QT602240_BTN_BACK_WITDTH/2)
	{
		key = KEY_BACK;
	}
	
	return key;
}

/* ��ġ ��ũ�� Ȱ��ȭ */
static void qt602240_start(struct qt602240_data *data)
{
	/* Touch enable */
	if (init_ts_reg > 0) {
		qt602240_write_object(data,
			QT602240_TOUCH_MULTI, QT602240_TOUCH_CTRL, touchscreen_config.ctrl);
		}
}

/* ��ġ ��ũ�� ��Ȱ��ȭ : Goto Deep-Sleep Mode */
static void qt602240_stop(struct qt602240_data *data)
{
	/* Touch disable */
	if (init_ts_reg > 0) {
		qt602240_write_object(data,
			QT602240_TOUCH_MULTI, QT602240_TOUCH_CTRL, 0);
	}
}

/* ��ġ Report Enable */
static void qt602240_report_enable(struct qt602240_data *data)
{	
	struct i2c_client *client = data->client;
		
	/* Anti-Touch Calibration �� ���� */
	acquisition_config.atchcalst = ATCHCALST_VAL;
	acquisition_config.atchcalsthr = ATCHCALSTHR_VAL;
	
	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev,"reset acq atchcalst=%d, atchcalsthr=%d\n", acquisition_config.atchcalst, acquisition_config.atchcalsthr );
	}
	/* Write normal acquisition config back to the chip. */
	if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
	{
		/* "Acquisition config write failed!\n" */
		dev_err(&client->dev, "\n[ERROR] Acquisition config write failed!, line : %d\n", __LINE__);
		// MUST be fixed
	}
	
	/* Touch enable */
	if (init_ts_reg > 0) {
		qt602240_write_object(data,
			QT602240_TOUCH_MULTI, QT602240_TOUCH_CTRL, touchscreen_config.ctrl);
		}
}

/* ��ġ Report Disable */
static void qt602240_report_disable(struct qt602240_data *data)
{
	struct i2c_client *client = data->client;
	
	/* Anti-Touch Calibration �� ��Ȱ�� */
	acquisition_config.atchcalst = 0;
	acquisition_config.atchcalsthr = 0;
	
	if (debug_point_level & DEBUG_LOW_CAL) { 
		dev_info(&client->dev,"reset acq atchcalst=%d, atchcalsthr=%d\n", acquisition_config.atchcalst, acquisition_config.atchcalsthr );
	}
	/* Write normal acquisition config back to the chip. */
	if (qt602240_reg_write(data, QT602240_GEN_ACQUIRE, (void *) &acquisition_config) != 0)
	{
		/* "Acquisition config write failed!\n" */
		dev_err(&client->dev, "\n[ERROR] Acquisition config write failed!, line : %d\n", __LINE__);
		// MUST be fixed
	}
	
	/* Touch disable */
	if (init_ts_reg > 0) {
		qt602240_write_object(data,
			QT602240_TOUCH_MULTI, QT602240_TOUCH_CTRL, 0x1);
	}
}

/* Charging State ���濡 ���� Configuration�� �������� �����ϴ� �Լ�  */
void check_charging_state(struct qt602240_data *data) {
	struct i2c_client *client = data->client;
	
	if (qt602240_get_chg_state_tmp != qt602240_get_chg_state) {
		if(qt602240_get_chg_state == QT602240_PWR_STATE_VBATT) {
			// T9 Register
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHTHR, TCHTHR_VAL_VBATT);
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHDI, TCHDI_VAL_VBATT);
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_MOVHYSTI, MOVHYSTI_VAL_VBATT);
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_MOVFILTER, MOVFILTER_VAL_VBATT);
			// T22 Register
			qt602240_write_object(data, QT602240_PROCG_NOISE, QT602240_NOISE_NOISETHR, NOISETHR_VAL_VBATT);			
		}
		else if (qt602240_get_chg_state == QT602240_PWR_STATE_AC_USB) {
			// T9 Register
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHTHR, TCHTHR_VAL_ACUSB);
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_TCHDI, TCHDI_VAL_ACUSB);
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_MOVHYSTI, MOVHYSTI_VAL_ACUSB);
			qt602240_write_object(data, QT602240_TOUCH_MULTI, QT602240_TOUCH_MOVFILTER, MOVFILTER_VAL_ACUSB);
			// T22 Register
			qt602240_write_object(data, QT602240_PROCG_NOISE, QT602240_NOISE_NOISETHR, NOISETHR_VAL_ACUSB);
		}
		
		qt602240_get_chg_state_tmp = qt602240_get_chg_state;
		
		dev_info(&client->dev,"Charging State Change Completed(%d).\n", qt602240_get_chg_state_tmp);
	}
	else {
		/* TODO : */
	}
}

/* Worker Thread ������� ���ͷ�Ʈ �߻� �� ��ǥ�� ����Ʈ �Ѵ�. */
#ifdef TS_USE_WORK_QUEUE
static void qt602240_input_read(struct work_struct * p)
#else
static void qt602240_input_read(struct qt602240_data *data)
#endif
{
#ifdef TS_USE_WORK_QUEUE
	struct qt602240_data *data = container_of(p, struct qt602240_data, ts_event_work);
#endif
	struct qt602240_finger  *finger = data->finger;
	struct qt602240_object *object;
	struct qt602240_message message;
	struct i2c_client *client = data->client;
	struct input_dev *input_dev = data->input_dev;
	struct device *dev = &data->client->dev;
	
	int i;
	int id, x_pos, y_pos, pressure, status, delay;
	static int key = 0;
	u8 reportid, max_reportid, min_reportid;
	
	/* Charging state�� Ȯ���ϰ�, AC���� Battery���� �����Ѵ�.*/
	if (init_ts_reg > 0) {
		check_charging_state(data);
	}

	/* ��ǥ �����͸� I2C�� ���� �о�´�. */
	if(qt602240_read_message(data, &message)) {
		dev_err(&client->dev, "Failed to read message\n");
	};

	reportid = message.reportid;
	
	/* Touch Message Flag �ʱ�ȭ */
	touch_message_flag = 0;

	if (debug_point_level & DEBUG_LOW_MESSAGE) 
		qt602240_dump_message(dev, &message);

	/* whether reportid is thing of QT602240_TOUCH_MULTI */
	object = qt602240_get_object(data, QT602240_TOUCH_MULTI);
	if (!object) {
			dev_err(&client->dev, "Failed to get Multi-Touch Object Table Information.\n");	
	}
	
	/* Finger Information �� Data�� ����ϴ� �κ� */
	max_reportid = object->max_reportid;
	min_reportid = max_reportid - object->num_report_ids + 1;
	id = reportid - min_reportid;
	
	if (debug_point_level & DEBUG_REPORT_ID) {
		dev_info(&client->dev,"Touch Report ID : Max - %d, Min - %d, Current ID - %d\n", max_reportid, min_reportid, id);
	}
		
	if((reportid >= min_reportid && reportid <= max_reportid)) {
		status = message.message[0];
		x_pos = (message.message[1] << 2) | ((message.message[3] & ~0x3f) >> 6);
		y_pos = (message.message[2] << 2) | ((message.message[3] & ~0xf3) >> 2);
		pressure = message.message[4];

		if (debug_point_level & DEBUG_LOW_POSITION) {
			dev_info(&client->dev,"Touch Pos : X - %d, Y- %d, PRESSURE - %d, Status - %x, Report ID - %d\n", x_pos, y_pos, pressure, status, id);
		}
	
		/* Face-up detect �� Ungrip�� ���� ó�� �ڵ鷯 �߰� */
		if ((status & 0x3) > 0) {
			if (debug_point_level & DEBUG_LOW_CAL) {
				dev_info(&client->dev, "Touch Grip,Face-up Detected! : %d\n",status);
			}
		}
		
		/* ����� ��ǥ���� �ϴ� Key ��ư�� �����ϸ�, �ƴϸ� ��ġ ��ǥ�� �����Ѵ�. */
		if((QT602240_BTN_AREA_Y_BOTTOM > y_pos) && (qt602240_btn_area_y_top < y_pos) && id == 0) {
			if((status & (QT602240_RELEASE)) > 0) {
				input_report_key(input_dev, key, 0);
				input_sync(input_dev);
				key = 0;
			}
			else {
				/* Touch Message Flag �ʱ�ȭ */
				touch_message_flag = 1;
				
				/* X ��ǥ�� �������� MENU, HOME, BACK Ű�� �����Ѵ�.  */
				key = qt602240_btn_check(x_pos);

				if (key > 0) {
					/* ��ġ Ű ��ǥ �������� Press �̺�Ʈ�� �߻� ���� ��� ó�� */
					input_report_key(input_dev, key, 1);
					/* ��Ű Ű�� ������ ��� ��ġ ��ũ���� ��ġ �̺�Ʈ�� ó�� */
					input_report_key(input_dev, BTN_TOUCH, 0);	
					input_sync(input_dev);
				}
				else {
					input_report_key(input_dev, KEY_BACK, 0);
					input_report_key(input_dev, KEY_HOME, 0);
					input_report_key(input_dev, KEY_MENU, 0);
					input_report_key(input_dev, BTN_TOUCH, 0);	
					input_sync(input_dev);
					key = 0;
				}
			}
			/* ��Ƽ��ġ ������ ���� �ʱ�ȭ */
			for(i=0; i < QT602240_MAX_FINGER; i++) {
				finger[i].status = 0;
				finger[i].pressure = -1;						
			}	
		}
		/* ��ġ ������ ���� ��ǥ �ڵ鷯 */
		else if(((QT602240_MAX_XC >= x_pos) && (0 <= x_pos)) && ((QT602240_MAX_YC >= y_pos) && (0 <= y_pos))) {		
			/* ��ġ�� ����ϱ� ���� ��ǥ �����͸� �߰� */
			finger[id].status = status;
			finger[id].x_pos = x_pos;
			finger[id].y_pos = y_pos;
			finger[id].pressure = pressure;

			/* Finger�� ����ŭ ������ ���鼭 ��ġ �������� ����Ʈ �Ѵ�. */
			for(i=0, finger_num=0;i < QT602240_MAX_FINGER; i++) {				
				if (((finger[i].status & (QT602240_PRESS | QT602240_MOVE | QT602240_AMP)) > 0) 
					&& ((finger[i].status & (QT602240_RELEASE)) == 0)) {
						
					/* Touch Message Flag �ʱ�ȭ */
					touch_message_flag = 1;
					
					input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, finger[i].pressure);
					input_report_abs(input_dev, ABS_MT_POSITION_X, finger[i].x_pos);
					input_report_abs(input_dev, ABS_MT_POSITION_Y,	finger[i].y_pos);

					/* Touch Position Data Report */
					input_mt_sync(input_dev);						
					
					if (debug_point_level & DEBUG_ABS_POSITION) {
						dev_info(&client->dev,"Touch Report ABS Pos : X - %d, Y- %d, PRESSURE - %d, Report ID - %d\n", 
							finger[i].x_pos, finger[i].y_pos, finger[i].pressure, i);
					}							
					/* Status�� Release ���� �ʾ��� ��� finger�� ���� �����Ѵ�. */
					finger_num++;			
				}
				/* Release �� ��Ÿ Status�� ��� ����ü ������ �ʱ�ȭ �Ѵ�. */
				else {
					finger[i].status = 0;
					finger[i].pressure = -1;
				}
			}
			/* ��ġ�� Finger�� ���� ���� BTN_TOUCH�� ����Ʈ �Ѵ�. */
			if(finger_num > 0 ) 
				input_report_key(input_dev, BTN_TOUCH, finger_num > 0);	
			else
				input_report_key(input_dev, BTN_TOUCH, 0);	
				
			/* ���� �ܰ��� BTN_TOUCH Ű�� ���� ��ũ Function */
			input_sync(input_dev);
		}
		/* ��ġ ������ Ű ���� ������ ��ǥ ó���� ���� Handler */
		else {
			/* ��ġ ������ �����Ͱ� ��ǥ�� ����� ���  ������ ���� �ʱ�ȭ */
			finger[id].status = 0;
			finger[id].pressure = -1;
			key = 0;
			
			/* Ű ������ Ű Release ���� ���� */
			input_report_key(input_dev, KEY_BACK, 0);
			input_report_key(input_dev, KEY_HOME, 0);
			input_report_key(input_dev, KEY_MENU, 0);
			input_report_key(input_dev, BTN_TOUCH, 0);	
			input_sync(input_dev);
		}
	}
	/* Report ID 1 �� ��Ÿ ������ ���� OverFlow Handler */
	else {
		/* Report ID 1�� Status�� 0x40�� ������ ��� Overflow�� ���� ó���� �����Ѵ�. */
		if(reportid == 1) {
			if((message.message[0] == 0x40)) {
				/* Calibration�� ����� */
				/* Overflow Error �߻��� �� 0.5�� �Ŀ� ������ Recalibration�� �����Ѵ�. */
				if(good_check_flag == 0) {
					msleep(50);
					good_check_flag = 0;
					doing_cal_flag = 0;
					cal_check_flag = 0;
					doing_cal_skip_cnt = 0;
					delay = CAL_OFL_DELAY_MSEC;
					if (!delayed_work_pending(&data->ts_event_cal_work))
						queue_delayed_work(qt602240_cal_wq, &data->ts_event_cal_work, round_jiffies_relative(msecs_to_jiffies(delay)));
				}				
				if (debug_point_level & DEBUG_LOW_CAL) {
					dev_info(&client->dev, "Overflow Error (ID : %d, Status : %x)\n",
						reportid, message.message[0]);
				}
				/* Finger�� ����ŭ ������ ���鼭 ��ġ �������� �ʱ�ȭ �Ѵ�. */
				for(i=0; i < QT602240_MAX_FINGER; i++) {
					finger[i].status = 0;
					finger[i].pressure = -1;
				}
			}
			/* Calibration�� ������ ��� ������ ǥ���Ѵ�. */
			else if((message.message[0] == 0x10)) {
				/* Calibration �˻縦 ����� */
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev, "Re-Calibration Completed. (ID : %d, Status : %x)\n",
						reportid, message.message[0]);
				}
				recal_comp_flag = 1;
#if 0
				/* 11/07/26 Debug : ���� ���۽� Re-Cal�� �߻����� ��� */
				if (good_check_flag == 1) {
					dev_info(&client->dev, "Calibration May be bad. Trying Re-Calibration.\n");
					good_check_flag = 0;
					cal_check_flag = 1;
				}
#endif
			}
		}
		/* Grip/Face Up/Down Detection, Configuration���� Grip/Face-Up report message enable �ʿ� */
		else if (reportid == 14) {
			if(message.message[0] == 0) {
				if (debug_point_level & DEBUG_LOW_CAL) { 
					dev_info(&client->dev, "Face-Up Detection! (ID : %d, Status : %x)\n",
						reportid, message.message[0]);
				}
				if (cal_check_flag == 1) {
					doing_cal_flag = 0;
					good_check_flag = 0;
					delay = CAL_OFL_DELAY_MSEC;
					if (!delayed_work_pending(&data->ts_event_cal_work))
						queue_delayed_work(qt602240_cal_wq, &data->ts_event_cal_work, round_jiffies_relative(msecs_to_jiffies(delay)));
				}
				else if(touch_faceup_flag > 2) {
					delay = CAL_DELAY_MSEC;
					queue_delayed_work(qt602240_cal_wq, &data->ts_event_cal_work, round_jiffies_relative(msecs_to_jiffies(delay)));
				}
				touch_faceup_flag = 0;
			}
			else if(message.message[0] == 1) {
				if (debug_point_level & DEBUG_LOW_CAL) {
					dev_info(&client->dev, "Face-Down Detection! (ID : %d, Status : %x)\n",	reportid, message.message[0]);
				}
				if (cal_check_flag == 1) {
					doing_cal_flag = 0;
					good_check_flag = 0;
					delay = CAL_OFL_DELAY_MSEC;
					if (!delayed_work_pending(&data->ts_event_cal_work))
						queue_delayed_work(qt602240_cal_wq, &data->ts_event_cal_work, round_jiffies_relative(msecs_to_jiffies(delay)));
				}
				else {
					touch_faceup_flag++;
				}	
			}
		}
		/* ����� �� �ִ� Report ID�� ������ ����� ����� ó�� */ 
		else if (reportid > max_reportid) {
			if (debug_point_level & DEBUG_LOW_CAL) {
				dev_dbg(&client->dev, "Unknown Report ID (ID : %d, Status : %x)\n",	reportid, message.message[0]);
			}
			/* Finger�� ����ŭ ������ ���鼭 ��ġ �������� �ʱ�ȭ �Ѵ�. */
			for(i=0; i < QT602240_MAX_FINGER; i++) {
				finger[i].status = 0;
				finger[i].pressure = -1;
			}
		}
		/* ��Ÿ ������ ���� ó���� ��� ��� ���� ������ Sync�� �����Ѵ�. */
		/* ��ġ Ű ��ǥ ������ ����� ���, Key Release ���� */
		if (key > 0)
			input_report_key(input_dev, key, 0);
				
		/* BTN Touch ���� �ʱ�ȭ Report */
		input_report_key(input_dev, BTN_TOUCH, 0);	
			
		/* ���� �ܰ��� BTN_TOUCH Ű�� ���� ��ũ Function */
		input_mt_sync(input_dev);		
		input_sync(input_dev);
	}
	/* Calibration�� ���������� ó�� �Ǿ������� �˻� */
	if (touch_message_flag == 1 && cal_check_flag == 1 && init_ts_reg == 1) {
		check_chip_calibration(data);
	}
}

/* ���ͷ�Ʈ �ڵ鷯. */
static irqreturn_t qt602240_interrupt(int irq, void *dev_id)
{
	/* ���ͷ�Ʈ ���� ó�� �ڵ� */
	
	struct qt602240_data *data = dev_id;

#ifdef TS_USE_WORK_QUEUE
	if ((atomic_read(&tsp_doing_suspend)) == 0) {
		queue_work(qt602240_wq, &data->ts_event_work);
	}
#else	
	qt602240_input_read(data);
#endif

	return IRQ_HANDLED;
}

/* QT602240�� �ʱ�ȭ ������ ���� (F/W Ver 2.0)   */
int qt602240_reg_init(struct qt602240_data *data)
{
	int ret;

	ret = QT602240_Command_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Command config\n");
		
	ret = QT602240_Powr_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Power config\n");
		
	ret = QT602240_Acquisition_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Acqusition config\n");
		
	ret = QT602240_Multitouch_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Multi-touch config\n");

	ret = QT602240_KeyArrary_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the KeyArrary config\n"); 

	ret = QT602240_GPIOPWM_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the GPIO/PWM config\n");
		
	ret = QT602240_Grip_Face_Suppression_Config_Init(data);
		if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Grip Face Supression config\n");
		
	ret = QT602240_Noise_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Noise config\n");

	ret = QT602240_Proximity_Config_Init(data);        
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the Proximity config\n");
		
	ret = QT602240_One_Touch_Gesture_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the 1 Gesture config\n");
	
	ret = QT602240_Selftest_Config_Init(data);
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the selftest config\n");	

	ret = QT602240_CTE_Config_Init(data);    
	if(ret<0)
		dev_err(&data->client->dev,"fail to initialize the CTE config\n");

	return ret;
}

/* QT602240�� �߿��� ������ Ȯ�� 
   ������Ʈ�� ����� ��� ������Ʈ ����Ʈ�� ������ ���� ���� �ʴ� ��츦 ȸ�� */
static int qt602240_check_reg_init(struct qt602240_data *data)
{
	struct qt602240_object *object;
	struct device *dev = &data->client->dev;
	int index = 0;
	int i, j, error;
	u8 version = data->info.version;
	u8 *init_vals;

#ifndef KTTECH_FINAL_BUILD
	dev_info(dev, "%s : firware version = %d\n", __func__, version);
#endif

	/* F/W Ver 2.0�� ��� Typedef ����� �������� ���� (Ʃ���� ���� �ϱ� ���� �뵵) */
	if ( version > QT602240_VER_22 ) {
		error = qt602240_reg_init(data);
		if (error)
			dev_err(&data->client->dev,"Touch Screen Device Initialize failed! (%d)\n", error);
	}
	/* F/W Ver 1.6�� ��� ������ Matrix ��� Config�� ���� */
	else {
		switch (version) {
		case QT602240_VER_20:
			init_vals = (u8 *)init_vals_ver_20;
			break;
		case QT602240_VER_21:
			init_vals = (u8 *)init_vals_ver_21;
			break;
		case QT602240_VER_22:
			init_vals = (u8 *)init_vals_ver_22;
			break;
		default:
			dev_err(dev, "Firmware version %d doesn't support\n", version);
			return -EINVAL;
		}
	
		for (i = 0; i < data->info.object_num; i++) {
			object = data->object_table + i;
	
			if (!qt602240_object_writable(object->type))
				continue;
	
			for (j = 0; j < object->size + 1; j++)
				qt602240_write_object(data, object->type, j,
						init_vals[index + j]);
	
			index += object->size + 1;
		}
	}
	return 0;
}

/* ��ġ��ũ���� �⺻ ������ �����´�. */
static int qt602240_get_info(struct qt602240_data *data)
{
	struct i2c_client *client = data->client;
	struct qt602240_info *info = &data->info;
	int error;
	u8 val;

	error = qt602240_read_reg(client, QT602240_FAMILY_ID, &val);
	if (error)
		return error;
	info->family_id = val;

	error = qt602240_read_reg(client, QT602240_VARIANT_ID, &val);
	if (error)
		return error;
	info->variant_id = val;

	error = qt602240_read_reg(client, QT602240_VERSION, &val);
	if (error)
		return error;
	info->version = val;

	error = qt602240_read_reg(client, QT602240_BUILD, &val);
	if (error)
		return error;
	info->build = val;

	error = qt602240_read_reg(client, QT602240_OBJECT_NUM, &val);
	if (error)
		return error;
	info->object_num = val;

	return 0;
}

/*�ʱ⿡ ������Ʈ ���̺��� ���� ������ �����´�. */
static int qt602240_get_object_table(struct qt602240_data *data)
{
	int error;
	int i;
	u16 reg;
	u8 reportid = 0;
	u8 buf[QT602240_OBJECT_SIZE];

	for (i = 0; i < data->info.object_num; i++) {
		struct qt602240_object *object = data->object_table + i;

		reg = QT602240_OBJECT_START + QT602240_OBJECT_SIZE * i;
		error = qt602240_read_object_table(data->client, reg, buf);
		if (error)
			return error;

		object->type = buf[0];
		object->start_address = (buf[2] << 8) | buf[1];
		object->size = buf[3];
		object->instances = buf[4];
		object->num_report_ids = buf[5];

		if (object->num_report_ids) {
			reportid += object->num_report_ids *
					(object->instances + 1);
			object->max_reportid = reportid;
		}
	}

	return 0;
}

/* QT602240�� ���� �ʱ�ȭ �κ�, ���� probe�� ���Ͽ� �⺻���� ����̹� �ʱ�ȭ�� �����Ѵ�.  */
static int qt602240_initialize(struct qt602240_data *data)
{
	struct i2c_client *client = data->client;
	struct qt602240_info *info = &data->info;
	int error;
	u8 val;
	
	error = qt602240_get_info(data);
	if (error)
		return error;

	/* ������Ʈ ���̺� �޸𸮰� ������ �Ҵ� �Ǿ� �־������� �˻� */
	if(!data->object_table) {
		/* ������Ʈ ���̺��� �����ϱ� ���� �޸� �Ҵ� */
		data->object_table = kcalloc(info->object_num,
						sizeof(struct qt602240_data),
						GFP_KERNEL);

		if (!data->object_table) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}		
	}

	/* Get object table information */
	error = qt602240_get_object_table(data);
	if (error)
		return error;

	/* Check register init values */
	if (init_ts_reg > 0) {
		error = qt602240_check_reg_init(data);
		if (error)
			return error;
	}
	/* Check X/Y matrix size */
	if (init_ts_reg > 0) {
		error = qt602240_check_matrix_size(data);
		if (error)
			return error;
	}

	/* Hardware���� Data�� ���� */
	if (init_ts_reg > 0) {
		qt602240_handle_pdata(data);
	}

	/* Backup to memory */
	qt602240_write_object(data, QT602240_GEN_COMMAND,
			QT602240_COMMAND_BACKUPNV,
			QT602240_BACKUP_VALUE);
	msleep(QT602240_BACKUP_TIME);

	/* Soft reset */
	qt602240_write_object(data, QT602240_GEN_COMMAND,
			QT602240_COMMAND_RESET, 1);
			
	msleep(QT602240_RESET_TIME);
	
	/* Update matrix size at info struct */
	error = qt602240_read_reg(client, QT602240_MATRIX_X_SIZE, &val);
	if (error)
		return error;
	info->matrix_xsize = val;

	error = qt602240_read_reg(client, QT602240_MATRIX_Y_SIZE, &val);
	if (error)
		return error;
	info->matrix_ysize = val;
	
	/* ��ġ��ũ�� �ʱ�ȭ�� �� ��� Object�� Information ���� ǥ�� */

#ifndef KTTECH_FINAL_BUILD
	dev_info(&client->dev,
			"Family ID: %d Variant ID: %d Version: %d Build: %d\n",
			info->family_id, info->variant_id, info->version,
			info->build);

	dev_info(&client->dev,
			"Matrix X Size: %d Matrix Y Size: %d Object Num: %d\n",
			info->matrix_xsize, info->matrix_ysize,
			info->object_num);
#endif
	
	return 0;
}

/* ������Ʈ�� ������ ���� ���� Function */
#ifndef KTTECH_FINAL_BUILD
static ssize_t qt602240_object_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	struct qt602240_object *object;
	int count = 0;
	int i, j;
	int error;
	u8 val;

	if ((atomic_read(&tsp_doing_suspend)) == 0) {
		for (i = 0; i < data->info.object_num; i++) {
			object = data->object_table + i;
  	
			count += sprintf(buf + count,
					"Object Table Element %d(Type %d)\n",
					i + 1, object->type);
  	
			if (!qt602240_object_readable(object->type)) {
				count += sprintf(buf + count, "\n");
				continue;
			}
  	
			for (j = 0; j < object->size + 1; j++) {
				error = qt602240_read_object(data,
							object->type, j, &val);
				if (error)
					return error;
  	
				count += sprintf(buf + count,
						"  Byte %d: 0x%x (%d)\n", j, val, val);
			}
  	
			count += sprintf(buf + count, "\n");
		}
  	
		return count;
	}
	else
		return 0;
}
#endif

/* �߿��� ���׷��̵带 ���� Functions (Internal) */
static int qt602240_load_fw_internal(struct device *dev, const char *fn)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	u8 *fw_data = (u8 *)fw_ver_32_bin;
	unsigned int fw_size = 0;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;

	fw_size = sizeof(fw_ver_32_bin);
	
	dev_info(dev, "Update Start - F/W Size : %d\n", fw_size);
	
	if ((client->addr == QT602240_APP_LOW) || (client->addr == QT602240_APP_HIGH)) {
		/* Change to the bootloader mode */
		qt602240_write_object(data, QT602240_GEN_COMMAND,
			QT602240_COMMAND_RESET, QT602240_BOOT_VALUE);
		msleep(QT602240_RESET_TIME);

	/* Change to slave address of bootloader */
	if (client->addr == QT602240_APP_LOW)
		client->addr = QT602240_BOOT_LOW;
	else
		client->addr = QT602240_BOOT_HIGH;
	}

	ret = qt602240_check_bootloader(client, QT602240_WAITING_BOOTLOAD_CMD);
	if (ret) {
		dev_err(dev, "Check bootloader : WAITING_BOOTLOAD_CMD failed!\n");
		goto out;
	}

	/* Unlock bootloader */
	qt602240_unlock_bootloader(client);
	
	msleep(QT602240_RESET_TIME);

	while (pos < fw_size) {
		ret = qt602240_check_bootloader(client,
						QT602240_WAITING_FRAME_DATA);
		if (ret) {
			dev_err(dev, "Check bootloader : WAITING_FRAME_DATA failed!\n");
			goto out;
		}

		frame_size = ((*(fw_data + pos) << 8) | *(fw_data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		 * included the CRC bytes.
		 */
		frame_size += 2;

		/* Write one frame to device */
		qt602240_fw_write(client, fw_data + pos, frame_size);

		ret = qt602240_check_bootloader(client,
						QT602240_FRAME_CRC_PASS);
		if (ret) {
			dev_err(dev, "Check bootloader : CRC failed!\n");
			goto out;
		}

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw_size);
	}

out:
	/* Change to slave address of application */
	if (client->addr == QT602240_BOOT_LOW)
		client->addr = QT602240_APP_LOW;
	else
		client->addr = QT602240_APP_HIGH;

	return ret;
}

/* �߿��� ���׷��̵带 ���� Functions (SYSFS) */
#ifndef KTTECH_FINAL_BUILD
static int qt602240_load_fw(struct device *dev, const char *fn)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;

	ret = request_firmware(&fw, fn, dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		return ret;
	}

	/* Change to the bootloader mode */
	qt602240_write_object(data, QT602240_GEN_COMMAND,
			QT602240_COMMAND_RESET, QT602240_BOOT_VALUE);
	msleep(QT602240_RESET_TIME);

	/* Change to slave address of bootloader */
	if (client->addr == QT602240_APP_LOW)
		client->addr = QT602240_BOOT_LOW;
	else
		client->addr = QT602240_BOOT_HIGH;

	ret = qt602240_check_bootloader(client, QT602240_WAITING_BOOTLOAD_CMD);
	if (ret)
		goto out;

	/* Unlock bootloader */
	qt602240_unlock_bootloader(client);

	while (pos < fw->size) {
		ret = qt602240_check_bootloader(client,
						QT602240_WAITING_FRAME_DATA);
		if (ret)
			goto out;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		 * included the CRC bytes.
		 */
		frame_size += 2;

		/* Write one frame to device */
		qt602240_fw_write(client, fw->data + pos, frame_size);

		ret = qt602240_check_bootloader(client,
						QT602240_FRAME_CRC_PASS);
		if (ret)
			goto out;

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw->size);
	}

out:
	release_firmware(fw);

	/* Change to slave address of application */
	if (client->addr == QT602240_BOOT_LOW)
		client->addr = QT602240_APP_LOW;
	else
		client->addr = QT602240_APP_HIGH;

	return ret;
}
#endif

/* ���׷��̵带 ���� �߿�� ��Ʈ�ѷ��� ����ϴ� Function (Internal)) */
static int qt602240_update_fw_store_internal(struct device *dev)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	int ret;
	int try_num = 0;

fw_retry:
	disable_irq(data->irq);

	ret = qt602240_load_fw_internal(dev, QT602240_FW_NAME);
	
	if (ret) {
		try_num++;
		dev_info(dev, "The firmware update failed. Try %d times.\n", try_num);
		
		/* �߿��� ������Ʈ�� �������� ��� 3�� ��õ� �� ����. */
		if(try_num < 3)
			goto fw_retry;
		else {
			/* ���������� ������Ʈ�� ��� ���� ���� ��� ������Ʈ ���μ����� �ߴ��Ѵ�. */
			dev_info(dev, "The firmware update failed.\n");
			return ret;
		}
	} else {
		dev_info(dev, "The firmware update succeeded\n");

		/* Wait for reset */
		msleep(QT602240_FWRESET_TIME);

		kfree(data->object_table);
		data->object_table = NULL;

		qt602240_initialize(data);
	}

	enable_irq(data->irq);

	return ret;
}

/* ���׷��̵带 ���� �߿�� ��Ʈ�ѷ��� ����ϴ� Function (SYSFS) */
#ifndef KTTECH_FINAL_BUILD
static ssize_t qt602240_update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	unsigned int version;
	int error;

	if ((atomic_read(&tsp_doing_suspend)) == 0) {
		if (sscanf(buf, "%u", &version) != 1) {
			dev_err(dev, "Invalid values\n");
			return -EINVAL;
		}
  	
		if (data->info.version > QT602240_VER_22) {
			dev_err(dev, "FW update supported starting with version 21\n");
			return -EINVAL;
		}
  	
		disable_irq(data->irq);
  	
		error = qt602240_load_fw(dev, QT602240_FW_NAME);
		if (error) {
			dev_err(dev, "The firmware update failed(%d)\n", error);
			count = error;
		} else {
			dev_dbg(dev, "The firmware update succeeded\n");
  	
			/* Wait for reset */
			msleep(QT602240_FWRESET_TIME);
  	
			kfree(data->object_table);
			data->object_table = NULL;
  	
			qt602240_initialize(data);
		}
  	
		enable_irq(data->irq);
  	
		return count;
	}
	else
		return 0;
}
#endif

/* SYSFS�� �̿��Ͽ� ������Ʈ �������͸� �о���� Function */
#ifndef KTTECH_FINAL_BUILD
static ssize_t qt602240_object_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	struct qt602240_object *object;
	int count = 0;
	int i;

	if ((atomic_read(&tsp_doing_suspend)) == 0) {
		for (i = 0; i < data->info.object_num; i++) {
			object = data->object_table + i;
	
			count += sprintf(buf + count,
					"Object Table Element %d\n", i + 1);
			count += sprintf(buf + count, "  type:\t\t\t0x%x (%d)\n",
					object->type, object->type);
			count += sprintf(buf + count, "  start_address:\t0x%x (%d)\n",
					object->start_address, object->start_address);
			count += sprintf(buf + count, "  size:\t\t\t0x%x (%d)\n",
					object->size, object->size);
			count += sprintf(buf + count, "  instances:\t\t0x%x (%d)\n",
					object->instances, object->instances);
			count += sprintf(buf + count, "  num_report_ids:\t0x%x (%d)\n",
					object->num_report_ids, object->num_report_ids);
			count += sprintf(buf + count, "  max_reportid:\t\t0x%x (%d)\n",
					object->max_reportid, object->max_reportid);
			count += sprintf(buf + count, "\n");
		}	
		return count;
	}
	else
		return 0;
}
#endif

/* SYSFS�� �̿��Ͽ� ������Ʈ �������͸� �����ϴ� Function */
#ifndef KTTECH_FINAL_BUILD
static ssize_t qt602240_object_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qt602240_data *data = dev_get_drvdata(dev);
	struct qt602240_object *object;
	unsigned int type;
	unsigned int offset;
	unsigned int val;
	int ret;
	
	if ((atomic_read(&tsp_doing_suspend)) == 0) {
		if (sscanf(buf, "%u %u %u", &type, &offset, &val) != 3) {
			dev_err(dev, "Invalid values\n");
			return -EINVAL;
		}
	
		object = qt602240_get_object(data, type);
		if (!object || (offset > object->size)) {
			dev_err(dev, "Invalid values\n");
			return -EINVAL;
		}
	
		ret = qt602240_write_object(data, type, offset, val);
		if (ret < 0)
			return ret;
	
		/* Backup to memory */
		qt602240_write_object(data, QT602240_GEN_COMMAND,
				QT602240_COMMAND_BACKUPNV,
				QT602240_BACKUP_VALUE);
		msleep(QT602240_BACKUP_TIME);
	
		/* Soft reset */
		qt602240_write_object(data, QT602240_GEN_COMMAND,
				QT602240_COMMAND_RESET, 1);
				
		msleep(QT602240_RESET_TIME);
	
		return count;
	}
	else
		return 0;
}
#endif

/* ��ġ��ũ�� ����� ���� ��� */
#ifndef KTTECH_FINAL_BUILD
static ssize_t qt602240_debug_point_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0;
	
	count = sprintf(buf, "Trace Point Level : %d\n", debug_point_level);
	
	return count;	
}
#endif

/* ��ġ��ũ�� ����� ���� ���� */
#ifndef KTTECH_FINAL_BUILD
static ssize_t qt602240_debug_point_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int level;
	
	if (sscanf(buf, "%u", &level) != 1) {
		dev_err(dev, "Invalid values\n");
		return -EINVAL;
	}	
	
	switch(level){
		case 0:
			level = 0;
			break;
		case 1:
			level = (debug_point_level | DEBUG_LOW_POSITION);
			break;
		case 2:
			level = (debug_point_level | DEBUG_LOW_MESSAGE);
			break;
		case 3:
			level = (debug_point_level | DEBUG_REPORT_ID);
			break;
		case 4:
			level = (debug_point_level | DEBUG_ABS_POSITION);
			break;
		case 5:
			level = (debug_point_level | DEBUG_LOW_CAL);
			break;
		case 6:
			level = 0xff; /* Print all debug messages. */
			break;			
		default:
			level = 0;
			break;
	}
		
	debug_point_level = level;
	
	return 0;
}
#endif

static ssize_t qt602240_version_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0;
	
	count = sprintf(buf, "%d\n", tsp_fw_version);
	
	return count;	
}

static ssize_t qt602240_tune_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int count = 0;
	
	count = sprintf(buf, "%d\n", tsp_tune_rev);
	
	return count;	
} 

/* SYSFS�� DEVICE ATTR ���� �κ� */

#ifndef KTTECH_FINAL_BUILD
static DEVICE_ATTR(object, 0666, qt602240_object_show, qt602240_object_store);
static DEVICE_ATTR(update_fw, 0666, NULL, qt602240_update_fw_store);
static DEVICE_ATTR(debug_point, 0666, qt602240_debug_point_show, qt602240_debug_point_store);
static DEVICE_ATTR(object_table, 0444, qt602240_object_table_show, NULL);
#endif
static DEVICE_ATTR(version, 0444, qt602240_version_info_show, NULL);
static DEVICE_ATTR(tunerev, 0444, qt602240_tune_info_show, NULL);

static struct attribute *qt602240_attrs[] = {
#ifndef KTTECH_FINAL_BUILD
	&dev_attr_object.attr,
	&dev_attr_update_fw.attr,
	&dev_attr_object_table.attr,
	&dev_attr_debug_point.attr,
#endif
	&dev_attr_version.attr,
	&dev_attr_tunerev.attr,
	/* TODO */
	NULL
};

static const struct attribute_group qt602240_attr_group = {
	.attrs = qt602240_attrs,
};

/* ��ġ ��ũ�� GPIO ���� */
static int qt602240_init_touch_gpio(struct qt602240_data *data)
{
	const struct qt602240_platform_data *pdata = data->pdata;
	struct i2c_client *client = data->client;
	int rc =0;

	if (gpio_request(pdata->reset_gpio, "ts_reset") < 0)
		goto gpio_failed;

	if (gpio_request(pdata->irq_gpio, "ts_int") < 0)
		goto gpio_failed;

	rc = gpio_direction_output(pdata->reset_gpio, 1);
	
	if (rc) { 
		dev_err(&client->dev,"GPIO Configuration failed! (%d)\n", rc);
		return rc;
	}

	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	rc = gpio_direction_input(pdata->irq_gpio);
	
	if (rc) { 
		dev_err(&client->dev,"GPIO Configuration failed! (%d)\n", rc);
		return rc;
	}

	return 0;

gpio_failed:
	dev_err(&client->dev,"GPIO Request failed! (%d)\n", rc);	
	return -EINVAL;
}

static struct regulator *qt602240_reg_lvs3; 	//1.80v PULL-UP
static struct regulator *qt602240_reg_l4; 		//2.70v MAIN Power
static struct regulator *qt602240_reg_mvs0; //2.7v switch

/* QT602240�� ���� ���� �κ� */
static int qt602240_power(int on, struct qt602240_data *data)
{
	const struct qt602240_platform_data *pdata = data->pdata;
	struct i2c_client *client = data->client;

	int rc = 0;

	if(on) {

		if (qt602240_reg_l4)
			return rc;
	
		if (qt602240_reg_lvs3)
			return rc;

		if (qt602240_reg_mvs0)
			return rc;
    
		qt602240_reg_lvs3 = regulator_get(NULL, "8901_lvs3");
		qt602240_reg_l4 = regulator_get(NULL, "8901_l4");
		qt602240_reg_mvs0 =  regulator_get(NULL, "8901_mvs0");

		if (IS_ERR(qt602240_reg_l4) || IS_ERR(qt602240_reg_lvs3)) {
			dev_err(&client->dev,"Regulator Get (L4) failed! (%d)\n", rc);
			return PTR_ERR(qt602240_reg_l4);
		}
		
		/* Set Voltage Level */
		rc = regulator_set_voltage(qt602240_reg_l4, pdata->voltage, pdata->voltage);
		if (rc) { 
			dev_err(&client->dev,"Regulator Set Voltage (L4) failed! (%d)\n", rc);
  			regulator_put(qt602240_reg_l4);
			return rc;
		}

		/* Enable Regulator */
		rc = regulator_enable(qt602240_reg_l4);
		if (rc) { 
			dev_err(&client->dev,"Regulator Enable (L4) failed! (%d)\n", rc);
			regulator_put(qt602240_reg_l4);
			return rc;
		}

		rc = regulator_enable(qt602240_reg_lvs3);
		if (rc) { 
			dev_err(&client->dev,"Regulator Enable (LVS3) failed! (%d)\n", rc);
			regulator_put(qt602240_reg_lvs3);
			return rc;
		}
 
 		rc = regulator_enable(qt602240_reg_mvs0);
		if (rc) { 
			dev_err(&client->dev,"Regulator Enable (mvs0) failed! (%d)\n", rc);
			regulator_put(qt602240_reg_mvs0);
			return rc;
		}

		msleep(20);

		/* Controller Reset */
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		msleep(10);    
		rc = gpio_direction_output(pdata->reset_gpio, 0);
		msleep(5);
		rc = gpio_direction_output(pdata->reset_gpio, 1);
		msleep(10);
  }
	else {    
		if (!qt602240_reg_l4)
			return rc;
	
		if (!qt602240_reg_lvs3)
			return rc;
	
		if (!qt602240_reg_mvs0)
			return rc;
    
		rc = regulator_disable(qt602240_reg_l4);

		if (rc) { 
			dev_err(&client->dev,"Regulator Disable (L4) failed! (%d)\n", rc);
			regulator_put(qt602240_reg_l4);
			return rc;
		}

		regulator_put(qt602240_reg_l4);
		qt602240_reg_l4 = NULL;
  	
		rc = regulator_disable(qt602240_reg_lvs3);
  	
		if (rc) { 
			dev_err(&client->dev,"Regulator Disable (LVS3) failed! (%d)\n", rc);
			return rc;
		}
		
		regulator_put(qt602240_reg_lvs3);
		qt602240_reg_lvs3 = NULL;
		
		rc = regulator_disable(qt602240_reg_mvs0);
  	
		if (rc) { 
			dev_err(&client->dev,"Regulator Disable (MVS0) failed! (%d)\n", rc);
			return rc;
		}
		
		regulator_put(qt602240_reg_mvs0);
		qt602240_reg_mvs0 = NULL;
		
		gpio_set_value_cansleep(pdata->reset_gpio, 0);
		gpio_set_value_cansleep(pdata->irq_gpio, 1);
		
		QT602240_I2C_PORT_DECONFIG();
		
		msleep(10);
	}
    
	return 0;
}

/* Ŀ�� Input ����̽� ��� */
static int qt602240_input_open(struct input_dev *dev)
{
	struct qt602240_data *data = input_get_drvdata(dev);

	qt602240_start(data);

	return 0;
}

/* Ŀ�� Input ����̽� ���� */
static void qt602240_input_close(struct input_dev *dev)
{
	struct qt602240_data *data = input_get_drvdata(dev);

	qt602240_stop(data);
}

/* ��ġ��ũ�� ����̹� Probe �κ� */
static int __devinit qt602240_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct qt602240_data *data;
	struct input_dev *input_dev;
	int error;

	if (!client->dev.platform_data)
		return -EINVAL;

	/* ���� data ����ü�� ���� �޸� �Ҵ� */
	data = kzalloc(sizeof(struct qt602240_data), GFP_KERNEL);
	
	/* Mutex ��� */
	mutex_init(&data->touch_mutex);
	
	/* Kernel Input Device �Ҵ� */
	input_dev = input_allocate_device();
	if (!data || !input_dev) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Input Device ���� �Ҵ� */
	input_dev->name = "KT Tech O3 Touchscreen Input Device";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = qt602240_input_open;
	input_dev->close = qt602240_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_HOME, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
	__set_bit(BTN_TOUCH, input_dev->keybit);

	/* For single touch */
	input_set_abs_params(input_dev, ABS_X,
			     0, QT602240_MAX_XC, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			     0, QT602240_MAX_YC, 0, 0);

	/* For multi touch */
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
			     0, QT602240_MAX_AREA, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, QT602240_MAX_XC, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
			     0, QT602240_MAX_YC, 0, 0);

	input_set_drvdata(input_dev, data);

	data->client = client;
	data->input_dev = input_dev;
	data->pdata = client->dev.platform_data;
	data->irq = client->irq;

	/* ��ġ ��ũ�� GPIO �� ���� Ȱ��ȭ */
	qt602240_init_touch_gpio(data);
	qt602240_power(QT602240_POWER_ON, data);  
	msleep(10);
    
	/* I2C �������̽� ��� */
	i2c_set_clientdata(client, data);

	/* O3 H/W Version ���� Ȯ�� */
	if (hw_ver == 0) {
		hw_ver = get_kttech_hw_version();
	}
	
	/* Touch Tuning ���� ���� ��� */ 
	if (hw_ver <= O3_PP2) {
		tsp_tune_rev = TSP_TUNING_REV_PP;
		qt602240_btn_area_y_top = QT602240_BTN_AREA_Y_TOP_PP;
	}	
	else if ( hw_ver >= O3_MP) {
		tsp_tune_rev = TSP_TUNING_REV_MP;
		qt602240_btn_area_y_top = QT602240_BTN_AREA_Y_TOP_MP;
	}
	
	/* I2C Port �ʱ�ȭ */
	QT602240_I2C_PORT_CONFIG();
	
	/* ��ġ��ũ�� ��Ʈ�ѷ� �ʱ�ȭ */
	error = qt602240_initialize(data);
	
	/* TSP Version ���� ���� */
	tsp_fw_version = data->info.version;
		
	if (error) {
		/* Touch Controller�� ����� �����Ǿ� ���� ���� ��� Flags ���� */
		qt602240_verification_ok = 0;
	}
	else {
		qt602240_verification_ok = 1;
	}
	
	if (qt602240_verification_ok) {		
		/* ������Ʈ �������� �߿��� ������Ʈ ���θ� �˻� */
		if (data->info.version < QT602240_VER_32) {
			dev_info(&client->dev, "FW update supported starting with version %d!\n", data->info.version);
			if (data->info.version == 0) {
				dev_info(&client->dev, "This Controller is already into Bootloader mode. Change Address.");
				/* Change to slave address of bootloader */
				if (client->addr == QT602240_APP_LOW)
					client->addr = QT602240_BOOT_LOW;
				else
					client->addr = QT602240_BOOT_HIGH;
			}
			error = qt602240_update_fw_store_internal(&client->dev);
			if (error)
				dev_err(&client->dev, "Touch Screen Driver F/W Update Failed!.\n");		
		}	
	
		/* �߿��� ������Ʈ �� �ʱ�ȭ ���� �� ���� ���� */
		if (error) {
			dev_err(&client->dev, "Fail to download the TOUCH F/W!\n");
			goto err_free_object;
		}
	}
	
	/* Input Device ��� */
	error = input_register_device(input_dev);
	if (error)
		goto err_free_irq;
	
	if (error) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_free_object;
	}			      

	if (qt602240_verification_ok) {
#ifdef TS_USE_WORK_QUEUE
		/* WorkQueue ���� ������ ���� */
		qt602240_wq = create_singlethread_workqueue("qt602240_wq");
		//qt602240_wq = create_workqueue("qt602240_wq");
		if (!qt602240_wq)
			return -ENOMEM;
#endif		
		qt602240_cal_wq = create_singlethread_workqueue("qt602240_cal_wq");
		if (!qt602240_cal_wq)
			return -ENOMEM;

		qt602240_autocal_wq = create_singlethread_workqueue("qt602240_autocal_wq");
		if (!qt602240_autocal_wq)
			return -ENOMEM;
						
		qt602240_100ms_timer_wq = create_singlethread_workqueue("qt602240_100ms_timer_wq");
		if (!qt602240_100ms_timer_wq)
			return -ENOMEM;
			
		qt602240_disable_freerun_wq = create_singlethread_workqueue("qt602240_disable_freerun_wq");
		if (!qt602240_disable_freerun_wq)
			return -ENOMEM;
			
		/* WorkQueue ��� */
#ifdef TS_USE_WORK_QUEUE
		INIT_WORK(&data->ts_event_work, qt602240_input_read);
#endif
		INIT_DELAYED_WORK(&data->ts_event_cal_work, calibrate_chip_wq);
		INIT_DELAYED_WORK(&data->ts_event_autocal_work, autocal_wq);
		INIT_DELAYED_WORK(&data->ts_100ms_timer_work, tsp_100ms_timer_wq);
		INIT_DELAYED_WORK(&data->disable_freerun_work, disable_freerun_wq);

		/* Thread ����� IRQ �ڵ鷯 ��� */
		error = request_threaded_irq(client->irq, NULL, qt602240_interrupt,
				IRQF_TRIGGER_FALLING, client->dev.driver->name, data);
	}
	
	/* SYSFS ��� */
	error = sysfs_create_group(&client->dev.kobj, &qt602240_attr_group);
	if (error)
		goto err_unregister_device;
		
	/* Android�� ���� Early Suspend ��� */
#ifdef CONFIG_HAS_EARLYSUSPEND 
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = qt602240_early_suspend;
	data->early_suspend.resume = qt602240_late_resume; 
	register_early_suspend(&data->early_suspend);
#endif /* CONFIG_HAS_EARLYSYSPEND */

	dev_info(&client->dev, "Touch Screen Driver Initialized Completed. (H/W Ver : %d, Tuning Rev : %d)\n", hw_ver, tsp_tune_rev);
	
	/* Touch Screen Controller Start */
	qt602240_start(data);
	
	/* Set Check Calibration Flags */
	cal_check_flag = 1u;
	good_check_flag = 0;
	doing_cal_flag = 0;
	
	return 0;

err_unregister_device:
	input_unregister_device(input_dev);
	input_dev = NULL;
err_free_irq:
	free_irq(client->irq, data);
err_free_object:
	kfree(data->object_table);
err_free_mem:
	input_free_device(input_dev);
	kfree(data);
	return error;
}

/* ����̽� ����̹� ���� */
static int __devexit qt602240_remove(struct i2c_client *client)
{
	struct qt602240_data *data = i2c_get_clientdata(client);

 if(qt602240_verification_ok) {
#ifdef TS_USE_WORK_QUEUE
	cancel_work_sync(&data->ts_event_work);
#endif
		free_irq(data->irq, data);
	}
	sysfs_remove_group(&client->dev.kobj, &qt602240_attr_group);
	input_unregister_device(data->input_dev);
	kfree(data->object_table);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
/* ��ġ��ũ�� Suspend */
static int qt602240_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct qt602240_data *data = i2c_get_clientdata(client);
		
	mutex_lock(&data->touch_mutex);

	disable_irq_nosync(data->irq);
	
	/* ��/���� ���� Ȱ��ȭ ���ο� ����, Suspend Resume�� Cal ���θ� �����Ѵ�. */
#ifdef USE_TOUCH_PROXIMITY_SENSOR
	if ((qt602240_get_proximity_enable_state == 0) && (qt602240_get_proximity_enable_state_old == 0)) {
#else
	if (1) {
#endif
#ifdef TS_USE_WORK_QUEUE
		if (work_pending(&data->ts_event_work))
			cancel_work_sync(&data->ts_event_work);
#endif
			
		if (delayed_work_pending(&data->ts_event_cal_work))
			cancel_delayed_work(&data->ts_event_cal_work);		
	
		if (delayed_work_pending(&data->ts_event_autocal_work))
			cancel_delayed_work(&data->ts_event_autocal_work);		
			
		if (delayed_work_pending(&data->ts_100ms_timer_work))
			cancel_delayed_work(&data->ts_100ms_timer_work);		
			
		if (delayed_work_pending(&data->disable_freerun_work))
			cancel_delayed_work(&data->disable_freerun_work);	
			
#ifdef QT602240_ENABLE_DEEPSLEEP_MODE
		qt602240_stop(data);
		qt602240_write_object(data, QT602240_GEN_POWER, QT602240_POWER_IDLEACQINT, 0);
		qt602240_write_object(data, QT602240_GEN_POWER, QT602240_POWER_ACTVACQINT, 0);
		
		//GPIO Configuration
		gpio_tlmm_config(GPIO_CFG(33, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
		
		/* Finger�� ����ŭ ������ ���鼭 ��ġ �������� �ʱ�ȭ �Ѵ�. */
		force_release_all_keys(data);
#else
		qt602240_power(QT602240_POWER_OFF, data);
#endif	
	}
	else {
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev, "Touch Report Position Disabled.\n");
		}		
		
		if (qt602240_get_proximity_enable_state == 0)
			qt602240_get_proximity_enable_state_old = 0;
		else
			qt602240_get_proximity_enable_state_old = 1; 
		
		force_release_all_keys(data);
		qt602240_report_disable(data);
		msleep(5);		
	}
	mutex_unlock(&data->touch_mutex);
	
	return 0;
}

/* ��ġ��ũ�� Resume */
static int qt602240_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct qt602240_data *data = i2c_get_clientdata(client);
#ifndef QT602240_ENABLE_DEEPSLEEP_MODE
	const struct qt602240_platform_data *pdata = data->pdata;
#endif
	int delay;
	
	mutex_lock(&data->touch_mutex);

	/* ��/���� ���� Ȱ��ȭ ���ο� ����, Suspend Resume�� Cal ���θ� �����Ѵ�. */
#ifdef USE_TOUCH_PROXIMITY_SENSOR
	if ((qt602240_get_proximity_enable_state == 0) && (qt602240_get_proximity_enable_state_old == 0)) {
#else
	if (1) {
#endif
		QT602240_I2C_PORT_CONFIG();
		
	/* Deep Sleep ���� ���� */
#ifdef QT602240_ENABLE_DEEPSLEEP_MODE
		//GPIO Configuration
		gpio_tlmm_config(GPIO_CFG(33, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
		
		/* Soft reset */
		qt602240_write_object(data, QT602240_GEN_COMMAND,
				QT602240_COMMAND_RESET, 1);
	
		msleep(QT602240_RESET_TIME);
#else
		/* ��ġ ��ũ�� GPIO �� ���� Ȱ��ȭ */
		gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		qt602240_power(QT602240_POWER_ON, data);
		msleep(10);
	
		/* soft reset */
		qt602240_write_object(data, QT602240_GEN_COMMAND, QT602240_COMMAND_RESET, 1);
		msleep(QT602240_RESET_TIME);
#endif	
	
		qt602240_start(data);
		msleep(5);
		
		/* Idle Acqint Enable */
		qt602240_write_object(data, QT602240_GEN_POWER, QT602240_POWER_IDLEACQINT, 32);
		
		/* Free-Run Mode Enable */
		qt602240_write_object(data, QT602240_GEN_POWER, QT602240_POWER_ACTVACQINT, ACTVACQINT_FREERUN_VAL);
		
		/* Calibration Check�� �����Ѵ�. */
		cal_check_flag = 0;
		good_check_flag = 0;
		doing_cal_flag = 0;
		doing_cal_skip_cnt = 0;
		
		/* Touch Good Check�� ���� Timer �ʱ�ȭ */
		qt_timer_ticks = 0;
		qt_timer_flag = 0;
		
		/* Calibration Command ���� */
		calibrate_chip(data);
	
		/* Finger�� ����ŭ ������ ���鼭 ��ġ �������� �ʱ�ȭ �Ѵ�. */
		force_release_all_keys(data);
	
		if (init_ts_reg > 0) {	
		/* ���� 3�� �̻� Touch Point�� ������ ���� ��츦 ���� ȸ�� ��ƾ */
		delay = CAL_RESUME_MSEC; //500ms
		queue_delayed_work(qt602240_cal_wq, &data->ts_event_cal_work, round_jiffies_relative(msecs_to_jiffies(delay)));
		}
	}
	else {
		if (debug_point_level & DEBUG_LOW_CAL) { 
			dev_info(&client->dev, "Touch Report Position Enabled.\n");
		}

		if (qt602240_get_proximity_enable_state == 0)
			qt602240_get_proximity_enable_state_old = 0;
		else
			qt602240_get_proximity_enable_state_old = 1; 
			
		force_release_all_keys(data);
		qt602240_report_enable(data);
		msleep(10);		
	}
	enable_irq(data->irq); 
		
	mutex_unlock(&data->touch_mutex);

	return 0;
}
#ifndef CONFIG_HAS_EARLYSUSPEND
static SIMPLE_DEV_PM_OPS(qt602240_pm, qt602240_suspend, qt602240_resume);
#endif
#else
/* Kernel�� Suspend Function�� �������� ���� ��� */
#define qt602240_suspend	NULL
#define qt602240_resume		NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND 
/* ��ġ��ũ�� Early Suspend */   
static void qt602240_early_suspend(struct early_suspend *h)   
{
	struct qt602240_data *data = container_of(h, struct qt602240_data, early_suspend);     
	struct i2c_client *client = data->client;
	
	atomic_set(&tsp_doing_suspend, 1);
	
	if(qt602240_verification_ok) {
		qt602240_suspend(client, PMSG_SUSPEND);
	}
	
	dev_info(&client->dev, "Touch Screen Driver Suspend Completed. (H/W Ver : %d, Tuning Rev : %d)\n", hw_ver, tsp_tune_rev);
}

/* ��ġ��ũ�� Early Resume*/  
static void qt602240_late_resume(struct early_suspend *h)   
{
	struct qt602240_data *data = container_of(h, struct qt602240_data, early_suspend);     
	struct i2c_client *client = data->client;
	
	atomic_set(&tsp_doing_suspend, 0);
	
	if(qt602240_verification_ok) {
		qt602240_resume(client);
	}	
	
	dev_info(&client->dev, "Touch Screen Driver Resume Completed. (H/W Ver : %d, Tuning Rev : %d)\n", hw_ver, tsp_tune_rev);
}
#endif /* CONFIG_HAS_EARLYSYSPEND */

/* I2C ID ���� ���� ��� */
static const struct i2c_device_id qt602240_id[] = {
	{ "kttech_o3_tsp", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, qt602240_id);

/* Linux Platform Device ���� ��� */
static struct i2c_driver qt602240_driver = {
	.driver = {
		.name	= "kttech_o3_tsp",
		.owner	= THIS_MODULE,
#ifndef CONFIG_HAS_EARLYSUSPEND 
        	.pm	= &qt602240_pm,
#endif
        .id_
	},
	.probe		= qt602240_probe,
	.remove		= __devexit_p(qt602240_remove),
	.id_table	= qt602240_id,
};

/* Driver Initialize */
static int __init qt602240_init(void)
{
	return i2c_add_driver(&qt602240_driver);
}

/* Driver Exit */
static void __exit qt602240_exit(void)
{
#ifdef TS_USE_WORK_QUEUE
	if (qt602240_wq)
		destroy_workqueue(qt602240_wq);
#endif
	if (qt602240_cal_wq)
		destroy_workqueue(qt602240_cal_wq);
	if (qt602240_autocal_wq)
		destroy_workqueue(qt602240_autocal_wq);
	if (qt602240_100ms_timer_wq)
		destroy_workqueue(qt602240_100ms_timer_wq);
	if (qt602240_disable_freerun_wq)
		destroy_workqueue(qt602240_disable_freerun_wq);
		
	i2c_del_driver(&qt602240_driver);
}

EXPORT_SYMBOL(qt602240_get_chg_state);
EXPORT_SYMBOL(qt602240_get_proximity_enable_state);
module_init(qt602240_init);
module_exit(qt602240_exit);

/* Module information */
MODULE_AUTHOR("KT tech Inc(JhoonKim)");
MODULE_DESCRIPTION("Qt602240 Touchscreen driver");
MODULE_LICENSE("GPL");
