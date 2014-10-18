/*
 *  apds990x.c - Linux kernel modules for ambient light + proximity sensor
 *
 *  Copyright (C) 2010 Lee Kai Koon <kai-koon.lee@avagotech.com>
 *  Copyright (C) 2010 Avago Technologies
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/sensors-kttech.h> /* KTTech sensor header file */
#ifdef KTTECH_SENSOR_AVAGO
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <mach/board.h>
#endif

#define APDS990x_DRV_NAME	"apds990x"
#define DRIVER_VERSION		"1.0.4"

#ifdef KTTECH_SENSOR_AVAGO
#define APDS990x_DEBUG_MSG
#define APDS990x_INT		124
#define APDS990x_I2C_SCL		103
#define APDS990x_I2C_SDA		104
#define APDS990x_I2C_RETRY		5

#define APDS990x_MAX_LUX		26
#define APDS990x_ALS_POLL_INTERVAL_MAX	1000 /* 1 secs */
#define APDS990x_ALS_POLL_INTERVAL_MIN	500 /* 0.5 sec */
#else
#define APDS990x_INT		IRQ_EINT20
#endif

#ifdef KTTECH_SENSOR_AVAGO
static int APDS990x_PS_DETECTION_THRESHOLD = 416;
static int APDS990x_PS_HSYTERESIS_THRESHOLD = 320;
#else
#define APDS990x_PS_DETECTION_THRESHOLD		600
#define APDS990x_PS_HSYTERESIS_THRESHOLD	500
#endif

#define APDS990x_ALS_THRESHOLD_HSYTERESIS	20	/* 20 = 20% */

/* Change History 
 *
 * 1.0.1	Functions apds990x_show_rev(), apds990x_show_id() and apds990x_show_status()
 *			have missing CMD_BYTE in the i2c_smbus_read_byte_data(). APDS-990x needs
 *			CMD_BYTE for i2c write/read byte transaction.
 *
 *
 * 1.0.2	Include PS switching threshold level when interrupt occurred
 *
 *
 * 1.0.3	Implemented ISR and delay_work, correct PS threshold storing
 *
 * 1.0.4	Added Input Report Event
 */

/*
 * Defines
 */

#define APDS990x_ENABLE_REG	0x00
#define APDS990x_ATIME_REG	0x01
#define APDS990x_PTIME_REG	0x02
#define APDS990x_WTIME_REG	0x03
#define APDS990x_AILTL_REG	0x04
#define APDS990x_AILTH_REG	0x05
#define APDS990x_AIHTL_REG	0x06
#define APDS990x_AIHTH_REG	0x07
#define APDS990x_PILTL_REG	0x08
#define APDS990x_PILTH_REG	0x09
#define APDS990x_PIHTL_REG	0x0A
#define APDS990x_PIHTH_REG	0x0B
#define APDS990x_PERS_REG	0x0C
#define APDS990x_CONFIG_REG	0x0D
#define APDS990x_PPCOUNT_REG	0x0E
#define APDS990x_CONTROL_REG	0x0F
#define APDS990x_REV_REG	0x11
#define APDS990x_ID_REG		0x12
#define APDS990x_STATUS_REG	0x13
#define APDS990x_CDATAL_REG	0x14
#define APDS990x_CDATAH_REG	0x15
#define APDS990x_IRDATAL_REG	0x16
#define APDS990x_IRDATAH_REG	0x17
#define APDS990x_PDATAL_REG	0x18
#define APDS990x_PDATAH_REG	0x19

#define CMD_BYTE	0x80
#define CMD_WORD	0xA0
#define CMD_SPECIAL	0xE0

#define CMD_CLR_PS_INT	0xE5
#define CMD_CLR_ALS_INT	0xE6
#define CMD_CLR_PS_ALS_INT	0xE7

/*
 * Structs
 */

struct apds990x_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct delayed_work	dwork;	/* for PS interrupt */
	struct delayed_work    als_dwork; /* for ALS polling */
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;

	unsigned int enable;
	unsigned int atime;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int ailt;
	unsigned int aiht;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold; /* always lower than ps_threshold */
	unsigned int ps_detection;		/* 0 = near-to-far; 1 = far-to-near */
	unsigned int ps_data;			/* to store PS data */

	/* ALS parameters */
	unsigned int als_threshold_l;	/* low threshold */
	unsigned int als_threshold_h;	/* high threshold */
	unsigned int als_data;			/* to store ALS data */

	unsigned int als_gain;			/* needed for Lux calculation */
	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int als_atime;			/* storage for als integratiion time */
};

/*
 * Global data
 */

#ifdef KTTECH_SENSOR_AVAGO
/*
 * Static data
 */
static int apds990x_lux_table[APDS990x_MAX_LUX] = {
	4, 10, 20, 30, 40, 50, 60, 70, 85, 100,
	150, 200, 250, 300, 400, 500, 600, 700, 800, 1000,
	1200, 1500, 2000, 3000, 4000, 5000
};

static int hw_ver;
static struct i2c_client * apds990x_client_backup = 0;
static int apds990x_init_client(struct i2c_client *client);
#endif

/*
 * Management functions
 */

#ifdef KTTECH_SENSOR_AVAGO
static s32 apds990x_i2c_smbus_write_byte(const struct i2c_client *client, u8 value)
{
	int retry;
	int ret = 0;

	for (retry = 0 ; retry < APDS990x_I2C_RETRY ; retry++) {
		ret = i2c_smbus_write_byte(client, value);
		if (ret == 0)
			break;
		else
			msleep(10);
	}

	if (retry >= APDS990x_I2C_RETRY) {
		printk("%s: retry over %d : error[%d]\n", __func__, APDS990x_I2C_RETRY, ret);
	}
	return ret;
}

static s32 apds990x_i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command, u8 value)
{
	int retry;
	int ret = 0;

	for (retry = 0 ; retry < APDS990x_I2C_RETRY ; retry++) {
		ret = i2c_smbus_write_byte_data(client, command, value);
		if (ret == 0)
			break;
		else
			msleep(10);
	}

	if (retry >= APDS990x_I2C_RETRY) {
		printk("%s: retry over %d : error[%d]\n", __func__, APDS990x_I2C_RETRY, ret);
	}
	return ret;
}

