#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <mach/board.h>

#ifdef CONFIG_KTTECH_BATTERY_GAUGE_TI

#define KTTech_0410_B104_Update_20111209

#if defined(KTTech_0410_B104_Update_20111209)
#include "KTTech_0410_B104_Update_20111209_bqfs.h"
#endif

#ifdef CONFIG_MSM_WATCHDOG
extern void pet_watchdog(void);
#endif

void kttech_battery_gauge_init(void);
static int ti_gauge_enter_rom_mode(void);
int kttech_battery_gauge_get_soc(void);
int kttech_battery_gauge_get_voltage(void);
int kttech_battery_gauge_get_current(void);
int kttech_battery_gauge_get_temperature(void);
int kttech_battery_gauge_get_availableEnergy(void);
int kttech_battery_gauge_get_remaining_cap(void);
void kttech_battery_gauge_reset(void);

int ti_gauge_read_cmd(u8 regoffset, u16 *value);
int ti_gauge_write_cmd(u8 regoffset, u16 value);
int ti_gauge_read_subcmd(u16 cntl_data, u16 *value);
int ti_gauge_write_subcmd(u16 cntl_data);
int ti_gauge_read_for_DFI(u8 regoffset, u8 *value, u8 num_bytes);
int ti_gauge_write_for_DFI(u8 regoffset, u8 data[], u8 num_bytes);
int ti_gauge_write_byte_for_DFI(u8 regoffset, u8 data);

static int i2c_err_count = 0;
static int bq27410_is_present = 1;

#define BQ27410_SLAVE_ADDR			0x55
#define BQ27410_SLAVE_ADDR_FOR_DFI	0x0B
#define BQ27410_QUP_I2C_BUS_ID	2

#define BQ27410_I2C_RETRY_NUM		4

/* Standard Commands */
#define BQ27410_STD_CNTL	0x00 /* Control, N/A, R/W */
#define BQ27410_STD_TEMP	0x02 /* Temperature, 0.1K, R/W */
#define BQ27410_STD_VOLT	0x04 /* Voltage, mV, R*/
#define BQ27410_STD_FLAGS	0x06 /* Flags, N/A, R */
#define BQ27410_STD_NAC	0x08 /* NominalAvailableCapacity, mAh, R */
#define BQ27410_STD_FAC	0x0A /* FullAvailableCapacity, mAh, R */
#define BQ27410_STD_RM	0x0C /* RemainingCapacity, mAh, R */
#define BQ27410_STD_FCC	0x0E /* FullChargeCapacity, mAh, R */
#define BQ27410_STD_AI		0x10 /* AverageCurrent, mA, R */
#define BQ27410_STD_SI		0x12 /* StandbyCurrent, mA, R */
#define BQ27410_STD_MLI	0x14 /* MaxLoadCurrent, mA, R */
#define BQ27410_STD_AE		0x16 /* AvailableEnergy, 10mWhr, R */
#define BQ27410_STD_AP		0x18 /* AveragePower, 10mW, R*/
#define BQ27410_STD_SOC	0x1C /* StateOfCharge, %, R */
#define BQ27410_STD_ITEMP	0x1E /* IntTemperature, 0.1K, R */
#define BQ27410_STD_SOH	0x20 /* StateOfHealth, %, R */

/* Extended Data Commands */
#define BQ27410_EXT_OPCFG	0x3A /* OperationConfiguration, N/A, R */
#define BQ27410_EXT_DCAP	0x3C /* DesignCapacity, mAh, R */

/* Control Subcommands */
#define BQ27410_SUBCMD_CONTROL_STATUS	0x0000
#define BQ27410_SUBCMD_DEVICE_TYPE		0x0001
#define BQ27410_SUBCMD_FW_VERSION		0x0002
#define BQ27410_SUBCMD_HW_VERSION		0x0003
#define BQ27410_SUBCMD_PREV_MACWRITE	0x0007
#define BQ27410_SUBCMD_BAT_INSERT		0x000C
#define BQ27410_SUBCMD_BAT_REMOVE		0x000D
#define BQ27410_SUBCMD_SET_FULLSLEEP	0x0010
#define BQ27410_SUBCMD_SET_HIBERNATE	0x0011
#define BQ27410_SUBCMD_CLEAR_HIBERNATE	0x0012
#define BQ27410_SUBCMD_FACTORY_RESTORE	0x0015
#define BQ27410_SUBCMD_SEALED			0x0020
#define BQ27410_SUBCMD_RESET			0x0041
#define BQ27410_SUBCMD_ROM_MODE		0x0F00

