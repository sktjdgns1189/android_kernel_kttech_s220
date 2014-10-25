/* Copyright (c) 2011, KTTech. All rights reserved.
 *
 * YDA165 Sound Amp Driver
 */

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/android_pmem.h>
#include <linux/gpio.h>
#include <linux/mutex.h>

#include <linux/mfd/msm-adie-codec.h>
#include <linux/msm_audio.h>
#include <linux/pmic8058-othc.h>

#include <mach/qdsp6v2/snddev_yda165.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>

#define YDA165_USED_REG_0X87
#define YDA165_USED_REG_0X86
//#define YDA165_USED_REG_NOISEGATE
#define YDA165_USED_REG_NONCLIP
#define YDA165_USED_REG_0X85_MUTE // Speaker Amplifier
#define YDA165_USED_REG_0X86_MUTE // Headset Amplifier
#define YDA165_USED_REG_DELAY
static AMP_PATH_TYPE_E m_curr_path = AMP_PATH_NONE;
static AMP_PATH_TYPE_E m_prev_path = AMP_PATH_NONE;
struct snddev_yda165 yda165_modules;

#define YDA165_RESET_REG		0x80
#define YDA165_RESET_VALUE		0x80
#define YDA165_SPK_SW_GPIO		105

REG_MEMORY yda165_register_type amp_headset_stereo_path[REG_COUNT] = {
#ifdef YDA165_USED_REG_DELAY
	{ 0x80 , 0x01 },  //
	{ 0x81 , 0x58 },  //0x58:HPgain0dB_ECOen
	{ 0x84 , 0x02 },  //0x02:IN1inputgain-3dB_IN2gain0dB
	{ AMP_REGISTER_DELAY , 14 },
	{ 0x86 , 0x19 },  //0x19:HPATT-3dB
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x87 , 0x02 },  //
#else
	{ 0x80 , 0x01 },  //
	{ 0x81 , 0x58 },  //0x58:HPgain0dB_ECOen
	{ 0x82 , 0x0D },  //0x0d:powerlimit_off
	{ 0x83 , 0x06 },  //0x06:Spk24dB_Non-clip1%_NGoff
	{ 0x84 , 0x02 },  //0x02:IN1inputgain-3dB_IN2gain0dB
	{ 0x85 , 0x00 },  //0x00:SPATTmute
	{ 0x86 , 0x19 },  //0x19:HPATT-3dB
	{ 0x87 , 0x02 },  //
#endif
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY yda165_register_type amp_speaker_stereo_path[REG_COUNT] = {
#ifdef YDA165_USED_REG_DELAY
	{ 0x80 , 0x01 },  //
	{ 0x81 , 0x58 },  //0x58:HPgain0dB_ECOen
#ifdef YDA165_USED_REG_NOISEGATE
	{ 0x82 , 0x05 },  //0x0d:powerlimit_off
	{ 0x83 , 0xE6 },  //
	//{ 0x84 , 0x05 },  //0x02:IN1inputgain-3dB_IN2gain4.5dB
#else
	{ 0x82 , 0x0D },  //0x0d:powerlimit_off
	{ 0x83 , 0x06 },  //0x06:Spk24dB_Non-clip1%_NGoff
#endif
	{ 0x84 , 0x02 },  //0x02:IN1inputgain-3dB_IN2gain0dB
	{ AMP_REGISTER_DELAY , 14 },
	{ 0x85 , 0x1A },  //0x1A:SPATT-5dB
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x87 , 0x10 },  //
#else
	{ 0x80 , 0x01 },  //
	{ 0x81 , 0x58 },  //0x58:HPgain0dB_ECOen
	{ 0x82 , 0x0D },  //0x0d:powerlimit_off
	{ 0x83 , 0x06 },  //0x06:Spk24dB_Non-clip1%_NGoff
	{ 0x84 , 0x02 },  //0x02:IN1inputgain-3dB_IN2gain0dB
	{ 0x85 , 0x1A },  //0x1A:SPATT-5dB
	{ 0x86 , 0x19 },  //0x19:HPATT-3dB
	{ 0x87 , 0x10 },  //
#endif
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY yda165_register_type amp_headset_and_speaker_stereo_path[REG_COUNT] = {
#ifdef YDA165_USED_REG_DELAY
	{ 0x80 , 0x01 },  //
	{ 0x81 , 0x58 },  //0x58:HPgain0dB_ECOen
#ifdef YDA165_USED_REG_NOISEGATE
	{ 0x82 , 0x05 },  //0x0d:powerlimit_off
	{ 0x83 , 0xE6 },  //
	//{ 0x84 , 0x05 },  //0x02:IN1inputgain-3dB_IN2gain4.5dB
#else
	{ 0x82 , 0x0D },  //0x0d:powerlimit_off
	{ 0x83 , 0x06 },  //0x06:Spk24dB_Non-clip1%_NGoff
#endif
	{ 0x84 , 0x32 },  //0x32:IN1inputgain1.5dB_IN2gain0dB
	{ AMP_REGISTER_DELAY , 14 },
	{ 0x85 , 0x1A },  //0x1A:SPATT-5dB
	{ 0x86 , 0x19 },  //0x19:HPATT-3dB
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x87 , 0x22 },  //
#else
	{ 0x80 , 0x01 },  //
	{ 0x81 , 0x58 },  //0x58:HPgain0dB_ECOen
	{ 0x82 , 0x0D },  //0x0d:powerlimit_off
	{ 0x83 , 0x06 },  //0x06:Spk24dB_Non-clip1%_NGoff
	{ 0x84 , 0x32 },  //0x32:IN1inputgain1.5dB_IN2gain0dB
	{ 0x85 , 0x1A },  //0x1A:SPATT-5dB
	{ 0x86 , 0x19 },  //0x19:HPATT-3dB
	{ 0x87 , 0x22 },  //