static s32 apds990x_i2c_smbus_write_word_data(const struct i2c_client *client, u8 command, u16 value)
{
	int retry;
	int ret = 0;

	for (retry = 0 ; retry < APDS990x_I2C_RETRY ; retry++) {
		ret = i2c_smbus_write_word_data(client, command, value);
		if (ret == 0)
			break;
		else
			msleep(10);
	}

	if (retry >= APDS990x_I2C_RETRY) {
		printk("%s: retry over %d : error[%d]\n", __func__, APDS990x_I2C_RETRY, ret);
	}
	return ret;
}

static s32 apds990x_i2c_smbus_read_byte_data(const struct i2c_client *client, u8 command)
{
	int retry;
	int ret = 0;

	for (retry = 0 ; retry < APDS990x_I2C_RETRY ; retry++) {
		ret = i2c_smbus_read_byte_data(client, command);
		if (ret >= 0)
			break;
		else
			msleep(10);
	}

	if (retry >= APDS990x_I2C_RETRY) {
		printk("%s: retry over %d : error[%d]\n", __func__, APDS990x_I2C_RETRY, ret);
	}
	return ret;
}

static s32 apds990x_i2c_smbus_read_word_data(const struct i2c_client *client, u8 command)
{
	int retry;
	int ret = 0;

	for (retry = 0 ; retry < APDS990x_I2C_RETRY ; retry++) {
		ret = i2c_smbus_read_word_data(client, command);
		if (ret >= 0)
			break;
		else
			msleep(10);
	}

	if (retry >= APDS990x_I2C_RETRY) {
		printk("%s: retry over %d : error[%d]\n", __func__, APDS990x_I2C_RETRY, ret);
	}
	return ret;
}

static struct regulator * apds990x_8901_l3; //3.0v main power
static struct regulator * apds990x_8901_lvs1; //1.8V pull up
static int apds990x_power_on = 0;
static int apds990x_power_control(int on)
{
	int rc = 0;

	if(on) {
		if (apds990x_power_on)
			return 0;

		apds990x_8901_lvs1 = regulator_get(NULL, "8901_lvs1");
		apds990x_8901_l3 = regulator_get(NULL, "8901_l3");

		if (IS_ERR(apds990x_8901_l3) || IS_ERR(apds990x_8901_lvs1)) {
			pr_err("%s: regulator_get(8901_l3) failed (%d)\n", __func__, rc);
			return PTR_ERR(apds990x_8901_l3);
		}

		//set voltage level
		rc = regulator_set_voltage(apds990x_8901_l3,3000000, 3000000);
		if (rc)
		{ 
			pr_err("%s: regulator_set_voltage(apds990x_8901_l3) failed (%d)\n", __func__, rc);
			regulator_put(apds990x_8901_l3);
			return rc;
		}

		//enable output
		rc = regulator_enable(apds990x_8901_l3);
		if (rc)
		{ 
			pr_err("%s: regulator_enable(apds990x_8901_l3) failed (%d)\n", __func__, rc);
			regulator_put(apds990x_8901_l3);
			return rc;
		}

		rc = regulator_enable(apds990x_8901_lvs1);
		if (rc)
		{ 
			pr_err("%s: regulator_enable(8901_lvs1) failed (%d)\n", __func__, rc);
			regulator_put(apds990x_8901_lvs1);
			return rc;
		}

		gpio_tlmm_config(GPIO_CFG(APDS990x_I2C_SCL, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(APDS990x_I2C_SDA, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		apds990x_power_on = 1;

#ifdef APDS990x_DEBUG_MSG
		printk("apds990x_power_control on\n");
#endif
	}
	else {
		struct apds990x_data *data = i2c_get_clientdata(apds990x_client_backup);

		printk("apds990x_power_control apds990x_power_on=%d\n", apds990x_power_on);
		printk("apds990x_power_control data->enable_als_sensor=%d\n", data->enable_als_sensor);
		printk("apds990x_power_control data->enable_ps_sensor=%d\n", data->enable_ps_sensor);
		if (!apds990x_power_on || data->enable_als_sensor || data->enable_ps_sensor)
			return 0;

		if (apds990x_8901_l3)
		{
			rc = regulator_disable(apds990x_8901_l3);

			if (rc)
			{ 
				pr_err("%s: regulator_disable(8901_l3) failed (%d)\n", __func__, rc);
				regulator_put(apds990x_8901_l3);
				return rc;
			}
			regulator_put(apds990x_8901_l3);

			apds990x_8901_l3 = NULL;
		}

		if (apds990x_8901_lvs1)
		{
			rc = regulator_disable(apds990x_8901_lvs1);

			if (rc)
			{ 
				pr_err("%s: regulator_disable(8901_lvs1b) failed (%d)\n", __func__, rc);
				regulator_put(apds990x_8901_lvs1);
				return rc;
			}
			regulator_put(apds990x_8901_lvs1);
		
			apds990x_8901_lvs1 = NULL;
		}

		gpio_tlmm_config(GPIO_CFG(APDS990x_I2C_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(APDS990x_I2C_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(APDS990x_I2C_SCL, 0);
		gpio_set_value(APDS990x_I2C_SDA, 0);
		apds990x_power_on = 0;
	
#ifdef APDS990x_DEBUG_MSG
		printk("apds990x_power_control off\n");
#endif		
	}

	return 0;
}
#endif

static int apds990x_set_command(struct i2c_client *client, int command)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	int clearInt;

	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;
		
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte(client, clearInt);
#else
	ret = i2c_smbus_write_byte(client, clearInt);
#endif
	mutex_unlock(&data->update_lock);

	return ret;
}

static int apds990x_set_enable(struct i2c_client *client, int enable)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ENABLE_REG, enable);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ENABLE_REG, enable);
#endif
	mutex_unlock(&data->update_lock);

	data->enable = enable;

	return ret;
}

static int apds990x_set_atime(struct i2c_client *client, int atime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ATIME_REG, atime);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ATIME_REG, atime);
#endif
	mutex_unlock(&data->update_lock);

	data->atime = atime;

	return ret;
}