#define BQ27410_SUBCMD_ENABLE_SOC_FILTER 0x001C
#define BQ27410_SUBCMD_DISABLE_SOC_FILTER 0x001D

#define BQ27410_REG_VOLT_LSB		0x04
#define BQ27410_REG_VOLT_MSB		0x05
#define BQ27410_REG_SOC_MSB		0x1C
#define BQ27410_REG_SOC_LSB			0x1D

int ti_gauge_read_for_DFI(u8 regoffset, u8 *value, u8 num_bytes)
{
	struct i2c_adapter *adap;
	int ret;
	struct i2c_msg msg[2];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	/* setup the address to read */
	msg[0].addr = BQ27410_SLAVE_ADDR_FOR_DFI;
	msg[0].len = 1;
	msg[0].flags = 0;
	msg[0].buf = &regoffset;

	/* setup the read buffer */
	msg[1].addr = BQ27410_SLAVE_ADDR_FOR_DFI;
	msg[1].flags = I2C_M_RD;
	msg[1].len = num_bytes;
	msg[1].buf = value;

	ret = i2c_transfer(adap, msg, 2);

	return (ret >= 0) ? 0 : ret;
}

int ti_gauge_write_for_DFI(u8 regoffset, u8 data[], u8 num_bytes)
{
	int ret;
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	u8 buf[num_bytes + 1];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	buf[0] = regoffset;
	memcpy(&buf[1], data, num_bytes);

	msg.addr = BQ27410_SLAVE_ADDR_FOR_DFI;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = num_bytes + 1;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret >= 0)
		return 0;
	return ret;
}

int ti_gauge_read_byte_for_DFI(u8 regoffset, u8 *value)
{
	struct i2c_adapter *adap;
	int ret;
	struct i2c_msg msg[2];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	/* setup the address to read */
	msg[0].addr = BQ27410_SLAVE_ADDR_FOR_DFI;
	msg[0].len = 1;
	msg[0].flags = 0;
	msg[0].buf = &regoffset;

	/* setup the read buffer */
	msg[1].addr = BQ27410_SLAVE_ADDR_FOR_DFI;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = value;

	ret = i2c_transfer(adap, msg, 2);

	return (ret >= 0) ? 0 : ret;
}

int ti_gauge_write_byte_for_DFI(u8 regoffset, u8 data)
{
	int ret;
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	u8 buf[2];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	buf[0] = regoffset;
	buf[1] = data;

	msg.addr = BQ27410_SLAVE_ADDR_FOR_DFI;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = 2;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret >= 0)
		return 0;
	return ret;
}

int ti_gauge_read_cmd(u8 regoffset, u16 *value)
{
	struct i2c_adapter *adap;
	int ret;
	struct i2c_msg msg[2];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	/* setup the address to read */
	msg[0].addr = BQ27410_SLAVE_ADDR;
	msg[0].len = 1;
	msg[0].flags = 0;
	msg[0].buf = &regoffset;

	/* setup the read buffer */
	msg[1].addr = BQ27410_SLAVE_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = (u8 *)value;

	ret = i2c_transfer(adap, msg, 2);

	return (ret >= 0) ? 0 : ret;
}

int ti_gauge_write_cmd(u8 regoffset, u16 value)
{
	int ret;
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	u8 buf[3];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	buf[0] = regoffset;
	buf[1] = value &0xFF;
	buf[2] = (value >> 8)&0xFF;

	msg.addr = BQ27410_SLAVE_ADDR;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = 3;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret >= 0)
		return 0;
	return ret;
}

int ti_gauge_read_subcmd(u16 cntl_data, u16 *value)
{
	int ret;
	ret = ti_gauge_write_subcmd(cntl_data);
	if(ret == 0) {
		ti_gauge_read_cmd(BQ27410_STD_CNTL, value);
	}

	return (ret >= 0) ? 0 : ret;
}