#endif
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY yda165_register_type amp_none_path[REG_COUNT] = {
	{ 0x80 , 0x81 },
	{ 0x81 , 0x10 },
	{ 0x82 , 0x0D },
	{ 0x83 , 0x00 },
	{ 0x84 , 0x00 },
	{ 0x85 , 0x80 },
	{ 0x86 , 0x00 },
	{ 0x87 , 0x00 },
	{ 0x88 , 0x00 },
	{ AMP_REGISTER_END   ,  0 },
};

static const yda165_register_type *amp_sequence_path[AMP_PATH_MAX] ={
	amp_none_path,                        // AMP_PATH_NONE
	NULL,                                 // AMP_PATH_HANDSET
	amp_headset_stereo_path,              // AMP_PATH_HEADSET
	amp_speaker_stereo_path,              // AMP_PATH_SPEAKER
	amp_headset_and_speaker_stereo_path,  // AMP_PATH_HEADSET_SPEAKER
	amp_headset_stereo_path,              // AMP_PATH_HEADSET_NOMIC
	NULL,                                 // AMP_PATH_MAINMIC
	NULL,                                 // AMP_PATH_EARMIC
};

/**
 * yda165_write - Sets register in YDA165
 * @param yda165: yda165 structure pointer passed by client
 * @param reg: register address
 * @param value: buffer values to be written
 * @param num_bytes: n bytes to write
 *
 * @returns result of the operation.
 */
static int yda165_write(struct snddev_yda165 *yda165, u8 reg, u8 *value,
							unsigned num_bytes)
{
#ifndef YDA165_FAST_I2C
	int ret, i;
#else
	int ret;
#endif
	struct i2c_msg *msg;
	u8 data[num_bytes + 1];
	u8 mask_value[num_bytes];

	if (yda165 == NULL)
		yda165 = &yda165_modules;

#ifndef YDA165_FAST_I2C
	mutex_lock(&yda165->xfer_lock);

	for (i = 0; i < num_bytes; i++)
		mask_value[i] = value[num_bytes-1-i];
#else
	mask_value[0] = value[0];
#endif

	msg = &yda165->xfer_msg[0];
	msg->addr = yda165->client->addr;
	msg->flags = 0;
	msg->len = num_bytes + 1;
	msg->buf = data;
	data[0] = reg;
	memcpy(data+1, mask_value, num_bytes);

	ret = i2c_transfer(yda165->client->adapter, yda165->xfer_msg, 1);
	/* Try again if the write fails */
	if (ret != 1)
	{
		ret = i2c_transfer(yda165->client->adapter, yda165->xfer_msg, 1);
		if (ret != 1)
		{
			pr_err("failed to write the yda165 device\n");
		}
	}

#ifndef YDA165_FAST_I2C
	mutex_unlock(&yda165->xfer_lock);
#endif

	return ret;
}

#ifdef YDA165_I2C_TEST
/**
 * yda165_read - Reads registers in YDA165
 * @param yda165: yda165 structure pointer passed by client
 * @param reg: register address
 * @param value: i2c read of the register to be stored
 * @param num_bytes: n bytes to read.
 * @param mask: bit mask concerning its register
 *
 * @returns result of the operation.
*/
static int yda165_read(struct snddev_yda165 *yda165, u8 reg, u8 *value, unsigned num_bytes)
{
	int ret, i;
	u8 data[num_bytes];
	struct i2c_msg *msg;

	if (yda165 == NULL)
		yda165 = &yda165_modules;

#ifndef YDA165_FAST_I2C
	mutex_lock(&yda165->xfer_lock);
#endif

	msg = &yda165->xfer_msg[0];
	msg->addr = yda165->client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = &reg;

	msg = &yda165->xfer_msg[1];
	msg->addr = yda165->client->addr;
	msg->flags = I2C_M_RD;
	msg->len = num_bytes;
	msg->buf = data;

	ret = i2c_transfer(yda165->client->adapter, yda165->xfer_msg, 2);
	/* Try again if read fails first time */
	if (ret != 2)
	{
		ret = i2c_transfer(yda165->client->adapter, yda165->xfer_msg, 2);
		if (ret != 2)
		{
			pr_err("failed to read yda165 register\n");
		}
	}
	if (ret == 2)
	{
		for (i = 0; i < num_bytes; i++)
		{
			value[i] = data[num_bytes-1-i];
		}
	}

#ifndef YDA165_FAST_I2C
	mutex_unlock(&yda165->xfer_lock);
#endif

	return ret;
}
#endif