static int apds990x_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_PTIME_REG, ptime);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_PTIME_REG, ptime);
#endif
	mutex_unlock(&data->update_lock);

	data->ptime = ptime;

	return ret;
}

static int apds990x_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_WTIME_REG, wtime);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_WTIME_REG, wtime);
#endif
	mutex_unlock(&data->update_lock);

	data->wtime = wtime;

	return ret;
}

static int apds990x_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO	
	ret = apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AILTL_REG, threshold);
#else
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AILTL_REG, threshold);
#endif
	mutex_unlock(&data->update_lock);
	
	data->ailt = threshold;

	return ret;
}

static int apds990x_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AIHTL_REG, threshold);
#else
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AIHTL_REG, threshold);
#endif
	mutex_unlock(&data->update_lock);
	
	data->aiht = threshold;

	return ret;
}

static int apds990x_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, threshold);
#else
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, threshold);
#endif
	mutex_unlock(&data->update_lock);
	
	data->pilt = threshold;

	return ret;
}

static int apds990x_set_piht(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, threshold);
#else
	ret = i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, threshold);
#endif
	mutex_unlock(&data->update_lock);
	
	data->piht = threshold;

	return ret;
}

static int apds990x_set_pers(struct i2c_client *client, int pers)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_PERS_REG, pers);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_PERS_REG, pers);
#endif
	mutex_unlock(&data->update_lock);

	data->pers = pers;

	return ret;
}

static int apds990x_set_config(struct i2c_client *client, int config)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_CONFIG_REG, config);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_CONFIG_REG, config);
#endif
	mutex_unlock(&data->update_lock);

	data->config = config;

	return ret;
}

static int apds990x_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_PPCOUNT_REG, ppcount);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_PPCOUNT_REG, ppcount);
#endif
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;

	return ret;
}

static int apds990x_set_control(struct i2c_client *client, int control)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	
	mutex_lock(&data->update_lock);
#ifdef KTTECH_SENSOR_AVAGO
	ret = apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_CONTROL_REG, control);
#else
	ret = i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_CONTROL_REG, control);
#endif
	mutex_unlock(&data->update_lock);

	data->control = control;

	/* obtain ALS gain value */
	if ((data->control&0x03) == 0x00) /* 1X Gain */
		data->als_gain = 1;
	else if ((data->control&0x03) == 0x01) /* 8X Gain */
		data->als_gain = 8;
	else if ((data->control&0x03) == 0x02) /* 16X Gain */
		data->als_gain = 16;
	else  /* 120X Gain */
		data->als_gain = 120;

	return ret;
}

static int LuxCalculation(struct i2c_client *client, int cdata, int irdata)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int luxValue=0;

	int IAC1=0;
	int IAC2=0;
	int IAC=0;
	int GA=48;			/* 0.48 without glass window */
	int COE_B=223;		/* 2.23 without glass window */
	int COE_C=70;		/* 0.70 without glass window */
	int COE_D=142;		/* 1.42 without glass window */
	int DF=52;

#ifdef KTTECH_SENSOR_AVAGO
	if(hw_ver == ES2) {
		GA = 256;		/* 2.56 with ES2 glass window */
		COE_B = 212;	/* 2.12 with ES2 glass window */
		COE_C = 53;		/* 0.53 with ES2 glass window */
		COE_D = 105;	/* 1.05 with ES2 glass window */
	}	
	else if(hw_ver >= PP1) {
		GA = 221;		/* 2.21 with ES3 glass window */
		COE_B = 211;	/* 2.11 with ES3 glass window */
		COE_C = 56;		/* 0.56 with ES3 glass window */
		COE_D = 110;	/* 1.10 with ES3 glass window */
	}
#endif

	IAC1 = (cdata - (COE_B*irdata)/100);	// re-adjust COE_B to avoid 2 decimal point
	IAC2 = ((COE_C*cdata)/100 - (COE_D*irdata)/100); // re-adjust COE_C and COE_D to void 2 decimal point

	if (IAC1 > IAC2)
		IAC = IAC1;
	else if (IAC1 <= IAC2)
		IAC = IAC2;
	else
		IAC = 0;

	if (IAC1 < 0 && IAC2 < 0) 
		IAC = 0;

	luxValue = ((IAC*GA*DF)/100)/(((272*(256-data->atime))/100)*data->als_gain);

	return luxValue;
}

static void apds990x_change_ps_threshold(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);

#ifdef KTTECH_SENSOR_AVAGO
	data->ps_data =	apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);	
#else
	data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);
#endif

#ifdef APDS990x_DEBUG_MSG
	printk("apds990x_change_ps_threshold data->ps_data=%d data->pilt=%d data->piht=%d\n", data->ps_data, data->pilt, data->piht);
#endif
	if ( (data->ps_data > data->pilt) && (data->ps_data >= data->piht) ) {
		/* far-to-near detected */
		data->ps_detection = 1;

#ifdef KTTECH_SENSOR_AVAGO
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* FAR-to-NEAR detection */	
#else
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* FAR-to-NEAR detection */	
#endif
		input_sync(data->input_dev_ps);

#ifdef KTTECH_SENSOR_AVAGO
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, data->ps_hysteresis_threshold);
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, 1023);
#else
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, data->ps_hysteresis_threshold);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, 1023);
#endif

		data->pilt = data->ps_hysteresis_threshold;
		data->piht = 1023;

#ifdef APDS990x_DEBUG_MSG
		printk("far-to-near detected\n");
#endif
	}
	else if ( (data->ps_data <= data->pilt) && (data->ps_data < data->piht) ) {
		/* near-to-far detected */
		data->ps_detection = 0;

#ifdef KTTECH_SENSOR_AVAGO
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* NEAR-to-FAR detection */	
#else
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* NEAR-to-FAR detection */	
#endif
		input_sync(data->input_dev_ps);

#ifdef KTTECH_SENSOR_AVAGO
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, 0);
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, data->ps_threshold);
#else
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, data->ps_threshold);
#endif

		data->pilt = 0;
		data->piht = data->ps_threshold;