int ti_gauge_write_subcmd(u16 cntl_data)
{
	int ret;
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	u8 buf[3];

	adap = i2c_get_adapter(BQ27410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	buf[0] = BQ27410_STD_CNTL;
	buf[1] = cntl_data &0xFF;
	buf[2] = (cntl_data >> 8)&0xFF;

	msg.addr = BQ27410_SLAVE_ADDR;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = 3;

	ret = i2c_transfer(adap, &msg, 1);
	if (ret >= 0)
		return 0;
	return ret;
}

static int ti_gauge_enter_rom_mode(void)
{
	int err;

	/* Start Read and Erase First Two Rows of Instruction Flash */
	err = ti_gauge_write_subcmd(BQ27410_SUBCMD_ROM_MODE);
	if(err < 0) {
		printk(KERN_INFO "Rom mode failed\n");
		return -1;;
	}
	mdelay(1000);
	return 0;
}

static int ti_gauge_update_firmware_write(u8 *firmware_image, int * read_pos)
{
	int pos = *read_pos;
	int i, result;
	u8 buffer[256];
	u8 regoffset = 0;
	u8 num_bytes = 0;

	#ifdef CONFIG_MSM_WATCHDOG
	pet_watchdog(); /* watchdog reset */
	#endif

	regoffset = firmware_image[pos+1];
	num_bytes = firmware_image[pos+3];
	pos += 4;

	for(i = 0; i < num_bytes; i++)
	{
		buffer[i] = firmware_image[pos+i];
	}


	result = ti_gauge_write_for_DFI(regoffset, buffer, num_bytes);
	if(result != 0) {
		return -1;
	}
		
	* read_pos = pos + num_bytes;

	return 0;
}

static int ti_gauge_update_firmware_read(u8 *firmware_image, int * read_pos)
{
	return 0;
}

static int ti_gauge_update_firmware_compare(u8 *firmware_image, int * read_pos)
{
	int pos = *read_pos;
	int i, result;
	u8 buffer[256];
	u8 regoffset = 0;
	u8 num_bytes = 0;

	regoffset = firmware_image[pos+1];
	num_bytes = firmware_image[pos+3];
	pos += 4;

	result = ti_gauge_read_for_DFI(regoffset, buffer, num_bytes);
	if(result != 0) {
		return -1;
	}

	for(i = 0; i < num_bytes; i++)
	{
		if(buffer[i] != firmware_image[pos+i]) {
			return -1;
		}
	}

	* read_pos = pos + num_bytes;

	return 0;
}

static int ti_gauge_update_firmware_wait(u8 *firmware_image, int * read_pos)
{
	int pos = *read_pos;
	int wait_time = 0;

	wait_time = firmware_image[pos+2] * 256 + firmware_image[pos+3];
	wait_time += 2;

	#ifdef CONFIG_MSM_WATCHDOG
	if(wait_time > 1000) {
		pet_watchdog(); /* watchdog reset */
	}
	#endif

	mdelay(wait_time);

	*read_pos = pos+4;

	return 0;
}

static void ti_gauge_update_firmware(void)
{
#if defined(bq27410G1_1_03_Nocal_20111013)
	u8 *firmware_Image = bq27410G1_1_03_Nocal_20111013_bqfs;
#elif defined(KTTech_0410_A104_Update_20111005)
	u8 *firmware_Image = KTTech_0410_A104_Update_20111005_bqfs;
#elif defined(KTTech_0410_B104_Update_20111209)
	u8 *firmware_Image = KTTech_0410_B104_Update_20111209_bqfs;
#endif
	int total_size = firmware_Image[0] * 256*256*256 + firmware_Image[1] * 256*256 + firmware_Image[2] * 256 + firmware_Image[3];
	int read_pos = 4;
	int result=0;

	while(read_pos < total_size)
	{
		switch(firmware_Image[read_pos])
		{
		case 'W':
			result = ti_gauge_update_firmware_write(firmware_Image, &read_pos);
			break;
		case 'R':
			result = ti_gauge_update_firmware_read(firmware_Image, &read_pos);			
			break;
		case 'C':
			result = ti_gauge_update_firmware_compare(firmware_Image, &read_pos);			
			break;
		case 'X':
			result = ti_gauge_update_firmware_wait(firmware_Image, &read_pos);			
			break;
		default:
			break;
		}

		if(result != 0)
			goto error;
	}

	printk(KERN_INFO "kttech-charger - update firmware\n");

error:
	mdelay(1000);

	result = ti_gauge_write_subcmd(BQ27410_SUBCMD_RESET);
	if(result != 0) {
		printk(KERN_INFO "error - Reset \n");
	}
}

int kttech_battery_gauge_get_bat_det(void)
{
	u16 flags = 0;
	int err = 0;
		
	err = ti_gauge_read_cmd(BQ27410_STD_FLAGS, &flags);
	if((err==0) && (flags & 0x8)) {
		return 1;	
	}

	return 0;
}

void kttech_battery_gauge_init(void)
{
	int err;
	u16 Op_Config = 0;
	u16 design_cap = 0;
	u16 fw_ver = 0;
	u16 cnt_status = 0;

	err = ti_gauge_read_subcmd(BQ27410_SUBCMD_FW_VERSION, &fw_ver);
	if(err < 0) {
		/*
			I2C통신이 안되면, rom mode상태인지 확인해야 합니다. 
			업데이트 도중에 실패하게 되면, rom mode상태로 있습니다. 
			read only인 0x4번지를 이용해서  rom mode여부를 판단합니다.  (TI문서 SLUA541A참고) 
		*/ 
		u8 x = 0;
		u8 y = 0;
		err = ti_gauge_read_byte_for_DFI(0x04, &x);
		if(err >= 0) {
			err = ti_gauge_write_byte_for_DFI(0x04, ~x);
			if(err >= 0) {
				err = ti_gauge_read_byte_for_DFI(0x04, &y);
				if(err >= 0) {
					if(x != y) {	/* x와 y가 다르면 rom mode상태 */ 
						ti_gauge_update_firmware();
						ti_gauge_read_subcmd(BQ27410_SUBCMD_FW_VERSION, &fw_ver);
						printk(KERN_INFO "kttech-charger (%x)\n", fw_ver);
						return;
					}
				}
			}
		}

		bq27410_is_present = 0;
		return;
	}

	ti_gauge_read_cmd(BQ27410_EXT_OPCFG, &Op_Config);
	ti_gauge_read_cmd(BQ27410_EXT_DCAP, &design_cap);
	ti_gauge_read_subcmd(BQ27410_SUBCMD_CONTROL_STATUS, &cnt_status);

	Op_Config = Op_Config & 0xFF;

	printk(KERN_INFO "kttech-charger (%x) (%d) (%d) (%x)\n", fw_ver, Op_Config, design_cap, cnt_status);

#if defined(bq27410G1_1_03_Nocal_20111013)
	if(fw_ver != 0x0103)
#elif defined(KTTech_0410_A104_Update_20111005)
	if(fw_ver != 0xa104)
#elif defined(KTTech_0410_B104_Update_20111209)
	if(fw_ver != 0xb104)
#else
	if(0)	
#endif
	{
		err = ti_gauge_enter_rom_mode();
		if(err == 0) {
			ti_gauge_update_firmware();
		}
	}

#if defined(KTTech_0410_A104_Update_20111005) || defined(KTTech_0410_B104_Update_20111209)
	err = ti_gauge_write_subcmd(BQ27410_SUBCMD_ENABLE_SOC_FILTER);
	if(err == 0) {
		u16 flags = 0;
		err = ti_gauge_read_cmd(BQ27410_STD_FLAGS, &flags);
		if((err==0) && (flags & 0x40)) {
			printk(KERN_INFO "kttech-charger Enable SOC filter\n");
		}
	}

#endif
}

int kttech_battery_gauge_get_temperature(void)
{
	u16 temperature; 
	int err;
	int i  = 0;

	if(bq27410_is_present == 0) {
		return 0;
	}

	for(i = 0; i < BQ27410_I2C_RETRY_NUM; i++) {
		err = ti_gauge_read_cmd(BQ27410_STD_ITEMP, &temperature);
		if(err >= 0) {
			return (temperature-2731)/10;
		}
		i2c_err_count++;
	}

	printk(KERN_INFO "kttech-charger I2C failed. %d\n", i2c_err_count);
	return 0;
}

int kttech_battery_gauge_get_voltage(void)
{
	u16 voltage; 
	int err;
	int i  = 0;

	if(bq27410_is_present == 0) {
		return -1;
	}

	for(i = 0; i < BQ27410_I2C_RETRY_NUM; i++) {
		err = ti_gauge_read_cmd(BQ27410_STD_VOLT, &voltage);
		if(err >= 0) {
			return (int)voltage;
		}
		i2c_err_count++;
	}

	printk(KERN_INFO "kttech-charger I2C failed.. %d\n", i2c_err_count);
	return 0;
}

int kttech_battery_gauge_get_current(void)
{
	s16 avg_current;
	int err;
	int i  = 0;

	if(bq27410_is_present == 0) {
		return 0;
	}

	for(i = 0; i < BQ27410_I2C_RETRY_NUM; i++) {
		err = ti_gauge_read_cmd(BQ27410_STD_AI, &avg_current);
		if(err >= 0) {
			printk(KERN_INFO "kttech-charger %d mA\n", (int)avg_current);
			return (int)avg_current;
		}
		i2c_err_count++;
	}

	printk(KERN_INFO "kttech-charger I2C failed... %d\n", i2c_err_count);
	return 0;
}

int kttech_battery_gauge_get_soc(void)
{
	u16 soc;
	int avg_current;
	int voltage;
	int err;
	int i  = 0;

	if(bq27410_is_present == 0) {
		return -1;
	}

	for(i = 0; i < BQ27410_I2C_RETRY_NUM; i++) {
		err = ti_gauge_read_cmd(BQ27410_STD_SOC, &soc);
		if(err >= 0) {
			avg_current = kttech_battery_gauge_get_current();	
			voltage = kttech_battery_gauge_get_voltage();

			if(soc & 0x80) {
				soc = 0;
			} else {
				soc = (soc & 0x7F);
			}

			if(soc < 3)
				soc = 0;
			else if(soc < 50)
				soc = soc - 2;
			else if(soc < 75)
				soc = soc - 1;

			if(soc == 0) {
				if(avg_current > -300) {
					if(voltage > 3400)
						soc = 1;
				} else if (avg_current > -500) {
					if(voltage > 3350)
						soc = 1;
				} else {
					if(voltage > 3325)
						soc = 1;
				}
			} else {
				if((voltage > 2800) && (voltage < 3300))
					soc = 0;
			}

			/*
			 * To fix the problem that charging is finished at 98% or 99%,
			 * soc value will be remapped like this.
			 * -- -- -- -- -- -- -- -- -- -- -- --
			 * 0 ~ 98% => (re-mapping) => 0 ~ 100%
			 */
			soc = (((soc * 1000)/98) + 5)/10;
			//printk("@@@ new soc is [%d]\n", soc);

			if(soc < 0)
				soc = 0;
			else if(soc > 100)
				soc = 100;

			printk(KERN_INFO "kttech-charger soc %d \n", (int)soc);
			return (int)soc;
		}
		i2c_err_count++;
	}

	printk(KERN_INFO "kttech-charger I2C failed.... %d\n", i2c_err_count);
	return 0;
}

void kttech_battery_gauge_reset(void)
{
	int err;

	if(bq27410_is_present == 0) {
		return;
	}

	err = ti_gauge_write_subcmd(BQ27410_SUBCMD_RESET);
	if(err < 0) {
		i2c_err_count++;
		printk(KERN_INFO "kttech-charger I2C failed.... %d\n", i2c_err_count);
	}
}

int kttech_battery_gauge_get_availableEnergy(void)
{
	s16 available_energy;
	int err;
	int i  = 0;

	if(bq27410_is_present == 0) {
		return 0;
	}

	for(i = 0; i < BQ27410_I2C_RETRY_NUM; i++) {
		err = ti_gauge_read_cmd(BQ27410_STD_AE, &available_energy);
		if(err >= 0) {
			return (int)available_energy;
		}
		i2c_err_count++;
	}

	printk(KERN_INFO "kttech-charger I2C failed... %d\n", i2c_err_count);
	return err;
}

int kttech_battery_gauge_get_remaining_cap(void)
{
	s16 remaining_cap;
	int err;
	int i  = 0;

	if(bq27410_is_present == 0) {
		return 0;
	}

	for(i = 0; i < BQ27410_I2C_RETRY_NUM; i++) {
		err = ti_gauge_read_cmd(BQ27410_STD_RM, &remaining_cap);
		if(err >= 0) {
			return (int)remaining_cap;
		}
		i2c_err_count++;
	}

	printk(KERN_INFO "kttech-charger I2C failed... %d\n", i2c_err_count);
	return err;
}

/* Module information */
MODULE_AUTHOR("KT tech Inc");
MODULE_DESCRIPTION("BQ27410 Battery gauge driver");
MODULE_LICENSE("GPL");
#endif