#ifdef CONFIG_KTTECH_SOUND_TUNE
static int yda165_apply_register(void *data, size_t size)
{
#ifdef YDA165_DEGUB_MSG
	int i = 0;
#endif
	int nCMDCount = 0;
	yda165_register_type *pFirstData = (yda165_register_type*)data;
	yda165_register_type *pCurData = (yda165_register_type*)data;
	AMP_PATH_TYPE_E path = (AMP_PATH_TYPE_E)pFirstData->reg;
	yda165_register_type *amp_regs = NULL;

	if (path > AMP_PATH_NONE && path < AMP_PATH_MAINMIC) // RX Devices
	{
		APM_INFO("Unknown Device : %d\n", path);
		return -1;
	}

	amp_regs = (yda165_register_type *)amp_sequence_path[path];
	if (amp_regs == NULL)
	{
		APM_INFO("not support path on yda165 path = %d\n", path);
		return -1;
	}

	nCMDCount = size / sizeof(yda165_register_type);  
	APM_INFO("Path = %d, Register Count = %d\n", path, nCMDCount);

#ifdef YDA165_DEGUB_MSG	
	for (i = 0 ; i < nCMDCount ; i ++)
	{
		APM_INFO("CMD = [0X%.2x] , [0X%.2x] \n" , pCurData->reg , pCurData->value);
		pCurData = pCurData + 1;
 	}
#endif

	pCurData = pFirstData + 1;
	memcpy(amp_regs, pCurData, size - sizeof(yda165_register_type));

	return path;
}

void yda165_tuning(void *data, size_t size)
{
	AMP_PATH_TYPE_E path = AMP_PATH_NONE;

	if (data == NULL || size == 0 || size > (sizeof(yda165_register_type) * AMP_REGISTER_MAX))
	{
		APM_INFO("invalid prarameters data = %d, size = %d \n", (int)data, size);
		return;	
	}

	path = yda165_apply_register(data, size);

	// on sequence이고 설정할 codec path 사용중인 경우 인 경우
	if (m_curr_path != AMP_PATH_NONE)
	{
		yda165_enable(m_curr_path);
	}
}
EXPORT_SYMBOL(yda165_tuning);
#endif

void yda165_set_register(yda165_register_type *amp_regs)
{
	uint32_t loop = 0;
	struct snddev_yda165 *yda165 = &yda165_modules;

	while (amp_regs[loop].reg != AMP_REGISTER_END)
	{
		if (amp_regs[loop].reg == AMP_REGISTER_DELAY)
		{
			msleep(amp_regs[loop].value);
		}
		else
		{
			if (yda165->noisegate_enabled == 0 && amp_regs[loop].reg == 0x83)
			{
				u8 value = 0x06; // Reg : 0x83, Val : 0x26 (Enable) / 0x06 (Disable)
				yda165_write(yda165, amp_regs[loop].reg , (u8 *)&value, 1);
				APM_INFO("Disable Noisegate\n");
			}
#ifdef YDA165_USED_REG_NONCLIP
			else if (amp_regs[loop].reg == 0x82)
			{
				u8 value = 0x0D;
				if(msm_device_get_isvoice() == 1)
				{
					// Voice
					value = 0x0C;
					APM_INFO("Non-Clip2 Setting Voice Source\n");
				}
				else
				{
					// Music
					value = 0x0E;
					APM_INFO("Non-Clip2 Setting Music Source\n");
				}
				yda165_write(yda165, amp_regs[loop].reg , (u8 *)&value, 1);
			}
#endif
			else
			{
				yda165_write(yda165, amp_regs[loop].reg , (u8 *)&amp_regs[loop].value, 1);
			}
#ifdef YDA165_DEGUB_MSG
			APM_INFO("reg 0x%x , value 0x%x",  amp_regs[loop].reg, amp_regs[loop].value);
#endif
		}
		loop++;
	}

	return;
}
EXPORT_SYMBOL(yda165_set_register);

void yda165_reset(void)
{
	const yda165_register_type *amp_regs = amp_sequence_path[AMP_PATH_NONE];

	if (amp_regs == NULL)
	{
		APM_INFO("Reset Register is Null!!!\n");
		return;
	}
	APM_INFO("Reset Register\n");

	yda165_set_register((yda165_register_type *)amp_regs);
}
EXPORT_SYMBOL(yda165_reset);