#ifdef APDS990x_DEBUG_MSG
		printk("near-to-far detected\n");
#endif
	}
}

static void apds990x_change_als_threshold(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int cdata, irdata;
	int luxValue=0;

#ifdef KTTECH_SENSOR_AVAGO
	cdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
	irdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_IRDATAL_REG);	
#else
	cdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
	irdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_IRDATAL_REG);
#endif

	luxValue = LuxCalculation(client, cdata, irdata);

	luxValue = luxValue>0 ? luxValue : 0;
	luxValue = luxValue<10000 ? luxValue : 10000;
	
	// check PS under sunlight
	if ( (data->ps_detection == 1) && (cdata > (75*(1024*(256-data->atime)))/100))	// PS was previously in far-to-near condition
	{
		// need to inform input event as there will be no interrupt from the PS
#ifdef KTTECH_SENSOR_AVAGO
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* NEAR-to-FAR detection */	
#else
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* NEAR-to-FAR detection */	
#endif
		input_sync(data->input_dev_ps);

#ifdef KTTECH_SENSOR_AVAGO
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, 0);
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, data->ps_threshold);
#else
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, data->ps_threshold);
#endif

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

#ifdef APDS990x_DEBUG_MSG
		printk("apds_990x_proximity_handler = FAR\n");
#endif
	}


	input_report_abs(data->input_dev_als, ABS_MISC, luxValue); // report the lux level
	input_sync(data->input_dev_als);

	data->als_data = cdata;

	data->als_threshold_l = (data->als_data * (100-APDS990x_ALS_THRESHOLD_HSYTERESIS) ) /100;
	data->als_threshold_h = (data->als_data * (100+APDS990x_ALS_THRESHOLD_HSYTERESIS) ) /100;

	if (data->als_threshold_h >= 65535) data->als_threshold_h = 65535;

#ifdef KTTECH_SENSOR_AVAGO
	apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AILTL_REG, data->als_threshold_l);

	apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AIHTL_REG, data->als_threshold_h);
#else
	i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AILTL_REG, data->als_threshold_l);

	i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_AIHTL_REG, data->als_threshold_h);
#endif
}

static void apds990x_reschedule_work(struct apds990x_data *data,
					  unsigned long delay)
{
	unsigned long flags;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);

	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	__cancel_delayed_work(&data->dwork);
	schedule_delayed_work(&data->dwork, delay);

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
}

/* ALS polling routine */
static void apds990x_als_polling_work_handler(struct work_struct *work)
{
	struct apds990x_data *data = container_of(work, struct apds990x_data, als_dwork.work);
	struct i2c_client *client=data->client;
	int cdata, irdata, pdata;
	int luxValue=0;
#ifdef KTTECH_SENSOR_AVAGO
	int i;
#endif

#ifdef KTTECH_SENSOR_AVAGO
	if (!data->enable_als_sensor) {
		return;
	}
	cdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
	irdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_IRDATAL_REG);
	pdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);
#else
	cdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
	irdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_IRDATAL_REG);
	pdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);
#endif
	
	luxValue = LuxCalculation(client, cdata, irdata);
	
	luxValue = luxValue>0 ? luxValue : 0;
	luxValue = luxValue<10000 ? luxValue : 10000;

#if 0//def APDS990x_DEBUG_MSG	
	printk("%s: lux = %d cdata = %x  irdata = %x pdata = %x \n", __func__, luxValue, cdata, irdata, pdata);
#endif

	// check PS under sunlight
	if ( (data->ps_detection == 1) && (cdata > (75*(1024*(256-data->atime)))/100))	// PS was previously in far-to-near condition
	{
		// need to inform input event as there will be no interrupt from the PS
#ifdef KTTECH_SENSOR_AVAGO
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* NEAR-to-FAR detection */	
#else
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);/* NEAR-to-FAR detection */	
#endif
		input_sync(data->input_dev_ps);

#ifdef KTTECH_SENSOR_AVAGO
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, 0);
		apds990x_i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, data->ps_threshold);
#else
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD|APDS990x_PIHTL_REG, data->ps_threshold);
#endif

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

#ifdef APDS990x_DEBUG_MSG
		printk("apds_990x_proximity_handler = FAR\n");
#endif
	}

#ifdef KTTECH_SENSOR_AVAGO
	for (i = 0 ; i < APDS990x_MAX_LUX ; i++) {
		if (apds990x_lux_table[i] > luxValue) {
			luxValue = apds990x_lux_table[i];
			break;
		}
	}
#endif

	input_report_abs(data->input_dev_als, ABS_MISC, luxValue); // report the lux level
	input_sync(data->input_dev_als);
	
	
	schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	// restart timer
}

/* PS interrupt routine */
static void apds990x_work_handler(struct work_struct *work)
{
	struct apds990x_data *data = container_of(work, struct apds990x_data, dwork.work);
	struct i2c_client *client=data->client;
	int	status;
	int cdata;

#ifdef KTTECH_SENSOR_AVAGO
	status = apds990x_i2c_smbus_read_byte_data(client, CMD_BYTE|APDS990x_STATUS_REG);
#else
	status = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS990x_STATUS_REG);
#endif

#ifdef KTTECH_SENSOR_AVAGO
	apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ENABLE_REG, 1);
#else
	i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ENABLE_REG, 1);	/* disable 990x's ADC first */
#endif

#ifdef APDS990x_DEBUG_MSG
	printk("status = %x\n", status);
