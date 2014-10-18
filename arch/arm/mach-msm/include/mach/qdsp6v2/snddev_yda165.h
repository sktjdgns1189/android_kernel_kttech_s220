/* Copyright (c) 2011, KTTech. All rights reserved.
 *
 * YDA165 Sound Amp Driver
 *
 */
#ifndef __MACH_QDSP6V2_SNDDEV_YDA165_H__
#define __MACH_QDSP6V2_SNDDEV_YDA165_H__

#include <linux/i2c.h>
#include <linux/mfd/msm-adie-codec.h>
#include <mach/qdsp6v2/audio_amp_ctl.h>

//////////////////////////////////////////////////////////////////////
// YDA165 FEATURE
#define YDA165_CHECK_AMP_STATUS // 20110708 by ssgun - check amp status
//#define YDA165_ALWAYSON_HEADSET // 20120321 by ssgun - always on headset
#define YDA165_FAST_I2C

//////////////////////////////////////////////////////////////////////
// YDA165  Register Control
#define AMP_REGISTER_MAX		50
#define AMP_REGISTER_DELAY		0xFE // YDA165 Delay
#define AMP_REGISTER_END		0xFF // YDA165 END 

#ifdef CONFIG_KTTECH_SOUND_TUNE
#define REG_MEMORY	static
#define REG_COUNT	AMP_REGISTER_MAX
#else
#define REG_MEMORY	static const     /* Normally, tables are in ROM  */
#define REG_COUNT
#endif /*CONFIG_KTTECH_SOUND_TUNE*/

// YDA165 Register Format
typedef struct {
	u8 reg;
	u8 value;
} yda165_register_type;

//////////////////////////////////////////////////////////////////////
// YDA165 Device Information
struct snddev_yda165 {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
#ifndef YDA165_FAST_I2C
	struct mutex xfer_lock; // for register
#endif
	struct mutex path_lock; // for path
	struct mutex lock; // for ioctl
	uint32_t cal_type; // voip, video, etc(voice) call
	uint32_t eq_type;
	uint32_t mute_enabled;
	uint32_t voip_micgain;
	uint32_t noisegate_enabled; // Reg : 0x83, Val : 0x26 (Enable) / 0x06 (Disable)
	uint32_t spksw_enabled; // Speaker Switch
#ifdef YDA165_CHECK_AMP_STATUS // 20110708 by ssgun - check amp status
	atomic_t amp_enabled;
#endif
#ifdef YDA165_FORCEUSED_PATH // 20120110 by ssgun - force used path
	atomic_t forceused_path;
#endif
	u8 suspend_poweroff;
};

// YDA165 Platform Data
struct yda165_platform_data {
	int (*yda165_setup) (struct device *dev);
	void (*yda165_shutdown) (struct device *dev);
};

//////////////////////////////////////////////////////////////////////
// YDA165 Function Prototype
void yda165_init(void);
void yda165_exit(void);
void yda165_enable_amplifier(void);
void yda165_disable_amplifier(void);

void yda165_enable(AMP_PATH_TYPE_E path);
void yda165_disable(AMP_PATH_TYPE_E path);
#ifdef CONFIG_KTTECH_SOUND_TUNE
void yda165_tuning(void *data, size_t size);
#endif /*CONFIG_KTTECH_SOUND_TUNE*/

#ifdef YDA165_FORCEUSED_PATH // 20120110 by ssgun - force used path
void yda165_setForceUse(AMP_PATH_TYPE_E path);
#endif /*YDA165_FORCEUSED_PATH*/

#endif /*__MACH_QDSP6V2_SNDDEV_YDA165_H__*/