void yda165_enable_amplifier(void)
{
	struct snddev_yda165 *yda165 = &yda165_modules;
	u8 addr = 0, value = 0;

	if (m_curr_path > AMP_PATH_NONE && m_curr_path < AMP_PATH_MAINMIC) // RX Devices
	{
		APM_INFO("Enable Amplifier - RX Device : %d, SeakerSw : %d\n", 
					m_curr_path, gpio_get_value(YDA165_SPK_SW_GPIO));

		if (m_curr_path == AMP_PATH_HANDSET)
		{
			APM_INFO("Handset device is ignored.\n");
			return;
		}
		else
		{
#if 0 // Speaker Switch의 경우 Volume Contorl과 겹치기 때문에 여기서 사용치 않는다.
			if (gpio_get_value(YDA165_SPK_SW_GPIO) != 0)
			{
				APM_INFO("##### Enable Speaker Switch : %d\n", m_curr_path);
				gpio_set_value(YDA165_SPK_SW_GPIO, 0);
			}
#endif
		}
	}
	else // Tx Devices
	{
		APM_INFO("Enable Amplifier - Unknown Device : %d\n", m_curr_path);
		return;
	}

#ifdef YDA165_CHECK_AMP_STATUS
	if (atomic_read(&yda165->amp_enabled) == 1)
	{
		APM_INFO("The device(%d) is already open\n", m_curr_path);
		return;
	}
#endif

#ifdef YDA165_USED_REG_0X87
	// * 0x87
	// - setting ON/OFF of signal mixing function of headphone/speaker.
	// - Set SP_AMIX/SP_BMIX/HP_AMIX/HP_BMIX register (0x87) to "0xxx".
	addr = 0x87;
#endif /*YDA165_USED_REG_0X87*/
	if (m_curr_path == AMP_PATH_HEADSET || m_curr_path == AMP_PATH_HEADSET_NOMIC)
	{
#ifdef YDA165_USED_REG_0X87
#ifndef YDA165_USED_REG_0X86 // 0x87 -> 0x86
		value = 0x02;
#else
		// * 0x86
		// - setting the headphone amplifier attenuator.
		// - Set HPSVOFF/HPZCSOFF/0/HPATT
		// - 0x00 : Mute, 0x19 : Soft Volume
		addr = 0x86;
		value = 0x19;
#endif
#else
		// Do Nothing
#endif /*YDA165_USED_REG_0X87*/
	}
	else if (m_curr_path == AMP_PATH_SPEAKER)
	{
#ifdef YDA165_USED_REG_0X87
		value = 0x10; //0x20;
#else
		// Do Nothing
#endif /*YDA165_USED_REG_0X87*/
	}
	else if (m_curr_path == AMP_PATH_HEADSET_SPEAKER)
	{
#ifdef YDA165_USED_REG_0X87
#ifdef YDA165_USED_REG_0X86 // 0x87 -> 0x86
		addr = 0x86;
		value = 0x19;
		yda165_write(yda165, addr, &value, 1);
#endif
		addr = 0x87;
		value = 0x22;
#else
		// Do Nothing
#endif /*YDA165_USED_REG_0X87*/
	}
	else
	{
		APM_INFO("Enable Amplifier - Unsupoorted Device : %d\n", m_curr_path);
		return;
	}

	if (addr != 0)
	{
		yda165_write(yda165, addr, &value, 1);
		APM_INFO("Enable Amplifier - Register(0x%x,0x%x)\n", addr, value);
#ifdef YDA165_CHECK_AMP_STATUS
		atomic_set(&yda165->amp_enabled, 1);
#endif
	}

	return;
}
EXPORT_SYMBOL(yda165_enable_amplifier);

void yda165_disable_amplifier(void)
{
	struct snddev_yda165 *yda165 = &yda165_modules;
	u8 addr = 0, value = 0;

	if (m_curr_path > AMP_PATH_NONE && m_curr_path < AMP_PATH_MAINMIC) // RX Devices
	{
		APM_INFO("Disable Amplifier - RX Device : %d, SeakerSw : %d\n", 
					m_curr_path, gpio_get_value(YDA165_SPK_SW_GPIO));

		if (m_curr_path == AMP_PATH_HANDSET)
		{
			APM_INFO("Handset device is ignored.\n");
			return;
		}
		else
		{
#if 0 // Speaker Switch의 경우 Volume Contorl과 겹치기 때문에 여기서 사용치 않는다.
			if (gpio_get_value(YDA165_SPK_SW_GPIO) != 0)
			{
				APM_INFO("##### Enable Speaker Switch : %d\n", m_curr_path);
				gpio_set_value(YDA165_SPK_SW_GPIO, 0);
			}
#endif
		}
	}
	else
	{
		APM_INFO("Disable Amplifier - Unknown Device : %d\n", m_curr_path);
		return;
	}

#ifdef YDA165_CHECK_AMP_STATUS
	if (atomic_read(&yda165->amp_enabled) == 0)
	{
		APM_INFO("The device(%d) is already closed\n", m_curr_path);
		return;
	}
#endif

	if (m_curr_path == AMP_PATH_HEADSET || m_curr_path == AMP_PATH_HEADSET_NOMIC)
	{
#ifdef YDA165_USED_REG_0X87
#ifndef YDA165_USED_REG_0X86 // 0x87 -> 0x86
		addr = 0x87;
#else
		addr = 0x86;
#endif
		value = 0x00;
#else
		// Do Nothing
#endif /*YDA165_USED_REG_0X87*/
	}
	else if (m_curr_path == AMP_PATH_SPEAKER)
	{
#ifdef YDA165_USED_REG_0X87
		// Set SP_AMIX/SP_BMIX/HP_AMIX/HP_BMIX register (0x87) to "0".
		addr = 0x87;
		value = 0x00;
#else
		// Do Nothing
#endif /*YDA165_USED_REG_0X87*/
	}
	else if (m_curr_path == AMP_PATH_HEADSET_SPEAKER)
	{
#ifdef YDA165_USED_REG_0X87
#ifdef YDA165_USED_REG_0X86 // 0x87 -> 0x86
		addr = 0x86;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);
#endif
		// Set SP_AMIX/SP_BMIX/HP_AMIX/HP_BMIX register (0x87) to "0".
		addr = 0x87;
		value = 0x00;
#else
		// Do Nothing
#endif /*YDA165_USED_REG_0X87*/
	}
	else
	{
		APM_INFO("Disable Amplifier - Unsupported Device : %d\n", m_curr_path);
		return;
	}

	if (addr != 0)
	{
		yda165_write(yda165, addr, &value, 1);
		APM_INFO("Disable Amplifier - Register(0x%x,0x%x)\n", addr, value);
#ifdef YDA165_CHECK_AMP_STATUS
		atomic_set(&yda165->amp_enabled, 0);
#endif
	}

	return;
}
EXPORT_SYMBOL(yda165_disable_amplifier);