#endif

	if ((status & data->enable & 0x30) == 0x30) {
#ifdef APDS990x_DEBUG_MSG		
		printk("both PS and ALS are interrupted\n");
#endif
		/* both PS and ALS are interrupted */
		apds990x_change_als_threshold(client);

#ifdef KTTECH_SENSOR_AVAGO
		cdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
#else		
		cdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
#endif
		if (cdata < (75*(1024*(256-data->atime)))/100)
			apds990x_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1) {
				apds990x_change_ps_threshold(client);			
			}
			else {
#ifdef APDS990x_DEBUG_MSG				
				printk("Triggered by background ambient noise\n");
#endif
			}
		}

		apds990x_set_command(client, 2);	/* 2 = CMD_CLR_PS_ALS_INT */
	}
	else if ((status & data->enable & 0x20) == 0x20) {
		/* only PS is interrupted */
#ifdef APDS990x_DEBUG_MSG		
		printk("only PS is interrupted\n");
#endif
		/* check if this is triggered by background ambient noise */
#ifdef KTTECH_SENSOR_AVAGO
		cdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
#else
		cdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
#endif
		if (cdata < (75*(1024*(256-data->atime)))/100)
			apds990x_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1) {
				apds990x_change_ps_threshold(client);			
			}
			else {
#ifdef APDS990x_DEBUG_MSG				
				printk("Triggered by background ambient noise\n");
#endif
			}
		}

		apds990x_set_command(client, 0);	/* 0 = CMD_CLR_PS_INT */
	}
	else if ((status & data->enable & 0x10) == 0x10) {
		/* only ALS is interrupted */	
		apds990x_change_als_threshold(client);

		apds990x_set_command(client, 1);	/* 1 = CMD_CLR_ALS_INT */
	}

#ifdef KTTECH_SENSOR_AVAGO
	apds990x_i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ENABLE_REG, data->enable);
#else
	i2c_smbus_write_byte_data(client, CMD_BYTE|APDS990x_ENABLE_REG, data->enable);	
#endif
}

/* assume this is ISR */
static irqreturn_t apds990x_interrupt(int vec, void *info)
{
	struct i2c_client *client=(struct i2c_client *)info;
	struct apds990x_data *data = i2c_get_clientdata(client);

#ifdef APDS990x_DEBUG_MSG
	printk("==> apds990x_interrupt\n");
#endif

#ifdef KTTECH_SENSOR_AVAGO
	if (data->enable_ps_sensor)
		apds990x_reschedule_work(data, 0);	
#else
	apds990x_reschedule_work(data, 0);	
#endif

	return IRQ_HANDLED;
}

/*
 * SysFS support
 */

static ssize_t apds990x_show_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);
	
	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t apds990x_store_enable_ps_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);
 	unsigned long flags;

#ifdef APDS990x_DEBUG_MSG	
	printk("%s: enable ps senosr ( %ld)\n", __func__, val);
#endif	
	if ((val != 0) && (val != 1)) {
#ifdef APDS990x_DEBUG_MSG		
		printk("%s:store unvalid value=%ld\n", __func__, val);
#endif
		return count;
	}
	
	if(val == 1) {
		//turn on p sensor
		if (data->enable_ps_sensor==0) {
#ifdef KTTECH_SENSOR_AVAGO
			apds990x_power_control(1);
			//msleep(100);

			if (!data->enable_als_sensor) {
				/* Initialize the APDS990x chip */
				apds990x_init_client(client);
			}
#endif
			data->enable_ps_sensor= 1;
		
			apds990x_set_enable(client,0); /* Power Off */
#ifdef KTTECH_SENSOR_AVAGO
			apds990x_set_atime(client, 0xB7); /* 198.56ms */
#else
			apds990x_set_atime(client, 0xf6); /* 27.2ms */
#endif
			apds990x_set_ptime(client, 0xff); /* 2.72ms */
		
			apds990x_set_ppcount(client, 8); /* 8-pulse */
			apds990x_set_control(client, 0x20); /* 100mA, IR-diode, 1X PGAIN, 1X AGAIN */
		
			apds990x_set_pilt(client, 0);		// init threshold for proximity
			apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);

			data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
			data->ps_hysteresis_threshold = APDS990x_PS_HSYTERESIS_THRESHOLD;
		
			apds990x_set_ailt( client, 0);
			apds990x_set_aiht( client, 0xffff);
		
			apds990x_set_pers(client, 0x22); /* 2 persistence */
		
			if (data->enable_als_sensor==0) {

				/* we need this polling timer routine for sunlight canellation */
				spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
			
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				__cancel_delayed_work(&data->als_dwork);
				schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	// 100ms
			
				spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);	
			}

			apds990x_set_enable(client, 0x27);	 /* only enable PS interrupt */
		}
	} 
	else {
#ifdef KTTECH_SENSOR_AVAGO
		if (!data->enable_ps_sensor)
			return count;
#endif
		//turn off p sensor - kk 25 Apr 2011 we can't turn off the entire sensor, the light sensor may be needed by HAL
		data->enable_ps_sensor = 0;
		if (data->enable_als_sensor) {
			
			// reconfigute light sensor setting			
			apds990x_set_enable(client,0); /* Power Off */
			
			apds990x_set_atime(client, data->als_atime);  /* previous als poll delay */
			
			apds990x_set_ailt( client, 0);
			apds990x_set_aiht( client, 0xffff);
			
			apds990x_set_control(client, 0x20); /* 100mA, IR-diode, 1X PGAIN, 1X AGAIN */
			apds990x_set_pers(client, 0x22); /* 2 persistence */
			
			apds990x_set_enable(client, 0x3);	 /* only enable light sensor */
			
			spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
			
			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			__cancel_delayed_work(&data->als_dwork);
			schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	// 100ms
			
			spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);	
			
		}
		else {
			apds990x_set_enable(client, 0);

			spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
			
			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			__cancel_delayed_work(&data->als_dwork);
			spin_unlock_irqrestore(&data->update_lock.wait_lock, flags); 
		}
