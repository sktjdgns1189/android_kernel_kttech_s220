/* Copyright (c) 2011, KTTech. All rights reserved.
 * snddev_wm8993.c -- WM8993 audio driver
 *
 *
 * Author: Gun Song
 * 
 */

#ifndef __MACH_QDSP6V2_SNDDEV_WM8993_H__
#define __MACH_QDSP6V2_SNDDEV_WM8993_H__

#include <linux/i2c.h>
#include <linux/mfd/msm-adie-codec.h>
#include <mach/qdsp6v2/audio_amp_ctl.h>

//////////////////////////////////////////////////////////////////////
// WM8993 FEATURE
#define WM8993_CHECK_RESET // Reset 중복 방지를 위해 체크한다.
#define WM8993_CODEC_FAST_CALL_REGISTER
#define WM8993_DIRECT_AMP_CONTROL
//#define WM8993_DEFAULT_REGISTER_RESTORE
#define WM8993_WHITE_NOIZE_ZERO
#define WM8993_POP_NOIZE_ZERO
//#define WM8993_STARTUPDOWN_CONTROL
//#define WM8993_STARTUPDOWN_CONTROL_CASE01 // reference start-up/down scenario
//#define WM8993_STARTUPDOWN_CONTROL_CASE02 // master bias start-up/down scenario

//////////////////////////////////////////////////////////////////////
// WM8993 Register Control
#define AMP_REGISTER_MAX		120
#define AMP_REGISTER_DELAY		0xFE
#define AMP_REGISTER_END		0xFF

#define WM8993_RESET_REG		0x00
#define WM8993_RESET_VALUE		0x8993
#define WM8993_VOICEVOL_REG		0x20
#define WM8993_IDAC_REG			0x57
#define WM8993_SPK_SW_GPIO		105

#ifdef CONFIG_KTTECH_SOUND_TUNE
#define REG_MEMORY   static
#define REG_COUNT    AMP_REGISTER_MAX
#else
#define REG_MEMORY   static const     /* Normally, tables are in ROM  */
#define REG_COUNT
#endif /*CONFIG_KTTECH_SOUND_TUNE*/

// WM8993 Register Format
typedef struct {
	u8 reg;
	u16 value;
} wm8993_register_type;

#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
enum wm8993_master_bias_level {
	WM8993_MASTER_BIAS_OFF,
	WM8993_MASTER_BIAS_STANDBY,
	WM8993_MASTER_BIAS_PREPARE,
	WM8993_MASTER_BIAS_ON,
};
#endif

//////////////////////////////////////////////////////////////////////
// WM8993 Device Information
struct snddev_wm8993 {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock; // for register
	int mod_id;
	struct mutex path_lock; // for path
	struct mutex lock; // for ioctl
	uint32_t cal_type; // voip, video, etc(voice) call
	uint32_t eq_type;
	uint32_t mute_enabled;
	uint32_t voip_micgain;
	uint32_t noisegate_enabled; // Only YDA165
	uint32_t spksw_enabled; // Speaker Switch

#ifdef WM8993_CHECK_RESET // Reset 중복 방지를 위해 체크한다.
	atomic_t isreset;
#endif /*WM8993_CHECK_RESET*/
#ifdef WM8993_DIRECT_AMP_CONTROL
	atomic_t amp_enabled;
#endif /*WM8993_DIRECT_AMP_CONTROL*/
	u8 suspend_poweroff;
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
	enum wm8993_master_bias_level master_bias_level;
#endif
};

// WM8993  Platform Data
struct wm8993_platform_data {
	int (*wm8993_setup) (struct device *dev);
	void (*wm8993_shutdown) (struct device *dev);
};

//////////////////////////////////////////////////////////////////////
// WM8993 Function Prototype
void wm8993_init(void);
void wm8993_exit(void);

int wm8993_read(u8 reg, unsigned short *value);
int wm8993_write(u8 reg, unsigned short value);
int wm8993_update(unsigned short reg, unsigned short mask, unsigned short value);

void wm8993_set(int type, int value);
void wm8993_get(int type, int * value);

void wm8993_set_caltype(AMP_CAL_TYPE_E cal);
AMP_CAL_TYPE_E wm8993_get_caltype(void);

void wm8993_set_pathtype(AMP_PATH_TYPE_E path);
AMP_PATH_TYPE_E wm8993_get_pathtype(void);

void wm8993_set_register(wm8993_register_type *amp_regs);

void wm8993_enable(AMP_PATH_TYPE_E path);
void wm8993_disable(AMP_PATH_TYPE_E path);
void wm8993_reset(AMP_PATH_TYPE_E path);

#ifdef CONFIG_KTTECH_SOUND_TUNE
void wm8993_tuning(void *data, size_t size);
#endif /*CONFIG_KTTECH_SOUND_TUNE*/

#ifdef WM8993_DIRECT_AMP_CONTROL
void wm8993_enable_amplifier(void);
void wm8993_disable_amplifier(void);
#endif /*WM8993_DIRECT_AMP_CONTROL*/

#endif // __MACH_QDSP6V2_SNDDEV_WM8993_H__