/**
 * yda165_enable - Enable path in YDA165
 * @param path: amp path
 *
 * @returns void
*/
void yda165_enable(AMP_PATH_TYPE_E path)
{
	const yda165_register_type *amp_regs = amp_sequence_path[path];
	struct snddev_yda165 *yda165 = &yda165_modules;
#ifdef YDA165_FORCEUSED_PATH
	int forceusedpath = 0;
#endif

	if (path <= AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("unknown path on yda165 path = %d -> %d\n", m_curr_path, path);
		return;
	}
	if (path == AMP_PATH_MAINMIC || path == AMP_PATH_EARMIC)
	{
		APM_INFO("not support path on yda165 tx's path = %d -> %d\n", m_curr_path, path);
		return;
	}
	APM_INFO("Enable path = %d -> %d\n", m_curr_path, path);

	mutex_lock(&yda165->path_lock);
	if (m_curr_path == path)
	{
#ifdef YDA165_ALWAYSON_HEADSET
		int headset_status = pm8058_get_headset_status();
		if (headset_status > 0 && path == AMP_PATH_HEADSET)
		{
#if 0 // Error : Reset Register //def YDA165_USED_REG_0X86_MUTE
			u8 addr = 0, value = 0;
			addr = 0x86;
			value = 0x19;
			yda165_write(yda165, addr, &value, 1);

			addr = 0x87;
			value = 0x02;
			yda165_write(yda165, addr, &value, 1);
#endif

			APM_INFO("Return!!! Headset(%d) used path : %d -> %d.\n", headset_status, m_curr_path, path);
			return;
		}
#endif

		mutex_unlock(&yda165->path_lock);
		APM_INFO("Return!!! current vs input path = %d vs %d\n", m_curr_path, path);
		return;
	}
	mutex_unlock(&yda165->path_lock);

#ifdef YDA165_FORCEUSED_PATH
	forceusedpath = atomic_read(&yda165->forceused_path);
	if (forceusedpath > 0)
	{
		if (m_curr_path == AMP_PATH_HEADSET_SPEAKER || m_curr_path == AMP_PATH_NONE)
		{
			APM_INFO("Set force used path : %d -> %d -> %d\n", m_curr_path, path, forceusedpath);
			path = forceusedpath;
		}
		else
		{
			if (path != AMP_PATH_HEADSET_SPEAKER)
			{
				APM_INFO("Return!!! force used path : %d -> %d -> %d\n",m_curr_path, path, forceusedpath);
				return;
			}
		}
	}
#endif

	mutex_lock(&yda165->path_lock);
	m_curr_path = path;
	mutex_unlock(&yda165->path_lock);

	if (amp_regs != NULL)
	{
		yda165_set_register((yda165_register_type *)amp_regs);
	}

	if (path > AMP_PATH_NONE && path < AMP_PATH_MAINMIC) // RX Devices
	{
		if (path == AMP_PATH_HANDSET)
		{
			APM_INFO("Disable Speaker Switch : %d\n", path);
			if (gpio_get_value(YDA165_SPK_SW_GPIO) != 1)
				gpio_set_value(YDA165_SPK_SW_GPIO, 1);
		}
		else
		{
			APM_INFO("Enable Speaker Switch : %d\n", path);
			if (gpio_get_value(YDA165_SPK_SW_GPIO) != 0)
				gpio_set_value(YDA165_SPK_SW_GPIO, 0);

#ifdef YDA165_CHECK_AMP_STATUS
			APM_DBG("Enable Amplifier : %d\n", path);
			atomic_set(&yda165->amp_enabled, 1);
#endif
		}
	}

	return;
}
EXPORT_SYMBOL(yda165_enable);

/**
 * yda165_disable - Disable path in YDA165
 * @param path: amp path
 *
 * @returns void
*/
void yda165_disable(AMP_PATH_TYPE_E path)
{
	struct snddev_yda165 *yda165 = &yda165_modules;
	u8 addr = 0, value = 0;
#ifdef YDA165_FORCEUSED_PATH
	int forceusedpath = 0;
#endif
#ifdef YDA165_ALWAYSON_HEADSET
	int headset_status = 0;
#endif

	if (path <= AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("unknown path on yda165 path = %d -> %d\n", m_curr_path, path);
		return;
	}
	if (path == AMP_PATH_MAINMIC || path == AMP_PATH_EARMIC)
	{
		APM_INFO("not support path on yda165 tx's path = %d -> %d\n", m_curr_path, path);
		return;
	}
	APM_INFO("Disable path = %d -> %d\n", m_curr_path, path);

#ifdef YDA165_FORCEUSED_PATH
	forceusedpath = atomic_read(&yda165->forceused_path);
	if (forceusedpath > 0)
	{
		if (path == forceusedpath)
		{
			APM_INFO("Return!!! force used path : %d -> %d -> %d\n", m_curr_path, path, forceusedpath);
			return;
		}
	}
#endif

#ifdef YDA165_ALWAYSON_HEADSET
	headset_status = pm8058_get_headset_status();
	if (headset_status > 0 && path == AMP_PATH_HEADSET)
	{
#if 0 // Error : Reset Register //def YDA165_USED_REG_0X86_MUTE
		addr = 0x86;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);
#endif

		APM_INFO("Return!!! Headset(%d) used path : %d -> %d.\n", headset_status, m_curr_path, path);
		return;
	}