#ifdef KTTECH_SENSOR_AVAGO
		apds990x_power_control(0);
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1); /* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);
#endif		
	}
	
	
	return count;
}

#ifdef KTTECH_SENSOR_AVAGO
static DEVICE_ATTR(enable_ps_sensor, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP,
				   apds990x_show_enable_ps_sensor, apds990x_store_enable_ps_sensor);
#else
static DEVICE_ATTR(enable_ps_sensor, S_IWUGO | S_IRUGO,
				   apds990x_show_enable_ps_sensor, apds990x_store_enable_ps_sensor);
#endif

static ssize_t apds990x_show_enable_als_sensor(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);
	
	return sprintf(buf, "%d\n", data->enable_als_sensor);
}

static ssize_t apds990x_store_enable_als_sensor(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);
 	unsigned long flags;

#ifdef APDS990x_DEBUG_MSG	
	printk("%s: enable als sensor ( %ld)\n", __func__, val);
#endif

	if ((val != 0) && (val != 1))
	{
#ifdef APDS990x_DEBUG_MSG	
		printk("%s: enable als sensor=%ld\n", __func__, val);
#endif
		return count;
	}
	
	if(val == 1) {
		//turn on light  sensor
		if (data->enable_als_sensor==0) {
#ifdef KTTECH_SENSOR_AVAGO
			apds990x_power_control(1);
			//msleep(100);

			if (!data->enable_ps_sensor) {
				/* Initialize the APDS990x chip */
				apds990x_init_client(client);
			}
#endif			

			data->enable_als_sensor = 1;
		
			apds990x_set_enable(client,0); /* Power Off */
		
			apds990x_set_atime(client, data->als_atime);  /* 100.64ms */
		
			apds990x_set_ailt( client, 0);
			apds990x_set_aiht( client, 0xffff);
		
			apds990x_set_control(client, 0x20); /* 100mA, IR-diode, 1X PGAIN, 1X AGAIN */
			apds990x_set_pers(client, 0x22); /* 2 persistence */
		
			if (data->enable_ps_sensor) {
				apds990x_set_ptime(client, 0xff); /* 2.72ms */
			
				apds990x_set_ppcount(client, 8); /* 8-pulse */			
				apds990x_set_enable(client, 0x27);	 /* if prox sensor was activated previously */
			}
			else {
				apds990x_set_enable(client, 0x3);	 /* only enable light sensor */
			}
		
			spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
		
			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			__cancel_delayed_work(&data->als_dwork);
			schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));
		
			spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
		}
	}
	else {
#ifdef KTTECH_SENSOR_AVAGO
		if (!data->enable_als_sensor)
			return count;
#endif		
		//turn off light sensor
		// what if the p sensor is active?
		data->enable_als_sensor = 0;
		if (data->enable_ps_sensor) {
			apds990x_set_enable(client,0); /* Power Off */
#ifdef KTTECH_SENSOR_AVAGO
			apds990x_set_atime(client, 0xB7); /* 198.56ms */
#else
			apds990x_set_atime(client, 0xf6);  /* 27.2ms */
#endif
			apds990x_set_ptime(client, 0xff); /* 2.72ms */
			apds990x_set_ppcount(client, 8); /* 8-pulse */
			apds990x_set_control(client, 0x20); /* 100mA, IR-diode, 1X PGAIN, 1X AGAIN */

#ifndef KTTECH_SENSOR_AVAGO			
			apds990x_set_pilt(client, 0);
			apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);
#endif

			apds990x_set_ailt( client, 0);
			apds990x_set_aiht( client, 0xffff);
			
			apds990x_set_pers(client, 0x22); /* 2 persistence */
			apds990x_set_enable(client, 0x27);	 /* only enable prox sensor with interrupt */			
		}
		else {
			apds990x_set_enable(client, 0);
		}
		
		
		spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
		
		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		__cancel_delayed_work(&data->als_dwork);
		
		spin_unlock_irqrestore(&data->update_lock.wait_lock, flags); 
#ifdef KTTECH_SENSOR_AVAGO
		apds990x_power_control(0);
#endif
	}
	
	return count;
}

#ifdef KTTECH_SENSOR_AVAGO
static DEVICE_ATTR(enable_als_sensor, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP,
				   apds990x_show_enable_als_sensor, apds990x_store_enable_als_sensor);
#else
static DEVICE_ATTR(enable_als_sensor, S_IWUGO | S_IRUGO,
				   apds990x_show_enable_als_sensor, apds990x_store_enable_als_sensor);
#endif

static ssize_t apds990x_show_als_poll_delay(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);
	
	return sprintf(buf, "%d\n", data->als_poll_delay*1000);	// return in micro-second
}

static ssize_t apds990x_store_als_poll_delay(struct device *dev,
					struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	int ret;
#ifndef KTTECH_SENSOR_AVAGO	
	int poll_delay=0;
#endif
 	unsigned long flags;

#ifdef KTTECH_SENSOR_AVAGO
	if (!data->enable_als_sensor)
		return count;

	if ((val / 1000) < APDS990x_ALS_POLL_INTERVAL_MIN)
		data->als_poll_delay = APDS990x_ALS_POLL_INTERVAL_MIN;
	else if ((val / 1000) > APDS990x_ALS_POLL_INTERVAL_MAX)
		data->als_poll_delay = APDS990x_ALS_POLL_INTERVAL_MAX;
	else
		data->als_poll_delay = val/1000;	// convert us => ms

	data->als_atime = 0xB7;	// 198.56ms ALS integration time, the minimum is 2.72ms = 2720 us, maximum is 696.32ms
#else
	if (val<5000)
		val = 5000;	// minimum 5ms
	
	data->als_poll_delay = val/1000;	// convert us => ms
	
	poll_delay = 256 - (val/2720);	// the minimum is 2.72ms = 2720 us, maximum is 696.32ms
	if (poll_delay >= 256)
		data->als_atime = 255;
	else if (poll_delay < 0)
		data->als_atime = 0;
	else
		data->als_atime = poll_delay;
#endif

	ret = apds990x_set_atime(client, data->als_atime);
	
	if (ret < 0)
		return ret;

	/* we need this polling timer routine for sunlight canellation */
	spin_lock_irqsave(&data->update_lock.wait_lock, flags); 
		
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	__cancel_delayed_work(&data->als_dwork);
	schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	// 100ms
			
	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);	
	
	return count;
}

static DEVICE_ATTR(als_poll_delay, S_IWUSR | S_IRUGO,
				   apds990x_show_als_poll_delay, apds990x_store_als_poll_delay);

#ifdef KTTECH_SENSOR_AVAGO
static ssize_t apds990x_show_ps_raw_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ps_raw_data = 0;
	struct i2c_client *client = to_i2c_client(dev);