#else
#ifdef YDA165_USED_REG_0X85_MUTE // Point (1)
	if (path == AMP_PATH_SPEAKER)
	{
		addr = 0x85;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);

		msleep(10);
	}
#endif
#ifdef YDA165_USED_REG_0X86_MUTE // Point (1)
	if (path == AMP_PATH_HEADSET)
	{
		addr = 0x86;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);

		msleep(10);
	}
#endif
#if defined(YDA165_USED_REG_0X85_MUTE) && defined(YDA165_USED_REG_0X86_MUTE) // Point (1)
	if (path == AMP_PATH_HEADSET_SPEAKER)
	{
		addr = 0x85;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);

		addr = 0x86;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);

		msleep(10);
	}
#endif
#endif /*YDA165_ALWAYSON_HEADSET*/

	mutex_lock(&yda165->path_lock);
	m_prev_path = m_curr_path;
	m_curr_path = AMP_PATH_NONE;
	mutex_unlock(&yda165->path_lock);

	if (gpio_get_value(YDA165_SPK_SW_GPIO) != 0)
	{
		APM_INFO("Enable Speaker Switch : %d\n", path);
		gpio_set_value(YDA165_SPK_SW_GPIO, 0);
	}

	if (path == AMP_PATH_HEADSET
		|| path == AMP_PATH_SPEAKER
		|| path == AMP_PATH_HEADSET_SPEAKER)
	{
		// Set SP_AMIX/SP_BMIX/HP_AMIX/HP_BMIX register (0x87) to "0".
		addr = 0x87;
		value = 0x00;
		yda165_write(yda165, addr, &value, 1);

#if 0 //def YDA165_USED_REG_0X85_MUTE // Point (2)
		if (path == AMP_PATH_SPEAKER)
		{
			addr = 0x85;
			value = 0x00;
			yda165_write(yda165, addr, &value, 1);
		}
#endif
#if 0 //def YDA165_USED_REG_0X86_MUTE // Point (2)
		if (path == AMP_PATH_HEADSET)
		{
			addr = 0x86;
			value = 0x00;
			yda165_write(yda165, addr, &value, 1);
		}
#endif
#if 0 //defined(YDA165_USED_REG_0X85_MUTE) && defined(YDA165_USED_REG_0X86_MUTE) // Point (2)
		if (path == AMP_PATH_HEADSET_SPEAKER)
		{
			addr = 0x85;
			value = 0x00;
			yda165_write(yda165, addr, &value, 1);

			addr = 0x86;
			value = 0x00;
			yda165_write(yda165, addr, &value, 1);
		}
#endif

#if 0 // 20120322 by ssgun - don't used : power shutdown sequence
		// Set SRST register (0x80) to "1".
		// This causes all the registers to be set to default values.
		addr = 0x80;
		value = 0x80;
		yda165_write(yda165, addr, &value, 1);
#endif

#ifdef YDA165_CHECK_AMP_STATUS
		APM_DBG("Disable Amplifier : %d\n", path);
		atomic_set(&yda165->amp_enabled, 0);
#endif
	}

	return;
}
EXPORT_SYMBOL(yda165_disable);

#ifdef YDA165_FORCEUSED_PATH
void yda165_setForceUse(AMP_PATH_TYPE_E path)
{
	struct snddev_yda165 *yda165 = &yda165_modules;

	if (path <= AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("unknown path on yda165 path = %d -> %d\n", m_curr_path, path);
		return;
	}
	if (path == AMP_PATH_MAINMIC || path == AMP_PATH_EARMIC)
	{
		APM_INFO("not support path on yda165 tx's path = %d -> %d\n", m_curr_path, path);
		return;
	}

	APM_INFO("Set Force Used path = %d -> %d\n", m_curr_path, path);

	atomic_set(&yda165->forceused_path, path);

	return;
}
EXPORT_SYMBOL(yda165_setForceUse);
#endif