#ifdef KTTECH_SENSOR_AVAGO
	ps_raw_data = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);
	ps_raw_data = ps_raw_data / 32;
#else
	ps_raw_data = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_PDATAL_REG);
#endif

	buf[0] = ps_raw_data & 0xFF;
	buf[1] = (ps_raw_data >> 8) & 0xFF;
	buf[2] = (ps_raw_data >> 16) & 0xFF;
	buf[3] = (ps_raw_data >> 24) & 0xFF;
	
	return 4;
}

static DEVICE_ATTR(ps_raw_data, S_IWUSR | S_IRUGO,
				   apds990x_show_ps_raw_data, NULL);

static ssize_t apds990x_store_calibration_data(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long cal_val = 0;

	struct i2c_client *client = to_i2c_client(dev);
	struct apds990x_data *data = i2c_get_clientdata(client);

	cal_val = *buf * 32;

	if(hw_ver == ES2) {
		APDS990x_PS_DETECTION_THRESHOLD = 520;
		APDS990x_PS_HSYTERESIS_THRESHOLD = 420;
	}	
	else if(hw_ver >= PP1) {
		APDS990x_PS_DETECTION_THRESHOLD = 416;
		APDS990x_PS_HSYTERESIS_THRESHOLD = 320;
	}

	if((cal_val > APDS990x_PS_DETECTION_THRESHOLD) && ((cal_val + 150) < 1023)) {
		APDS990x_PS_DETECTION_THRESHOLD = cal_val + 50;
		APDS990x_PS_HSYTERESIS_THRESHOLD = APDS990x_PS_DETECTION_THRESHOLD + 100;
	}

	data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
	data->ps_hysteresis_threshold = APDS990x_PS_HSYTERESIS_THRESHOLD;
	if(apds990x_power_on) {
		apds990x_set_pilt(client, 0);
		apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);
	}

	return count;
}

static DEVICE_ATTR(ps_calibration_data, S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP,
				   NULL, apds990x_store_calibration_data);

static ssize_t apds990x_show_als_raw_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int cdata, irdata;
	int luxValue=0;
	struct i2c_client *client = to_i2c_client(dev);

#ifdef KTTECH_SENSOR_AVAGO
	cdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
	irdata = apds990x_i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_IRDATAL_REG);
#else
	cdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_CDATAL_REG);
	irdata = i2c_smbus_read_word_data(client, CMD_WORD|APDS990x_IRDATAL_REG);
#endif

	luxValue = LuxCalculation(client, cdata, irdata);

	buf[0] = luxValue & 0xFF;
	buf[1] = (luxValue >> 8) & 0xFF;
	buf[2] = (luxValue >> 16) & 0xFF;
	buf[3] = (luxValue >> 24) & 0xFF;
	
	return 4;
}

static DEVICE_ATTR(als_raw_data, S_IWUSR | S_IRUGO,
				   apds990x_show_als_raw_data, NULL);
#endif

#ifdef KTTECH_SENSOR_AVAGO
static struct attribute *apds990x_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_als_sensor.attr,
	&dev_attr_als_poll_delay.attr,
	&dev_attr_ps_raw_data.attr,
	&dev_attr_ps_calibration_data.attr,
	&dev_attr_als_raw_data.attr,
	NULL
};
#else
static struct attribute *apds990x_attributes[] = {
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_enable_als_sensor.attr,
	&dev_attr_als_poll_delay.attr,
	NULL
};
#endif


static const struct attribute_group apds990x_attr_group = {
	.attrs = apds990x_attributes,
};

/*
 * Initialization function
 */

static int apds990x_init_client(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int err;
	int id;

	err = apds990x_set_enable(client, 0);

	if (err < 0)
		return err;

#ifdef KTTECH_SENSOR_AVAGO
	id = apds990x_i2c_smbus_read_byte_data(client, CMD_BYTE|APDS990x_ID_REG);
#else
	id = i2c_smbus_read_byte_data(client, CMD_BYTE|APDS990x_ID_REG);
#endif
	if (id == 0x20) {
		printk("APDS-9901\n");
	}
	else if (id == 0x29) {
		printk("APDS-990x\n");
	}
	else {
		printk("Neither APDS-9901 nor APDS-9901\n");
		return -EIO;
	}

#ifdef KTTECH_SENSOR_AVAGO
	apds990x_set_atime(client, 0xB7);	// 198.56ms ALS integration time
#else
	apds990x_set_atime(client, 0xDB);	// 100.64ms ALS integration time
#endif
	apds990x_set_ptime(client, 0xFF);	// 2.72ms Prox integration time
	apds990x_set_wtime(client, 0xFF);	// 2.72ms Wait time

	apds990x_set_ppcount(client, 0x08);	// 8-Pulse for proximity
	apds990x_set_config(client, 0);		// no long wait
	apds990x_set_control(client, 0x20);	// 100mA, IR-diode, 1X PGAIN, 1X AGAIN

	apds990x_set_pilt(client, 0);		// init threshold for proximity
	apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);

	data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
	data->ps_hysteresis_threshold = APDS990x_PS_HSYTERESIS_THRESHOLD;

	apds990x_set_ailt(client, 0);		// init threshold for als
	apds990x_set_aiht(client, 0xFFFF);

	apds990x_set_pers(client, 0x22);	// 2 consecutive Interrupt persistence

	// sensor is in disabled mode but all the configurations are preset

	return 0;
}

/*
 * I2C init/probing/exit functions
 */

static struct i2c_driver apds990x_driver;
static int __devinit apds990x_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds990x_data *data;
	int err = 0;

#ifdef KTTECH_SENSOR_AVAGO
	apds990x_power_control(1);
#endif /* KTTECH_SENSOR_BMC050 */

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct apds990x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->client = client;
	i2c_set_clientdata(client, data);

#ifdef KTTECH_SENSOR_AVAGO	
	apds990x_client_backup = client;
#endif
	data->enable = 0;	/* default mode is standard */
	data->ps_threshold = 0;
	data->ps_hysteresis_threshold = 0;
	data->ps_detection = 0;	/* default to no detection */
	data->enable_als_sensor = 0;	// default to 0
	data->enable_ps_sensor = 0;	// default to 0
	data->als_poll_delay = 100;	// default to 100ms
#ifdef KTTECH_SENSOR_AVAGO
	data->als_atime	= 0xB7;	// 198.56ms ALS integration time
#else
	data->als_atime	= 0xdb;			// work in conjuction with als_poll_delay
#endif
	
	printk("enable = %x\n", data->enable);

	mutex_init(&data->update_lock);

#ifdef KTTECH_SENSOR_AVAGO
	if (gpio_tlmm_config(GPIO_CFG(APDS990x_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), GPIO_CFG_ENABLE)) {
		printk(KERN_ERR "%s: Err: Config AVAGO_IRQ_GPIO\n", __func__);
	}

	msleep(100);

	INIT_DELAYED_WORK(&data->dwork, apds990x_work_handler);
	INIT_DELAYED_WORK(&data->als_dwork, apds990x_als_polling_work_handler);

	/* Initialize the APDS990x chip */
	err = apds990x_init_client(client);
	if (err)
		goto exit_kfree;
	
	//if (request_irq(MSM_GPIO_TO_INT(APDS990x_INT), apds990x_interrupt, IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING,
	if (request_irq(MSM_GPIO_TO_INT(APDS990x_INT), apds990x_interrupt, (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING),
		APDS990x_DRV_NAME, (void *)client)) {
		printk("%s Could not allocate APDS990x_INT !\n", __func__);
	
		goto exit_kfree;
	}

	if (enable_irq_wake(MSM_GPIO_TO_INT(APDS990x_INT))) {
		printk("%s Could not enable irq wake for APDS990x_INT !\n", __func__);
		free_irq(MSM_GPIO_TO_INT(APDS990x_INT), 0);

		goto exit_kfree;
	}

	printk("%s interrupt is hooked\n", __func__);
#else
	if (request_irq(APDS990x_INT, apds990x_interrupt, IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING,
		APDS990x_DRV_NAME, (void *)client)) {
		printk("%s Could not allocate APDS990x_INT !\n", __func__);
	
		goto exit_kfree;
	}

	INIT_DELAYED_WORK(&data->dwork, apds990x_work_handler);
	INIT_DELAYED_WORK(&data->als_dwork, apds990x_als_polling_work_handler); 

	printk("%s interrupt is hooked\n", __func__);

	/* Initialize the APDS990x chip */
	err = apds990x_init_client(client);
	if (err)
		goto exit_kfree;
#endif

	/* Register to Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		err = -ENOMEM;
		printk("Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		printk("Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}
	
	set_bit(EV_ABS, data->input_dev_als->evbit);
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_als, ABS_MISC, 0, 10000, 0, 0);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);

	data->input_dev_als->name = "Avago light sensor";
	data->input_dev_ps->name = "Avago proximity sensor";

	err = input_register_device(data->input_dev_als);
	if (err) {
		err = -ENOMEM;
		printk("Unable to register input device als: %s\n",
		       data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		printk("Unable to register input device ps: %s\n",
		       data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &apds990x_attr_group);
	if (err)
		goto exit_unregister_dev_ps;

	input_report_abs(data->input_dev_ps, ABS_DISTANCE, 1);/* FAR-to-NEAR detection */
	input_sync(data->input_dev_ps);
	printk("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);

#ifdef KTTECH_SENSOR_AVAGO
	apds990x_power_control(0);
#endif /* KTTECH_SENSOR_BMC050 */
	return 0;

exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);	
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_free_dev_als:
	input_free_device(data->input_dev_als);
exit_free_irq:
	free_irq(APDS990x_INT, client);	
exit_kfree:
	kfree(data);
exit:
#ifdef KTTECH_SENSOR_AVAGO
	apds990x_power_control(0);
#endif /* KTTECH_SENSOR_BMC050 */
	return err;
}

static int __devexit apds990x_remove(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	
	input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);
	
	input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);

	free_irq(APDS990x_INT, client);

	sysfs_remove_group(&client->dev.kobj, &apds990x_attr_group);

	/* Power down the device */
	apds990x_set_enable(client, 0);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static int apds990x_suspend(struct i2c_client *client, pm_message_t mesg)
{
#ifdef KTTECH_SENSOR_AVAGO
  #if 1//gpio IRQ isuue workaround
  	struct apds990x_data *data = i2c_get_clientdata(client);
         printk("=========> %s  data->enable_ps_sensor=%d \n", __func__,data->enable_ps_sensor);
  	if (data->enable_ps_sensor)
            return -EBUSY ;
         else 
            return 0;
  #else       
  	   return 0;
  #endif
#else
	return apds990x_set_enable(client, 0);
#endif
}

static int apds990x_resume(struct i2c_client *client)
{
#ifdef KTTECH_SENSOR_AVAGO
	return 0;
#else
	return apds990x_set_enable(client, 0);
#endif
}

#else

#define apds990x_suspend	NULL
#define apds990x_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id apds990x_id[] = {
	{ "apds990x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds990x_id);

static struct i2c_driver apds990x_driver = {
	.driver = {
		.name	= APDS990x_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = apds990x_suspend,
	.resume	= apds990x_resume,
	.probe	= apds990x_probe,
	.remove	= __devexit_p(apds990x_remove),
	.id_table = apds990x_id,
};

static int __init apds990x_init(void)
{
#ifdef KTTECH_SENSOR_AVAGO
	hw_ver = get_kttech_hw_version();

	if(hw_ver == ES2) {
		APDS990x_PS_DETECTION_THRESHOLD = 520;
		APDS990x_PS_HSYTERESIS_THRESHOLD = 420;
	}	
	else if(hw_ver >= PP1) {
		APDS990x_PS_DETECTION_THRESHOLD = 416;
		APDS990x_PS_HSYTERESIS_THRESHOLD = 320;
	}
#endif
	return i2c_add_driver(&apds990x_driver);
}

static void __exit apds990x_exit(void)
{
	i2c_del_driver(&apds990x_driver);
}

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS990x ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds990x_init);
module_exit(apds990x_exit);