static long yda165_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct snddev_yda165 *yda165 = file->private_data;
	int rc = 0;

	pr_debug("%s\n", __func__);

	switch (cmd) {
		case AUDIO_START:
		{
			APM_INFO("AUDIO_START\n");
			yda165_enable_amplifier();
		}
		break;

		case AUDIO_STOP:
		case AUDIO_PAUSE:
		{
			APM_INFO("AUDIO_STOP\n");
			yda165_disable_amplifier();
		}
		break;

		case AUDIO_FLUSH:
		{
			AMP_PATH_TYPE_E path = m_curr_path;
			APM_INFO("AUDIO_FLUSH\n");

			// TODO: it will set original register when it is finished reset operation
			yda165_disable(m_curr_path);
			msleep(10); // 10msec
			yda165_enable(path);
		}
		break;

		case AUDIO_SET_CONFIG:
		{
			struct msm_audio_config config;

			if (copy_from_user(&config, (void *) arg, sizeof(config)))
			{
				rc = -EFAULT;
				pr_err("%s: AUDIO_SET_CONFIG : Failed copy from user\n", __func__);
				break;
			}
			APM_INFO("AUDIO_SET_CONFIG : Type %d\n", config.type);

			mutex_lock(&yda165->lock);
			if (config.type == 0) {
				yda165->noisegate_enabled = config.channel_count;
				APM_INFO("Set Noisegate %s\n", yda165->noisegate_enabled ? "ON" : "OFF");
			} else if (config.type == 1) {
				yda165->cal_type = config.channel_count;
				APM_INFO("Set Cal Type %d\n", yda165->cal_type);
			} else if (config.type == 2) {
				yda165->spksw_enabled = config.channel_count;
				APM_INFO("Set Speaker Switch %s\n", yda165->spksw_enabled ? "ON" : "OFF");

				APM_INFO("Current Device = %d, Speaker Switch State = %d\n", 
							m_curr_path, gpio_get_value(YDA165_SPK_SW_GPIO));
				if (m_curr_path > AMP_PATH_NONE && m_curr_path < AMP_PATH_MAINMIC) // RX Devices
				{
					if (yda165->spksw_enabled == 1)
					{
						if (m_curr_path == AMP_PATH_HANDSET)
						{
							if (gpio_get_value(YDA165_SPK_SW_GPIO) != 1)
							{
								APM_INFO("Current Path = %d -> Disable Speaker Switch\n",
										m_curr_path);
								gpio_set_value(YDA165_SPK_SW_GPIO, 1);
							}
						}
						else
						{
							if (gpio_get_value(YDA165_SPK_SW_GPIO) != 0)
							{
								APM_INFO("Current Path = %d -> Enable Speaker Switch\n",
										m_curr_path);
								gpio_set_value(YDA165_SPK_SW_GPIO, 0);
							}
						}
					}
					else
					{
						if (m_curr_path != AMP_PATH_HANDSET)
						{
							if (gpio_get_value(YDA165_SPK_SW_GPIO) != 1)
							{
								APM_INFO("Current Path = %d -> Disable Speaker Switch\n",
										m_curr_path);
								gpio_set_value(YDA165_SPK_SW_GPIO, 1);
							}
						}
					}
				}
			}
			mutex_unlock(&yda165->lock);
		}
		break;

		case AUDIO_GET_CONFIG:
		{
			struct msm_audio_config config;

			memset(&config, 0x00, sizeof(struct msm_audio_config));
			config.buffer_size = yda165->noisegate_enabled; // Noisegate
			config.buffer_count = yda165->cal_type; // Cal Type
			config.channel_count = yda165->spksw_enabled; // Speaker Switch
			config.sample_rate = (uint32_t)atomic_read(&yda165->amp_enabled); // Amp State
			config.type = (uint32_t)yda165->suspend_poweroff;

			if (copy_to_user((void *) arg, &config, sizeof(config)))
			{
				rc = -EFAULT;
				pr_err("%s: AUDIO_GET_CONFIG : Failed copy to user\n", __func__);
				break;
			}
			APM_INFO("AUDIO_GET_CONFIG\n");
		}
		break;

		case AUDIO_SET_EQ:
		{
			struct msm_audio_config config;

			if (copy_from_user(&config, (void *) arg, sizeof(config)))
			{
				rc = -EFAULT;
				pr_err("%s: AUDIO_SET_EQ : Failed copy from user\n", __func__);
				break;
			}
			APM_INFO("AUDIO_SET_EQ\n");

			mutex_lock(&yda165->lock);
			yda165->eq_type = config.type;
			mutex_unlock(&yda165->lock);
			APM_INFO("Set EQ type = %d\n", yda165->eq_type);
		}
		break;

		case AUDIO_SET_MUTE:
		{
			struct msm_audio_config config;

			if (copy_from_user(&config, (void *) arg, sizeof(config)))
			{
				rc = -EFAULT;
				pr_err("%s: AUDIO_SET_MUTE : Failed copy from user\n", __func__);
				break;
			}
			APM_INFO("AUDIO_SET_MUTE\n");

			mutex_lock(&yda165->lock);
			yda165->mute_enabled = config.type;
			mutex_unlock(&yda165->lock);
			APM_INFO("Set Mute type = %d\n", yda165->mute_enabled);
#if 0 // alsa api
			if (yda165->mute_enabled == 1)
				msm_set_voice_tx_mute(1);
			else
				msm_set_voice_tx_mute(0);
#else
			// TODO: Set AMP YDA165 Mute Register
#endif
		 }
		 break;

		case AUDIO_SET_VOLUME:
		{
			struct msm_audio_config config;

			if (copy_from_user(&config, (void *) arg, sizeof(config))) {
				rc = -EFAULT;
				pr_err("%s: AUDIO_SET_VOLUME : Failed copy from user\n", __func__);
				break;
			}
			APM_INFO("AUDIO_SET_VOLUME\n");

			mutex_lock(&yda165->lock);
			yda165->voip_micgain = config.type;
			mutex_unlock(&yda165->lock);
			APM_INFO("Set Voip Mic Gain type = %d\n", yda165->voip_micgain);
#if 0 // alsa api
			if (yda165->voip_micgain > 0)
				msm_set_voice_rx_vol(yda165->voip_micgain);
#else
			// TODO: Set AMP YDA165 Volume Register
#endif
		}
		break;

		default:
		rc = -EINVAL;
		pr_err("%s: Unsupported Command %d\n", __func__, cmd);
		break;
	}

	return rc;
}

static int yda165_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	file->private_data = &yda165_modules;
	return 0;
}

static int yda165_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static struct file_operations yda165_dev_fops = {
	.owner      = THIS_MODULE,
	.open		= yda165_open,
	.release	= yda165_release,
	.unlocked_ioctl = yda165_ioctl,
};

struct miscdevice yda165_control_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "snddev_yda165",
	.fops   = &yda165_dev_fops,
};

static int yda165_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct yda165_platform_data *pdata = client->dev.platform_data;
	struct snddev_yda165 *yda165;
	int status;

	APM_INFO("\n");

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0)
	{
		dev_err(&client->dev, "can't talk I2C?\n");
		return -EIO;
	}

	if (pdata->yda165_setup != NULL)
	{
		status = pdata->yda165_setup(&client->dev);
		if (status < 0)
		{
			pr_err("yda165_probe: yda165 setup power failed\n");
			return status;
		}
	}

	yda165 = &yda165_modules;
	yda165->client = client;
	strlcpy(yda165->client->name, id->name, sizeof(yda165->client->name));
#ifndef YDA165_FAST_I2C
	mutex_init(&yda165->xfer_lock);
#endif
	mutex_init(&yda165->path_lock);
	mutex_init(&yda165->lock);
	yda165->cal_type = 0;
	yda165->eq_type = 0;
	yda165->mute_enabled = 0;
	yda165->voip_micgain = 0;
#ifdef CONFIG_KTTECH_MODEL_O6
#ifdef YDA165_USED_REG_NOISEGATE
	yda165->noisegate_enabled = 1;
#else
	yda165->noisegate_enabled = 0;
#endif
#else // O4
	yda165->noisegate_enabled = 1;
#endif /*CONFIG_KTTECH_MODEL_O6*/
	yda165->spksw_enabled = 0;
#ifdef YDA165_CHECK_AMP_STATUS
	atomic_set(&yda165->amp_enabled, 0);
#endif /*YDA165_CHECK_AMP_STATUS*/
#ifdef YDA165_FORCEUSED_PATH
	atomic_set(&yda165->forceused_path, 0);
#endif /*YDA165_FORCEUSED_PATH*/
	yda165->suspend_poweroff = 0;

	status = misc_register(&yda165_control_device);
	if (status)
	{
		pr_err("yda165_probe: yda165_control_device register failed\n");
		return status;
	}

#ifdef YDA165_I2C_TEST
	{
		u8 buf = 0xff;
		yda165_read(yda165, YDA165_RESET_REG, &buf, 1);
	}
#endif

	return 0;
}

#ifdef CONFIG_PM
static int yda165_suspend(struct i2c_client *client, pm_message_t mesg)
{
	if (m_curr_path == AMP_PATH_NONE)
	{
		struct yda165_platform_data *pdata;
		struct snddev_yda165 *yda165;

		pdata = client->dev.platform_data;
		yda165 = &yda165_modules;

		if (pdata->yda165_shutdown != NULL)
		{
			pdata->yda165_shutdown(&client->dev);
			yda165->suspend_poweroff = 1;
			APM_INFO("Current Path : %d\n", m_curr_path);
		}
	}

    return 0;
}

static int yda165_resume(struct i2c_client *client)
{
	struct yda165_platform_data *pdata;
	struct snddev_yda165 *yda165;
	int status;

	yda165 = &yda165_modules;

	if (yda165->suspend_poweroff == 1)
	{
		pdata = client->dev.platform_data;

		if (pdata->yda165_setup != NULL)
		{
			status = pdata->yda165_setup(&client->dev);
			if (status < 0)
			{
				pr_err("%s : %d, fail PowerOn = %d\n", __func__, m_curr_path, status);
				return status;
			}
			yda165->suspend_poweroff = 0;
			APM_INFO("Current Path : %d\n", m_curr_path);
		}
	}
	return 0;
}
#endif

static int __devexit yda165_remove(struct i2c_client *client)
{
	struct yda165_platform_data *pdata;

	pdata = client->dev.platform_data;
	yda165_modules.client = NULL;

	if (pdata->yda165_shutdown != NULL)
		pdata->yda165_shutdown(&client->dev);

	misc_deregister(&yda165_control_device);

	return 0;
}

static struct i2c_device_id yda165_id_table[] = {
	{"yda165", 0x0},
	{}
};
MODULE_DEVICE_TABLE(i2c, yda165_id_table);

static struct i2c_driver yda165_driver = {
		.driver			= {
			.owner		=	THIS_MODULE,
			.name		= 	"yda165",
		},
		.id_table		=	yda165_id_table,
		.probe			=	yda165_probe,
#ifdef CONFIG_PM
    	.suspend		= yda165_suspend,
    	.resume			= yda165_resume,
#endif
		.remove			=	__devexit_p(yda165_remove),
};

// 20110416 by ssgun - GPIO_CFG_NO_PULL -> GPIO_CFG_PULL_DOWN
#define SPK_SW_CTRL_0 \
	GPIO_CFG(YDA165_SPK_SW_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)

void yda165_init(void)
{
	int rc = 0;

	rc = gpio_tlmm_config(SPK_SW_CTRL_0, GPIO_CFG_ENABLE);
	if (rc)
	{
		APM_ERR("Audio gpio  config failed: %d\n", rc);
		goto fail;
	}

	gpio_set_value(YDA165_SPK_SW_GPIO, 0);
	i2c_add_driver(&yda165_driver);

fail:
    return;
}
EXPORT_SYMBOL(yda165_init);

void yda165_exit(void)
{
	i2c_del_driver(&yda165_driver);
}
EXPORT_SYMBOL(yda165_exit);
