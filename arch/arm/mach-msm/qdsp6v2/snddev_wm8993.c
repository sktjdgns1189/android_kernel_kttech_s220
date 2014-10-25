/* Copyright (c) 2011-2012, KTTech. All rights reserved.
 *
 * WM8993 Audio Driver
 * Author: Gun Song
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

#include <mach/qdsp6v2/snddev_wm8993.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>

#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
#include "wm8993.h"
#endif

static AMP_PATH_TYPE_E	m_curr_path	= AMP_PATH_NONE;
static AMP_PATH_TYPE_E	m_prev_path	= AMP_PATH_NONE;
static AMP_CAL_TYPE_E	m_curr_cal	= AMP_CAL_NORMAL;
static AMP_CAL_TYPE_E	m_prev_cal	= AMP_CAL_NORMAL;
#ifndef WM8993_SINGLE_PATH
static AMP_PATH_TYPE_E	cur_tx = AMP_PATH_NONE;
static AMP_PATH_TYPE_E	cur_rx = AMP_PATH_NONE;
#endif /*WM8993_SINGLE_PATH*/

struct snddev_wm8993 wm8993_modules;

REG_MEMORY wm8993_register_type amp_none_path[REG_COUNT] = {
#ifdef WM8993_DEFAULT_REGISTER_RESTORE
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
    { 0x01, 0x0000 },
    { 0x02, 0x6000 },
#endif
    { 0x03, 0x0000 },
    { 0x04, 0x4050 },
    { 0x05, 0x4000 },
    { 0x06, 0x01C8 },
    { 0x07, 0x0000 },
    { 0x08, 0x0000 },
    { 0x09, 0x0040 },
    { 0x0A, 0x0004 },
    { 0x0B, 0x00C0 },
    { 0x0C, 0x00C0 },
    { 0x0D, 0x0000 },
    { 0x0E, 0x0300 },
    { 0x0F, 0x00C0 },
    { 0x10, 0x00C0 },
    { 0x12, 0x0000 },
    { 0x13, 0x0010 },
    { 0x14, 0x0000 },
    { 0x15, 0x0000 },
    { 0x16, 0x8000 },
    { 0x17, 0x0800 },
    { 0x18, 0x008B },
    { 0x19, 0x008B },
    { 0x1A, 0x008B },
    { 0x1B, 0x008B },
    { 0x1C, 0x006D },
    { 0x1D, 0x006D },
    { 0x1E, 0x0066 },
    { 0x1F, 0x0020 },
    { 0x20, 0x0079 },
    { 0x21, 0x0079 },
    { 0x22, 0x0003 },
    { 0x23, 0x0003 },
    { 0x24, 0x0011 },
    { 0x25, 0x0100 },
    { 0x26, 0x0079 },
    { 0x27, 0x0079 },
    { 0x28, 0x0000 },
    { 0x29, 0x0000 },
    { 0x2A, 0x0000 },
    { 0x2B, 0x0000 },
    { 0x2C, 0x0000 },
    { 0x2D, 0x0000 },
    { 0x2E, 0x0000 },
    { 0x2F, 0x0000 },
    { 0x30, 0x0000 },
    { 0x31, 0x0000 },
    { 0x32, 0x0000 },
    { 0x33, 0x0000 },
    { 0x34, 0x0000 },
    { 0x35, 0x0000 },
    { 0x36, 0x0000 },
    { 0x37, 0x0000 },
    { 0x38, 0x0000 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
    { 0x39, 0x0000 },
#endif
    { 0x3A, 0x0000 },
    { 0x3C, 0x0000 },
    { 0x3D, 0x0000 },
    { 0x3E, 0x0000 },
    { 0x3F, 0x2EE0 },
    { 0x40, 0x0002 },
    { 0x41, 0x2287 },
    { 0x42, 0x025F },
    { 0x43, 0x0000 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
    { 0x44, 0x0000 },
#endif
    { 0x45, 0x0002 },
    { 0x46, 0x0000 },
    { 0x47, 0x0000 },
    { 0x48, 0x0000 },
    { 0x49, 0x0000 },
    { 0x4A, 0x0000 },
    { 0x4B, 0x0000 },
    { 0x4C, 0x1F25 },
    { 0x51, 0x0000 },
    { 0x54, 0x0000 },
    { 0x55, 0x054A },
    { 0x57, 0x0000 },
    { 0x58, 0x0000 },
    { 0x59, 0x0000 },
    { 0x5A, 0x0000 },
    { 0x60, 0x0100 },
    { 0x62, 0x0000 },
    { 0x63, 0x000C },
    { 0x64, 0x000C },
    { 0x65, 0x000C },
    { 0x66, 0x000C },
    { 0x67, 0x000C },
    { 0x68, 0x0FCA },
    { 0x69, 0x0400 },
    { 0x6A, 0x00D8 },
    { 0x6B, 0x1EB5 },
    { 0x6C, 0xF145 },
    { 0x6D, 0x0B75 },
    { 0x6E, 0x01C5 },
    { 0x6F, 0x1C58 },
    { 0x70, 0xF373 },
    { 0x71, 0x0A54 },
    { 0x72, 0x0558 },
    { 0x73, 0x168E },
    { 0x74, 0xF829 },
    { 0x75, 0x07AD },
    { 0x76, 0x1103 },
    { 0x77, 0x0564 },
    { 0x78, 0x0559 },
    { 0x79, 0x4000 },
    { 0x7A, 0x0000 },
    { 0x7B, 0x0F08 },
    { 0x7C, 0x0000 },
    { 0x7D, 0x0080 },
    { 0x7E, 0x0000 },
    { 0xC1, 0x0181 },
#else
	{ 0x00 , 0x8993 },
#endif
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_handset_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
#endif /*WM8993_POP_NOIZE_ZERO*/
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE01
	{ 0x03 , 0x00f3 },
	{ 0x2d , 0x0001 },
	{ 0x2e , 0x0001 },
	{ 0x33 , 0x0018 },
	{ 0x33 , 0x0010 },
	{ 0x33 , 0x0008 },
	{ 0x1f , 0x0030 },
	{ 0x46 , 0x0100 },
	{ 0x49 , 0x0110 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x004e },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x38 , 0x0040 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0800 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0803 },
	{ AMP_REGISTER_DELAY , 267 }, // delay = 256.5ms
	{ 0x39 , 0x004c },
#endif
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x1f , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x05 , 0x4000 },
	{ AMP_REGISTER_DELAY , 260 },
#else
	{ 0x1f , 0x0000 },
	{ 0x33 , 0x0011 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x2d , 0x0001 },
	{ 0x2e , 0x0001 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 50 },
#endif
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0803 },
#endif
	{ 0x03 , 0x00f3 },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
#endif /*WM8993_STARTUPDOWN_CONTROL_CASE01*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0040 },
#else
	{ 0x0a , 0x0000 },
#endif /*WM8993_POP_NOIZE_ZERO*/
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_headset_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
#endif /*WM8993_POP_NOIZE_ZERO*/
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE01
    { 0x03 , 0x00f3 },
    { 0x2d , 0x0100 },
    { 0x2e , 0x0100 },
    { 0x46 , 0x0100 },
    { 0x49 , 0x0108 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x006c },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
#endif
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 }, // delay = 4.5ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0303 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x60 , 0x0122 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x54 , 0x0033 },
	{ AMP_REGISTER_DELAY , 257 }, // delay = 256.5ms
	{ 0x60 , 0x01ee },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x1c , 0x0176 },
	{ 0x1d , 0x0176 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
#endif
	{ 0x55 , 0x03e0 },
	{ 0x57 , 0x00fd },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x05 , 0xc000 }, // L,R Switching
#else
	{ 0x1c , 0x0176 },
	{ 0x1d , 0x0176 },
	{ 0x60 , 0x0000 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 33 },
#endif
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ AMP_REGISTER_DELAY , 10 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
#endif
	{ 0x55 , 0x03e0 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0303 },
#endif
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x05 , 0x8000 }, // L,R Switching
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
#endif /*WM8993_STARTUPDOWN_CONTROL_CASE01*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0040 },
#else
	{ 0x0a , 0x0000 },
#endif /*WM8993_POP_NOIZE_ZERO*/
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_speaker_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
#endif /*WM8993_POP_NOIZE_ZERO*/
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE01
	{ 0x03 , 0x0333 },
	{ 0x36 , 0x1003 },
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0100 },
	{ 0x46 , 0x0100 },
	{ 0x49 , 0x0110 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x006C },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
	{ 0x01 , 0x3003 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0173 },
	{ 0x27 , 0x0173 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ 0x05 , 0x4000 },
	{ AMP_REGISTER_DELAY , 15 },
#else
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0173 },
	{ 0x27 , 0x0173 },
	{ 0x36 , 0x0003 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
#endif
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x3003 },
#endif
	{ 0x03 , 0x0333 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
#endif /*WM8993_STARTUPDOWN_CONTROL_CASE01*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0040 },
#else
	{ 0x0a , 0x0000 },
#endif /*WM8993_POP_NOIZE_ZERO*/
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_headset_speaker_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
#endif /*WM8993_POP_NOIZE_ZERO*/
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE01
    { 0x03 , 0x03f3 },
    { 0x36 , 0x1003 },
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0100 },
    { 0x2d , 0x0100 },
    { 0x2e , 0x0100 },
    { 0x46 , 0x0100 },
    { 0x49 , 0x0108 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x006c },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
#endif
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 }, // delay = 4.5ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x3303 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x60 , 0x0122 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x54 , 0x0033 },
	{ AMP_REGISTER_DELAY , 257 }, // delay = 256.5ms
	{ 0x60 , 0x01ee },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x1c , 0x0176 },
	{ 0x1d , 0x0176 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
#endif
	{ 0x55 , 0x03e0 },
	{ 0x57 , 0x00fd },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0173 },
	{ 0x27 , 0x0173 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ 0x05 , 0xc000 }, // L,R Switching
#else
	{ 0x05 , 0x4000 },
	{ 0x1c , 0x0176 },
	{ 0x1d , 0x0176 },
	{ 0x60 , 0x0000 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 33 },
#endif
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ AMP_REGISTER_DELAY , 10 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
#endif
	{ 0x55 , 0x03e0 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0303 },
#endif
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x05 , 0x8000 }, // L,R Switching
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x60 , 0x00ee },
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0173 },
	{ 0x27 , 0x0173 },
	{ 0x36 , 0x0003 },
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x3303 },
#endif
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x03 , 0x03f3 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
#endif /*WM8993_STARTUPDOWN_CONTROL_CASE01*/
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0040 },
#else
	{ 0x0a , 0x0000 },
#endif /*WM8993_STARTUPDOWN_CONTROL_CASE01*/
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x07 , 0x8000 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x01 , 0x0003 },
	{ 0x04 , 0x0010 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x0f , 0x01c0 },
	{ 0x18 , 0x010b },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x02 , 0x6243 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_earmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x07 , 0x8000 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x01 , 0x0023 },
	{ 0x04 , 0xc010 },
	{ 0x10 , 0x01c0 },
	{ 0x1a , 0x010b },
	{ 0x28 , 0x0001 },
	{ 0x2a , 0x0030 },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x02 , 0x6113 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_headset_earmic_loop_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0173 },  // 20110317 by ssgun - volume up : 0x0176 -> 0x016d
	{ 0x1d , 0x0176 },  // 20110317 by ssgun - volume up : 0x0176 -> 0x016d
	{ 0x1a , 0x010f },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x07 , 0x0000 },
#if 0 // 20110322 by ssgun - enable earmic
	{ 0x04 , 0x4010 },
#else
	{ 0x04 , 0xc010 },
#endif
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0323 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0001 },
	{ 0x2a , 0x0020 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x10 , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x60 , 0x00ee },
	{ 0x05 , 0x4001 },
	{ 0x02 , 0x6111 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_speaker_mainmic_loop_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x22 , 0x0000},
	{ 0x23 , 0x0000},
	{ 0x24 , 0x0018},
	{ 0x25 , 0x0138},
	{ 0x26 , 0x0171},
	{ 0x27 , 0x0171},
	{ 0x36 , 0x0003},
	{ 0x39 , 0x0068},
	{ 0x01 , 0x0003},
	{ 0x0f , 0x01d0},
	{ 0x20 , 0x0179},
	{ 0x21 , 0x0179},
	{ 0x7b , 0xc718},
	{ 0x7c , 0x1124},
	{ 0x7d , 0x2c80},
	{ 0x7e , 0x4600},
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000},
	{ 0x04 , 0x0010},
	{ 0x01 , 0x3003},
	{ 0x03 , 0x0333},
	{ 0x01 , 0x3013},
	{ 0x18 , 0x010b},
	{ 0x28 , 0x0010},
	{ 0x29 , 0x0020},
	{ 0x62 , 0x0001},
	{ 0x63 , 0x0001},
	{ 0x64 , 0x000b},
	{ 0x65 , 0x000f},
	{ 0x66 , 0x000d},
	{ 0x6f , 0x1b18},
	{ 0x70 , 0xf48a},
	{ 0x71 , 0x040a},
	{ 0x72 , 0x11fa},
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4001},
	{ 0x02 , 0x6243},
	{ 0x0a , 0x0130},
	{ AMP_REGISTER_END   ,  0 },
};

// VOICE CALL
REG_MEMORY wm8993_register_type amp_call_handset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x07 , 0x0000 },
	{ 0x1f , 0x0000 },
	{ 0x33 , 0x0018 },
	{ 0x20 , 0x017f },
	{ 0x21 , 0x017f },
	{ 0x2d , 0x0001 },
	{ 0x2e , 0x0001 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x0f , 0x01c0 },
	{ 0x10 , 0x01c0 },
	{ 0x18 , 0x0106 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x0813 },
	{ 0x03 , 0x00f3 },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x0a , 0x0130 },
	{ 0x02 , 0x6342 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_call_speaker_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0174 },
	{ 0x27 , 0x0174 },
	{ 0x36 , 0x0003 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x0f , 0x01c0 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x3003 },
	{ 0x03 , 0x0333 },
	{ 0x01 , 0x3013 },
	{ 0x18 , 0x010b },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_call_headset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x18 , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0313 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x0f , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_call_headset_earmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x1a , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x04 , 0xc010 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0323 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0001 },
	{ 0x2a , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x10 , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6111 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END   ,  0 },
};

// VOIP CALL
REG_MEMORY wm8993_register_type amp_voip_handset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x07 , 0x0000 },
	{ 0x1f , 0x0000 },
	{ 0x33 , 0x0018 },
	{ 0x20 , 0x017f }, // 17f -> 179 (6db -> 0 db) -> 17f
	{ 0x21 , 0x017f }, // 17f -> 179 (6db -> 0 db) -> 17f
	{ 0x2d , 0x0001 },
	{ 0x2e , 0x0001 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x0f , 0x01c0 },
	{ 0x10 , 0x01c0 },
	{ 0x18 , 0x010b }, // 0x0106 -> 0x010b
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x0803 }, // 0x0813 -> 0x0803
	{ 0x03 , 0x00f3 },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x0a , 0x2000 }, // 0x0000 -> 0x2000
	{ 0x02 , 0x6342 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_voip_headset_earmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x1a , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x04 , 0xc010 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0323 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0001 },
	{ 0x2a , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x10 , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6111 },
	{ 0x0a , 0x0130 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_voip_headset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x18 , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0313 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x0f , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_voip_speaker_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0174 },
	{ 0x27 , 0x0174 },
	{ 0x36 , 0x0003 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x0f , 0x01c0 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x3003 },
	{ 0x03 , 0x0333 },
	{ 0x01 , 0x3013 },
	{ 0x18 , 0x010b },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

// VIDEO CALL
REG_MEMORY wm8993_register_type amp_video_handset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0174 },
	{ 0x27 , 0x0174 },
	{ 0x36 , 0x0003 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x0f , 0x01c0 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x3003 },
	{ 0x03 , 0x0333 },
	{ 0x01 , 0x3013 },
	{ 0x18 , 0x010b },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_video_headset_earmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x1a , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x04 , 0xc010 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0323 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0001 },
	{ 0x2a , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x10 , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6111 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_video_headset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x18 , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0313 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x0f , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_video_speaker_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0174 },
	{ 0x27 , 0x0174 },
	{ 0x36 , 0x0003 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x0f , 0x01c0 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x3003 },
	{ 0x03 , 0x0333 },
	{ 0x01 , 0x3013 },
	{ 0x18 , 0x010b },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

// MEDIA RX+TX
REG_MEMORY wm8993_register_type amp_handset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x07 , 0x0000 },
	{ 0x1f , 0x0000 },
	{ 0x33 , 0x0018 },
	{ 0x20 , 0x017f },
	{ 0x21 , 0x017f },
	{ 0x2d , 0x0001 },
	{ 0x2e , 0x0001 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x0f , 0x01c0 },
	{ 0x10 , 0x01c0 },
	{ 0x18 , 0x0106 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x0813 },
	{ 0x03 , 0x00f3 },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x0a , 0x0130 },
	{ 0x02 , 0x6342 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_headset_earmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x1a , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x04 , 0xc010 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0323 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0001 },
	{ 0x2a , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x10 , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6111 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_headset_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x1c , 0x0179 },
	{ 0x1d , 0x0179 },
	{ 0x18 , 0x010b },
	{ 0x60 , 0x0000 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ AMP_REGISTER_DELAY , 33 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ AMP_REGISTER_DELAY , 10 },
	{ 0x44 , 0x0003 },
	{ 0x56 , 0x0003 },
	{ 0x44 , 0x0000 },
	{ 0x55 , 0x03e0 },
	{ 0x01 , 0x0313 },
	{ 0x60 , 0x0022 },
	{ 0x4c , 0x9f25 },
	{ AMP_REGISTER_DELAY , 5 },
	{ 0x03 , 0x00f3 },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x2d , 0x0100 },
	{ 0x2e , 0x0100 },
	{ 0x0f , 0x01c0 },
	{ 0x54 , 0x0303 },
	{ AMP_REGISTER_DELAY , 160 },
	{ 0x57 , 0x00fd },
	{ 0x54 , 0x000f },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x000c },
	{ 0x64 , 0x000c },
	{ 0x65 , 0x000c },
	{ 0x66 , 0x000c },
	{ 0x67 , 0x000c },
	{ 0x68 , 0x0fca },
	{ 0x69 , 0x0400 },
	{ 0x6a , 0x00d8 },
	{ 0x6b , 0x1eb5 },
	{ 0x6c , 0xf145 },
	{ 0x6d , 0x0b75 },
	{ 0x6e , 0x01c5 },
	{ 0x6f , 0x1c58 },
	{ 0x70 , 0xf373 },
	{ 0x71 , 0x0a54 },
	{ 0x72 , 0x0558 },
	{ 0x73 , 0x168e },
	{ 0x74 , 0xf829 },
	{ 0x75 , 0x07ad },
	{ 0x76 , 0x1103 },
	{ 0x77 , 0x0564 },
	{ 0x78 , 0x0559 },
	{ 0x79 , 0x4000 },
	{ 0x60 , 0x00ee },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_speaker_mainmic_path[REG_COUNT] = {
#ifdef CONFIG_KTTECH_I2S_MSM_SLAVE // 20110210 by ssgun - slave mode
	{ 0x08 , 0x8000 },
#endif /*CONFIG_KTTECH_I2S_MSM_SLAVE*/
	{ 0x22 , 0x0000 },
	{ 0x23 , 0x0000 },
	{ 0x24 , 0x0018 },
	{ 0x25 , 0x0138 },
	{ 0x26 , 0x0174 },
	{ 0x27 , 0x0174 },
	{ 0x36 , 0x0003 },
	{ 0x39 , 0x0068 },
	{ 0x01 , 0x0003 },
	{ 0x0f , 0x01c0 },
	{ 0x20 , 0x0179 },
	{ 0x21 , 0x0179 },
	{ 0x7b , 0xc718 },
	{ 0x7c , 0x1124 },
	{ 0x7d , 0x2c80 },
	{ 0x7e , 0x4600 },
	{ AMP_REGISTER_DELAY , 50 },
	{ 0x07 , 0x0000 },
	{ 0x04 , 0x4010 },
	{ 0x01 , 0x3003 },
	{ 0x03 , 0x0333 },
	{ 0x01 , 0x3013 },
	{ 0x18 , 0x010b },
	{ 0x28 , 0x0010 },
	{ 0x29 , 0x0030 },
	{ 0x62 , 0x0001 },
	{ 0x63 , 0x0001 },
	{ 0x64 , 0x000b },
	{ 0x65 , 0x000f },
	{ 0x66 , 0x000d },
	{ 0x6f , 0x1b18 },
	{ 0x70 , 0xf48a },
	{ 0x71 , 0x040a },
	{ 0x72 , 0x11fa },
	{ AMP_REGISTER_DELAY , 15 },
	{ 0x05 , 0x4000 },
	{ 0x02 , 0x6242 },
	{ 0x0a , 0x0000 },
	{ AMP_REGISTER_END	 ,	0 },
};

REG_MEMORY wm8993_register_type amp_handset_earmic_path[REG_COUNT] = {
	{ 0x00 , 0x8993 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_speaker_earmic_path[REG_COUNT] = {
	{ 0x00 , 0x8993 },
	{ AMP_REGISTER_END   ,  0 },
};

static const wm8993_register_type *amp_sequence_path[AMP_PATH_MAX] [AMP_CAL_MAX]= {
	// AMP_PATH_NONE
	{
		amp_none_path, // AMP_CAL_NORMAL
		amp_none_path, // AMP_CAL_VOICECALL
		amp_none_path, // AMP_CAL_VIDEOCALL
		amp_none_path, // AMP_CAL_VOIPCALL
		amp_none_path  // AMP_CAL_LOOPBACK
	},

	// AMP_PATH_HANDSET
	{
		amp_handset_path,
		amp_call_handset_mainmic_path,
		amp_video_handset_mainmic_path,
		amp_voip_handset_mainmic_path,
		amp_none_path
	},

	// AMP_PATH_HEADSET
	{
		amp_headset_path,
		amp_call_headset_earmic_path,
		amp_video_headset_earmic_path,
		amp_voip_headset_earmic_path,
		amp_headset_earmic_loop_path
	},

	// AMP_PATH_SPEAKER
	{
		amp_speaker_path,
		amp_call_speaker_mainmic_path,
		amp_video_speaker_mainmic_path,
		amp_voip_speaker_mainmic_path,
		amp_speaker_mainmic_loop_path
	},

	// AMP_PATH_HEADSET_SPEAKER
	{
		amp_headset_speaker_path,
		amp_headset_speaker_path,
		amp_headset_speaker_path,
		amp_headset_speaker_path,
		amp_none_path
	},

	// AMP_PATH_HEADSET_NOMIC
	{
		amp_headset_path,
		amp_call_headset_mainmic_path,
		amp_video_headset_mainmic_path,
		amp_voip_headset_mainmic_path,
		amp_headset_mainmic_path
	},

	// AMP_PATH_MAINMIC
	{
		amp_mainmic_path,
		amp_handset_mainmic_path,
		amp_headset_mainmic_path,
		amp_speaker_mainmic_path,
		amp_speaker_mainmic_loop_path
	},

	// AMP_PATH_EARMIC
	{
		amp_earmic_path,
		amp_handset_earmic_path,
		amp_headset_earmic_path,
		amp_speaker_earmic_path,
		amp_headset_earmic_loop_path
	},
};

#ifdef WM8993_STARTUPDOWN_CONTROL
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
REG_MEMORY wm8993_register_type amp_shutdown_handset_path[REG_COUNT] = {
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x46 , 0x0100 },
	{ 0x49 , 0x012a },
	{ 0x00 , 0x1000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x54 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x4c , 0x1f25 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x39 , 0x006e },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0000 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
#endif
	{ 0x37 , 0x0001 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x38 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x37 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x0000 },
	{ AMP_REGISTER_DELAY , 200 }, // delay = 512.5ms too long -> 200ms
#endif
	{ 0x2d , 0x0000 },
	{ 0x2e , 0x0000 },
	{ 0x33 , 0x0000 },
	{ 0x1f , 0x0020 },
	{ 0x03 , 0x0000 },
	//{ 0x00 , 0x8993 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ AMP_REGISTER_END   , 0 },
};

REG_MEMORY wm8993_register_type amp_shutdown_headset_path[REG_COUNT] = {
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x46 , 0x0100 },
	{ 0x49 , 0x0122 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x60 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x54 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x3003 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x4c , 0x1f25 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x39 , 0x006e },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0000 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
	{ 0x39 , 0x0000 },
	{ AMP_REGISTER_DELAY , 200 }, // delay = 512.5ms too long -> 200ms
#endif
	{ 0x2d , 0x0000 },
	{ 0x2e , 0x0000 },
	{ 0x03 , 0x0000 },
	//{ 0x00 , 0x8993 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ AMP_REGISTER_END   , 0 },
};

REG_MEMORY wm8993_register_type amp_shutdown_speaker_path[REG_COUNT] = {
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x46 , 0x0100 },
	{ 0x49 , 0x0122 },
	{ 0x54 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x4c , 0x1f25 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x39 , 0x006e },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0000 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
	{ 0x39 , 0x0000 },
	{ AMP_REGISTER_DELAY , 200 }, // delay = 512.5ms too long -> 200ms
#endif
	{ 0x22 , 0x0003 },
	{ 0x23 , 0x0103 },
	{ 0x36 , 0x0000 },
	{ 0x03 , 0x0000 },
	//{ 0x00 , 0x8993 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ AMP_REGISTER_END   , 0 },
};

REG_MEMORY wm8993_register_type amp_shutdown_headsetspeaker_path[REG_COUNT] = {
#ifdef WM8993_POP_NOIZE_ZERO
	{ 0x0a , 0x0004 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x46 , 0x0100 },
	{ 0x49 , 0x0122 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x60 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x54 , 0x0000 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x3003 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#endif
	{ 0x4c , 0x1f25 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
#ifndef WM8993_STARTUPDOWN_CONTROL_CASE02
	{ 0x01 , 0x0003 },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x39 , 0x006e },
	{ AMP_REGISTER_DELAY , 1 }, // delay = 0.5625ms
	{ 0x01 , 0x0000 },
	{ AMP_REGISTER_DELAY , 33 }, // delay = 32.5ms
	{ 0x39 , 0x0000 },
	{ AMP_REGISTER_DELAY , 200 }, // delay = 512.5ms too long -> 200ms
#endif
	{ 0x22 , 0x0003 },
	{ 0x23 , 0x0103 },
	{ 0x2d , 0x0000 },
	{ 0x2e , 0x0000 },
	{ 0x36 , 0x0000 },
	{ 0x03 , 0x0000 },
	//{ 0x00 , 0x8993 },
	{ AMP_REGISTER_END   , 0 },
};

REG_MEMORY wm8993_register_type amp_shutdown_mainmic_path[REG_COUNT] = {
	{ 0x00 , 0x8993 },
	{ AMP_REGISTER_END   ,  0 },
};

REG_MEMORY wm8993_register_type amp_shutdown_earmic_path[REG_COUNT] = {
	{ 0x00 , 0x8993 },
	{ AMP_REGISTER_END   ,  0 },
};

static const wm8993_register_type *amp_shutdown_sequence_path[AMP_PATH_MAX]= {
	amp_none_path,                    // AMP_PATH_NONE
	amp_shutdown_handset_path,        // AMP_PATH_HANDSET
	amp_shutdown_headset_path,        // AMP_PATH_HEADSET
	amp_shutdown_speaker_path,        // AMP_PATH_SPEAKER
	amp_shutdown_headsetspeaker_path, // AMP_PATH_HEADSET_SPEAKER
	amp_shutdown_headset_path,        // AMP_PATH_HEADSET_NOMIC
	amp_shutdown_mainmic_path,        // AMP_PATH_MAINMIC
	amp_shutdown_earmic_path,         // AMP_PATH_EARMIC
};
#endif
#endif /* WM8993_STARTUPDOWN_CONTROL */

void wm8993_init(void)
{
	pr_debug("%s\n", __func__);
}
EXPORT_SYMBOL(wm8993_init);

void wm8993_exit(void)
{
	pr_debug("%s\n", __func__);
}
EXPORT_SYMBOL(wm8993_exit);

#ifdef WM8993_DEFAULT_REGISTER_RESTORE
static void wm8993_restore_defaultregister(wm8993_register_type *amp_regs)
{
	int i;
	struct snddev_wm8993 *wm8993 = &wm8993_modules;
	const wm8993_register_type *wm8993_reg_defaults = amp_sequence_path[0][0];

	if(amp_regs == NULL || wm8993_reg_defaults == NULL)
	{
		APM_INFO("Failed to resotre wm8993 default register\n");
		return;
	}

	mutex_lock(&wm8993->path_lock);

	/* Restore the register settings */
	for (i = 0; (i < AMP_REGISTER_MAX) && (amp_regs[i].reg == wm8993_reg_defaults[i].reg); i++)
	{
		wm8993_write(i, wm8993_reg_defaults[i].value);
	}

	mutex_unlock(&wm8993->path_lock);

	APM_INFO("Completed to resotre wm8993 default register\n");
	return;
}
#endif

#ifdef CONFIG_KTTECH_SOUND_TUNE
static void wm8993_apply_register (void *data, size_t size)
{
#ifdef WM8993_DEGUB_MSG
	int i = 0;
	int nCMDCount = 0;
	AMP_PATH_TYPE_E path = (AMP_PATH_TYPE_E)pFirstData->reg;
	AMP_CAL_TYPE_E cal   = (AMP_PATH_TYPE_E)pFirstData->value;
#endif
	wm8993_register_type *pFirstData = (wm8993_register_type*)data;
	wm8993_register_type *pCurData = (wm8993_register_type*)data;
	wm8993_register_type *amp_regs;

#ifdef WM8993_DEGUB_MSG
	nCMDCount = size / sizeof(wm8993_register_type);
	APM_INFO("CODEC_TUNING PATH = %d, CAL = %d COUNT =%d \n", path, cal, nCMDCount);
	for ( i = 0 ; i < nCMDCount ; i ++ )
	{
		APM_INFO("CMD = [0X%.2x] , [0X%.4x] \n" , pCurData->reg , pCurData->value);
		pCurData = pCurData + 1;
	}
#endif
	pCurData = pFirstData + 1;
	amp_regs = pCurData;

	wm8993_set_register((wm8993_register_type *)amp_regs);
}

void wm8993_tuning(void *data, size_t size)
{
	if (data == NULL || size == 0 || size > (sizeof(wm8993_register_type) * AMP_REGISTER_MAX))
	{
		APM_INFO("invalid prarameters data = %d, size = %d \n", (int)data, size);
		return;
	}

	wm8993_apply_register(data, size);
}
EXPORT_SYMBOL(wm8993_tuning);
#endif /*CONFIG_KTTECH_SOUND_TUNE*/

/**
 * wm8993_set
 * @param cal: kind of cal type
 *
 * @returns void
 */
void wm8993_set(int type, int value)
{
	if(type == AMP_TYPE_CAL)
	{
		wm8993_set_caltype(value);
	}
	else
	{
		wm8993_set_pathtype(value);
	}
}
EXPORT_SYMBOL(wm8993_set);

/**
 * wm8993_get
 * @param cal: kind of cal type
 *
 * @returns void
 */
void wm8993_get(int type, int * value)
{
	if(type == AMP_TYPE_CAL)
	{
		*value = wm8993_get_caltype();
	}
	else
	{
		*value = wm8993_get_pathtype();
	}
}
EXPORT_SYMBOL(wm8993_get);

/**
 * wm8993_set_caltype - Set calibration kind in WM8993
 * @param cal: kind of cal type
 *
 * @returns void
 */
void wm8993_set_caltype(AMP_CAL_TYPE_E cal)
{
	if (cal < AMP_CAL_NORMAL || cal >= AMP_CAL_MAX)
	{
		APM_INFO("not support calibration type on wm8993 cal = %d\n", cal);
		return;
	}

	m_curr_cal = cal;
	APM_INFO("Set calibration type on wm8993 cal = %d\n", m_curr_cal);
}
EXPORT_SYMBOL(wm8993_set_caltype);

/**
 * wm8993_get_caltype - get calibration kind in WM8993
 *
 * @returns AMP_CAL_TYPE_E
 */
AMP_CAL_TYPE_E wm8993_get_caltype(void)
{
	APM_INFO("Get calibration type on wm8993 cal = %d\n", m_curr_cal);
	return m_curr_cal;
}
EXPORT_SYMBOL(wm8993_get_caltype);

/**
 * wm8993_set_caltype - Set path kind in WM8993
 * @param cal: kind of cal type
 *
 * @returns void
 */
void wm8993_set_pathtype(AMP_PATH_TYPE_E path)
{
	if (path < AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("not support path type on wm8993 path = %d\n", path);
		return;
	}

	m_curr_path = path;
	APM_INFO("Set path type on wm8993 path = %d\n", m_curr_cal);
}
EXPORT_SYMBOL(wm8993_set_pathtype);

/**
 * wm8993_get_pathtype - get path kind in WM8993
 *
 * @returns AMP_PATH_TYPE_E
 */
AMP_PATH_TYPE_E wm8993_get_pathtype(void)
{
	APM_INFO("Get path type on wm8993 path = %d\n", m_curr_path);
	return m_curr_path;
}
EXPORT_SYMBOL(wm8993_get_pathtype);

void wm8993_set_register(wm8993_register_type *amp_regs)
{
	uint32_t loop = 0;
	struct snddev_wm8993 *wm8993 = &wm8993_modules;

#ifdef WM8993_CHECK_RESET // 새로운 레지스터를 설정한다.
	if (atomic_read(&wm8993->isreset) == 1)
		atomic_set(&wm8993->isreset, 0);
#endif /*WM8993_CHECK_RESET*/

	while (amp_regs[loop].reg != AMP_REGISTER_END)
	{
		if (amp_regs[loop].reg == AMP_REGISTER_DELAY)
		{
			msleep(amp_regs[loop].value);
		}
		else if (amp_regs[loop].reg == WM8993_IDAC_REG)
		{
			// read R59h & R5Ah and apply -2 code offset to left and right iDAC values
			unsigned short r59 = 0, r5a = 0, r57 = 0;
			wm8993_read(0x59, &r59);
			wm8993_read(0x5a, &r5a);
			r59 -= 2;
			r5a -= 2;
			r57 = (r5a & 0x00FF) | ((r59 & 0x00FF)<<8);
			wm8993_write(amp_regs[loop].reg, r57);
		}
		else
		{
			wm8993_write(amp_regs[loop].reg, amp_regs[loop].value);
#ifdef WM8993_DEGUB_MSG
			APM_INFO("reg 0x%x , value 0x%x",amp_regs[loop].reg, amp_regs[loop].value);
#endif
		}
		loop++;
	}

	return;
}
EXPORT_SYMBOL(wm8993_set_register);

#ifdef WM8993_DIRECT_AMP_CONTROL
void wm8993_enable_amplifier(void)
{
	u8 addr;
	unsigned short value;
	struct snddev_wm8993 *wm8993 = &wm8993_modules;

	if(atomic_read(&wm8993->amp_enabled) == 1)
	{
		APM_INFO("The device(%d) is already open\n", cur_rx);
		return;
	}

	addr = 0x01;
	if (cur_rx == AMP_PATH_HEADSET || cur_rx == AMP_PATH_HEADSET_NOMIC)
	{
		if(cur_tx == AMP_PATH_MAINMIC)
			value = 0x0317; //0x0303;
		else if(cur_tx == AMP_PATH_EARMIC)
			value = 0x0327; //0x0303;
		else
			value = 0x0307; //0x0303;
	}
	else if (cur_rx == AMP_PATH_SPEAKER)
	{
		if(cur_tx == AMP_PATH_MAINMIC)
			value = 0x3017; //0x3003;
		else if(cur_tx == AMP_PATH_EARMIC)
			value = 0x3027; //0x3003;
		else
			value = 0x3007; //0x3003;
	}
	else if (cur_rx == AMP_PATH_HEADSET_SPEAKER)
	{
		if(cur_tx == AMP_PATH_MAINMIC)
			value = 0x3317; //0x3303;
		else if(cur_tx == AMP_PATH_EARMIC)
			value = 0x3327; //0x3303;
		else
			value = 0x3307; //0x3303;
	}
	else
	{
		APM_INFO("Enable Amplifier - Unsupported Device\n");
		return;
	}

	wm8993_write(addr, value);
	APM_INFO("Enable Amplifier - Register(0x01,0x%x)\n", value);
	atomic_set(&wm8993->amp_enabled, 1);

	return;
}
EXPORT_SYMBOL(wm8993_enable_amplifier);

void wm8993_disable_amplifier(void)
{
	u8 addr;
	unsigned short value;
	struct snddev_wm8993 *wm8993 = &wm8993_modules;

	if(atomic_read(&wm8993->amp_enabled) == 0)
	{
		APM_INFO("The device(%d) is already closed\n", cur_rx);
		return;
	}

	if(cur_rx == AMP_PATH_NONE)
	{
		APM_INFO("Disable Amplifier - Unsupported Device\n");
		return;
	}

	addr = 0x01;
	if(cur_tx == AMP_PATH_MAINMIC)
		value = 0x0013;
	else if(cur_tx == AMP_PATH_EARMIC)
		value = 0x0023;
	else
		value = 0x0003;

	wm8993_write(addr, value);
	APM_INFO("Disable Amplifier - Register(0x0x,0x0003)\n");
	atomic_set(&wm8993->amp_enabled, 0);

	return;
}
EXPORT_SYMBOL(wm8993_disable_amplifier);
#endif /*WM8993_DIRECT_AMP_CONTROL*/

#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
static int wm8993_set_masterbias_level(enum wm8993_master_bias_level level)
{
	int ret = 0;
	struct snddev_wm8993 *wm8993 = &wm8993_modules;

	switch (level)
	{
		case WM8993_MASTER_BIAS_PREPARE:
			if (wm8993->master_bias_level == WM8993_MASTER_BIAS_STANDBY)
			{
				/* VMID=2*40k */
				wm8993_update(WM8993_POWER_MANAGEMENT_1,
				WM8993_VMID_SEL_MASK, 0x2);
				wm8993_update(WM8993_POWER_MANAGEMENT_2,
				WM8993_TSHUT_ENA, WM8993_TSHUT_ENA);
			}
			break;

		case WM8993_MASTER_BIAS_STANDBY:
			if (wm8993->master_bias_level == WM8993_MASTER_BIAS_OFF)
			{
				wm8993_reset(AMP_PATH_NONE);

				/* Tune DC servo configuration */
				wm8993_write(0x44, 0x0003);
				wm8993_write(0x56, 0x0003);
				wm8993_write(0x44, 0x0000);

				/* Bring up VMID with fast soft start */
				wm8993_update(WM8993_ANTIPOP2,
				WM8993_STARTUP_BIAS_ENA |
				WM8993_VMID_BUF_ENA |
				WM8993_VMID_RAMP_MASK |
				WM8993_BIAS_SRC,
				WM8993_STARTUP_BIAS_ENA |
				WM8993_VMID_BUF_ENA |
				WM8993_VMID_RAMP_MASK |
				WM8993_BIAS_SRC); 

				/* VMID=2*40k */
				wm8993_update(WM8993_POWER_MANAGEMENT_1,
				WM8993_VMID_SEL_MASK |
				WM8993_BIAS_ENA,
				WM8993_BIAS_ENA | 0x2);
				msleep(32);

				/* Switch to normal bias */
				wm8993_update(WM8993_ANTIPOP2,
				WM8993_BIAS_SRC |
				WM8993_STARTUP_BIAS_ENA, 0);
			}

			/* VMID=2*240k */
			wm8993_update(WM8993_POWER_MANAGEMENT_1,
			WM8993_VMID_SEL_MASK, 0x4);

			wm8993_update(WM8993_POWER_MANAGEMENT_2,
			WM8993_TSHUT_ENA, 0);
			break;

		case WM8993_MASTER_BIAS_ON:
			if (wm8993->master_bias_level == WM8993_MASTER_BIAS_PREPARE)
			{
				wm8993_write(0x39, 0x0068);
				wm8993_write(0x01, 0x3003);
			}
			break;

		case WM8993_MASTER_BIAS_OFF:
			wm8993_update(WM8993_POWER_MANAGEMENT_1,
			WM8993_VMID_SEL_MASK | WM8993_BIAS_ENA,
			0);

			wm8993_update(WM8993_ANTIPOP2,
			WM8993_STARTUP_BIAS_ENA |
			WM8993_VMID_BUF_ENA |
			WM8993_VMID_RAMP_MASK |
			WM8993_BIAS_SRC, 0);
			break;
	}

	wm8993->master_bias_level = level;

	return ret;
}
#endif

/**
 * wm8993_enable - Enable path  in WM8993
 * @param path: amp path
 *
 * @returns void
 */
void wm8993_enable(AMP_PATH_TYPE_E path)
{
	struct snddev_wm8993 *wm8993 = &wm8993_modules;
#ifdef WM8993_SINGLE_PATH // 20110510 by ssgun
	const wm8993_register_type *amp_regs = amp_sequence_path[path][m_curr_cal];
	int isvoice = msm_device_get_isvoice();

	APM_INFO("path = new[%d] vs old[%d:%d], cal_type = %d\n",
			path, m_curr_path, m_prev_path, m_curr_cal);

	if(amp_regs == NULL || path <= AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("not support path on wm8993 path = %d\n", path);
		return;
	}

	mutex_lock(&wm8993->path_lock);
	if(path == m_curr_path)
	{
		mutex_unlock(&wm8993->path_lock);
		APM_INFO("same previous path on wm8993 path = %d\n", path);
		return;
	}

	// 통화의 경우 Rx, Tx가 거의 동시에 열린다.
	if(isvoice == 1) // AUDDEV_EVT_START_VOICE
	{
		if((m_curr_path != AMP_PATH_NONE) && (m_curr_path != path))
		{
			m_prev_path = m_curr_path;
			m_curr_path = path;

			mutex_unlock(&wm8993->path_lock);
			APM_INFO("========== RETURN ==========\n");

			return;
		}

		if(m_curr_cal == AMP_CAL_NORMAL) // if it is'nt call
		{
			APM_INFO("[CALL] OLD : path = %d, old reg = %p, cal_type = %d\n",
					m_curr_path, amp_regs, m_curr_cal);
			m_curr_cal = AMP_CAL_VOICECALL;
			amp_regs = amp_sequence_path[path][m_curr_cal];
			APM_INFO("[CALL] NEW : path = %d, new reg = %p, cal_type = %d\n",
					path, amp_regs, m_curr_cal);
		}
	}
	// 미디어의 경우 RX, TX가 각각 열리거나 순차적으로 열린다.
	else // MEDIA
	{
		bool bTxRxOpen = false;
		unsigned short newpath = 0;
		unsigned short newcaltype = 0;

		if(m_curr_path != AMP_PATH_NONE && m_curr_path != path)
		{
			if(m_curr_path == AMP_PATH_HANDSET && path == AMP_PATH_MAINMIC)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VOICECALL;
				newpath = path;
			}
			else if(m_curr_path == AMP_PATH_HEADSET && path == AMP_PATH_MAINMIC)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VIDEOCALL;
				newpath = path;
			}
			else if(m_curr_path == AMP_PATH_SPEAKER && path == AMP_PATH_MAINMIC)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VOIPCALL;
				newpath = path;
			}
			else if(m_curr_path == AMP_PATH_HEADSET && path == AMP_PATH_EARMIC)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VIDEOCALL;
				newpath = path;
			}
			else if(m_curr_path == AMP_PATH_MAINMIC && path == AMP_PATH_HANDSET)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VOICECALL;
				newpath = AMP_PATH_MAINMIC;
			}
			else if(m_curr_path == AMP_PATH_MAINMIC && path == AMP_PATH_HEADSET)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VIDEOCALL;
				newpath = AMP_PATH_MAINMIC;
			}
			else if(m_curr_path == AMP_PATH_EARMIC && path == AMP_PATH_HEADSET)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VIDEOCALL;
				newpath = AMP_PATH_EARMIC;
			}
			else if(m_curr_path == AMP_PATH_MAINMIC && path == AMP_PATH_SPEAKER)
			{
				bTxRxOpen = true;
				newcaltype = AMP_CAL_VOIPCALL;
				newpath = AMP_PATH_MAINMIC;
			}
			else
			{
			}

			if(bTxRxOpen)
			{
				wm8993_reset(m_curr_path);

				APM_INFO("[MEDIA] OLD : path = %d, cal_type = %d => reg = %p\n",
						path, m_curr_cal, amp_regs);
				if(m_curr_cal == AMP_CAL_LOOPBACK) newcaltype = AMP_CAL_LOOPBACK;
				amp_regs = amp_sequence_path[newpath][newcaltype];
				APM_INFO("[MEDIA] NEW : path = %d, cal_type = %d => reg = %p\n",
						newpath, newcaltype, amp_regs);
			}
		}
	}

	m_prev_path = m_curr_path;
	m_curr_path = path;

	mutex_unlock(&wm8993->path_lock);
#else
	int tx = 0, rx = 0;
	int new_cal = 0, new_path = 0;
	int new_rx = 0, new_tx = 0;
	int isvoice = 0;
	bool reset = false;
	const wm8993_register_type *amp_regs;

	if(path <= AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("not support path on wm8993 path = %d\n", path);
		return;
	}

	if(path == AMP_PATH_MAINMIC || path == AMP_PATH_EARMIC)
	{
		new_tx = path;
	}
	else
	{
		new_rx = path;
#ifdef WM8993_DIRECT_AMP_CONTROL
		atomic_set(&wm8993->amp_enabled, 1);
#endif /*WM8993_DIRECT_AMP_CONTROL*/
	}

	isvoice = msm_device_get_isvoice();

	APM_INFO("Input Path [%d], CAL[%d] - NEW R:Tx[%d:%d] vs Cur R:Tx[%d:%d]\n",
			path, new_rx, new_tx, cur_rx, cur_tx, m_curr_cal);

	mutex_lock(&wm8993->path_lock);

#ifdef WM8993_CODEC_FAST_CALL_REGISTER
	// 통화 중이면 Rx open시 Tx Register도 같이 적용한다.
	// 이후 Rx 변경 시도 체크하여 R/Tx Register 동시 적용한다.
	if(isvoice == 1)
	{
		bool bSetReg = false;
		bool bReset = false;

		if(new_tx) // 통화 시 Tx Device만 변경되거나 적용될 수 없다.
		{
			cur_tx = m_curr_path = new_path = new_tx;
		}
		if(new_rx) // 통화 시 Rx Device가 먼저 open을 시도한다.
		{
			bSetReg = true;
			if(new_rx != cur_rx) bReset = true;
			else if(new_rx == cur_rx) bSetReg = false;
			cur_rx = m_curr_path = new_path = new_rx;
		}

		// Exception Case: HEADSET_NOMIC
		if(cur_tx == AMP_PATH_MAINMIC && cur_rx == AMP_PATH_HEADSET)
		{
			new_path = AMP_PATH_HEADSET_NOMIC;
			bSetReg = true;
			bReset = true;
		}

		if(bSetReg)
		{
			if(m_curr_cal == AMP_CAL_NORMAL) m_curr_cal = AMP_CAL_VOICECALL;
			new_cal = m_curr_cal;

			if(bReset) wm8993_reset(m_curr_path);

			amp_regs = amp_sequence_path[new_path][new_cal];
			if(amp_regs == NULL)
			{
				APM_INFO("WM8993 Path:Cal [%d:%d] is empty value\n", new_path, new_cal);
				mutex_unlock(&wm8993->path_lock);
				return;
			}

			mutex_unlock(&wm8993->path_lock);
			goto set_register;
		}
		else
		{
			APM_INFO("WM8993 Path:Cal [%d:%d] Call State\n", new_path, new_cal);
			mutex_unlock(&wm8993->path_lock);
			return;
		}
	}
#endif /*WM8993_CODEC_FAST_CALL_REGISTER*/

	if(new_rx != 0 && cur_rx != 0)
	{
		cur_rx = rx = new_rx;
		reset = true;
	}
	else if(new_rx)
	{
		cur_rx = rx = new_rx;
	}
	else if(cur_rx)
	{
		rx = cur_rx;
	}

	if(new_tx != 0 && cur_tx != 0)
	{
		cur_tx = tx = new_tx;
		reset = true;
	}
	else if(new_tx)
	{
		cur_tx = tx = new_tx;
	}
	else if(cur_tx)
	{
		tx = cur_tx;
	}

	if(isvoice == 1)
	{
		if(m_curr_cal == AMP_CAL_NORMAL) m_curr_cal = AMP_CAL_VOICECALL;
	}
	else
	{
		// voice, video call은 HAL Layer에서 설정되는 값으로 변경하지 않는다.
		if(m_curr_cal == AMP_CAL_VOICECALL) m_curr_cal = AMP_CAL_NORMAL;
	}
	mutex_unlock(&wm8993->path_lock);

	if(rx != 0 && tx != 0)
	{
		reset = true;
		if(isvoice == 1)
		{
			APM_INFO("WM8993 Call State - New R:Tx [%d:%d] Cur R:Tx[%d:%d]\n",
					new_rx, new_tx, rx, tx);

			// 20110707 by ssgun - 통화 처리 간소화
			if(rx == AMP_PATH_HANDSET)
			{
				new_cal = m_curr_cal;
				new_path = rx;
			}
			else if(rx == AMP_PATH_HEADSET)
			{
				new_cal = m_curr_cal;
				if(tx == AMP_PATH_MAINMIC)
					new_path = AMP_PATH_HEADSET_NOMIC;
				else
					new_path = rx;
			}
			else if(rx == AMP_PATH_SPEAKER)
			{
				new_cal = m_curr_cal;
				new_path = rx;
			}
			else if(rx == AMP_PATH_HEADSET_SPEAKER)
			{
				new_cal = AMP_CAL_NORMAL; // Exception Case
				new_path = rx;
			}
			else if(rx == AMP_PATH_HEADSET_NOMIC)
			{
				new_cal = m_curr_cal;
				if(tx == AMP_PATH_MAINMIC)
					new_path = rx;
				else // Exception Case
					new_path = AMP_PATH_HEADSET;
			}
			else
			{
				APM_INFO("WM8993 Unknown Path - New R:Tx [%d:%d] Cal[%d]\n",
						new_rx, new_tx, m_curr_cal);
				return;
			}
		}
		else
		{
			APM_INFO("WM8993 Media State - New R:Tx [%d:%d] Cur R:Tx[%d:%d]\n",
					new_rx, new_tx, rx, tx);

			// 20110707 by ssgun - 다음 마이피플 예외 상황 처리
			// 예외 케이스
			// - 아래 예외 케이스를 대비하기 위해 CAL Type을 무시하고 
			//   Sound Device를 체크하여 재 설정한다.
			// 1) INCALL_MODE 설정 후 어플 실행 중 해제되는 케이스
			// 2) VOIP 등 CAL TYPE 설정 후 어플 실행 중 해제되는 케이스
			if(rx == AMP_PATH_HANDSET)
			{
				if(m_curr_cal == AMP_CAL_VOICECALL)
				{
					new_cal = m_curr_cal;
					new_path = rx;
				}
				else
				{
					new_cal = AMP_CAL_VOICECALL;
					new_path = tx;
				}
			}
			else if(rx == AMP_PATH_HEADSET)
			{
				if(m_curr_cal == AMP_CAL_VOICECALL)
				{
					new_cal = m_curr_cal;
					new_path = rx;
				}
				else
				{
					new_cal = AMP_CAL_VIDEOCALL;
					new_path = tx;
				}
			}
			else if(rx == AMP_PATH_SPEAKER)
			{
				if(m_curr_cal == AMP_CAL_VOICECALL)
				{
					new_cal = m_curr_cal;
					new_path = rx;
				}
				else
				{
					new_cal = AMP_CAL_VOIPCALL;
					new_path = tx;
				}
			}
			else if(rx == AMP_PATH_HEADSET_SPEAKER)
			{
				if(m_curr_cal == AMP_CAL_VOICECALL)
				{
					new_cal = m_curr_cal;
					new_path = rx;
				}
				else
				{
					new_cal = AMP_CAL_NORMAL; // Exception Case
					new_path = rx;
				}
			}
			else if(rx == AMP_PATH_HEADSET_NOMIC)
			{
				if(m_curr_cal == AMP_CAL_VOICECALL)
				{
					new_cal = m_curr_cal;
					new_path = rx;
				}
				else
				{
					new_cal = AMP_CAL_VIDEOCALL;
					new_path = rx;
				}
			}
			else
			{
				APM_INFO("WM8993 Unknown Path - New R:Tx [%d:%d] Cal[%d]\n",
						new_rx, new_tx, m_curr_cal);
				return;
			}
		}
	}
	else if(rx)
	{
		new_path = rx;
		new_cal = m_curr_cal; // 20110516 by ssgun
	}
	else if(tx)
	{
		new_path = tx;
		new_cal = m_curr_cal; // 20110516 by ssgun
	}
	else
	{
		APM_INFO("WM8993 Unknown Path [%d] Cal[%d]\n", new_path, new_cal);
		return;
	}
	m_curr_path = new_path;
	m_prev_cal = new_cal;

	APM_INFO("WM8993 Set Path - New R:Tx [%d:%d] Current R:Tx [%d:%d]\n",
			new_path, new_cal, cur_rx, cur_tx);

	if(reset)
	{
		if(path == cur_rx)
			wm8993_reset(cur_tx);
		else if(path == cur_tx)
			wm8993_reset(cur_rx);
		else
			wm8993_reset(AMP_PATH_NONE);
	}

	amp_regs = amp_sequence_path[new_path][new_cal];
	if(amp_regs == NULL)
	{
		APM_INFO("WM8993 Path[%d:%d] is empty value\n", new_path, new_cal);
		return;
	}
#endif /*WM8993_SINGLE_PATH*/

#ifdef WM8993_CODEC_FAST_CALL_REGISTER
set_register:
#endif /*WM8993_CODEC_FAST_CALL_REGISTER*/

#if 0 // default Speaker Switch Status : Enable -> Disable
#ifdef WM8993_CHECK_SPEAKERSWITCH
	if((path == AMP_PATH_HANDSET) && (atomic_read(&wm8993->isspksw) == 0))
	{
		APM_INFO("HANDSET[%d]-Disable SpeakerSwitch\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 1);
		atomic_set(&wm8993->isspksw, 1);
	}
#else
	if((path == AMP_PATH_HANDSET) && (gpio_get_value(WM8993_SPK_SW_GPIO) == 0))
	{
		APM_INFO("HANDSET[%d]-Disable SpeakerSwitch\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 1);
	}
#endif /*WM8993_CHECK_SPEAKERSWITCH*/
#else
	if(path == AMP_PATH_SPEAKER || path == AMP_PATH_HEADSET_SPEAKER)
	{
		if (gpio_get_value(WM8993_SPK_SW_GPIO) != 0) {
			gpio_set_value(WM8993_SPK_SW_GPIO, 0);
			APM_INFO("Enable Speaker Switch : %d\n", path);
			usleep(100);
		}
	}
	else
	{
		if (gpio_get_value(WM8993_SPK_SW_GPIO) != 1) {
			gpio_set_value(WM8993_SPK_SW_GPIO, 1);
			APM_INFO("Disable Speaker Switch : %d\n", path);
			usleep(100);
		}
	}
#endif

#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
    wm8993_set_masterbias_level(WM8993_MASTER_BIAS_PREPARE);
    //msleep(1);
    wm8993_set_masterbias_level(WM8993_MASTER_BIAS_ON);
    //msleep(1);
#endif

	mutex_lock(&wm8993->path_lock);
	wm8993_set_register((wm8993_register_type *)amp_regs);
	mutex_unlock(&wm8993->path_lock);

	APM_INFO("WM8993 Apply to register is completed - Path [%d:%d] Cal[%d]\n",
			path, new_path, new_cal);
	return;
}
EXPORT_SYMBOL(wm8993_enable);

/**
 * wm8993_disable - Disable path  in WM8993
 * @param path: amp path
 *
 * @returns void
 */
void wm8993_disable(AMP_PATH_TYPE_E path)
{
	struct snddev_wm8993 *wm8993 = &wm8993_modules;

#ifdef WM8993_SINGLE_PATH // 20110510 by ssgun
	int isvoice = msm_device_get_isvoice();

	APM_INFO("path = new[%d] vs old[%d:%d], cal_type = %d\n",
			path, m_curr_path, m_prev_path, m_curr_cal);

	mutex_lock(&wm8993->path_lock);

	// Call, Media 모두 Rx, Tx Device가 동시에 열릴 수 있다.
	// 따라서 PATH 값을 Rx, Tx 두 개로 처리해야 한다.
	// 향후 Rx, Tx에 대한 PATH를 구분하여 처리하도록 하자.
	if(path == m_prev_path)
	{
		m_prev_path = AMP_PATH_NONE;
		APM_INFO("disable path = %d vs enable path = %d\n", path, m_curr_path);
		goto noreset;
	}
	else if(path == m_curr_path)
	{
		if(m_prev_path != AMP_PATH_NONE)
		{
			m_curr_path = m_prev_path;
			m_prev_path = AMP_PATH_NONE;
			APM_INFO("disable path = %d vs enable path = %d\n", path, m_curr_path);
			goto noreset;
		}
	}

	m_curr_path = AMP_PATH_NONE;
	m_prev_path = AMP_PATH_NONE;

	wm8993_reset(path);

noreset:
	mutex_unlock(&wm8993->path_lock);

#ifdef WM8993_CHECK_SPEAKERSWITCH
	if((path == AMP_PATH_HANDSET) && (atomic_read(&wm8993->isspksw) == 1))
	{
		APM_INFO("HANDSET[%d]-Enable SpeakerSwitch\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 0);
		atomic_set(&wm8993->isspksw, 0);
	}
#else
	if((path == AMP_PATH_HANDSET) && (gpio_get_value(WM8993_SPK_SW_GPIO) == 1))
	{
		APM_INFO("HANDSET[%d]-SPK_SW_GPIO:LOW\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 0);
	}
#endif /*WM8993_CHECK_SPEAKERSWITCH*/

	// Media일 경우 이전 레지스터 데이터를 복원해야 한다.
	// (Call은 거의 동시 처리되므로 의미 없다.)
	if((m_curr_path != AMP_PATH_NONE) && (isvoice != 1))
	{
		const wm8993_register_type *amp_regs = NULL;

		APM_INFO("new apply path = %d -> %d, caltype = %d\n", path, m_curr_path, m_curr_cal);
		m_curr_cal = AMP_CAL_NORMAL;
		amp_regs = amp_sequence_path[m_curr_path][m_curr_cal];
		if(amp_regs == NULL)
		{
			APM_INFO("not support path on wm8993 path = %d, cal = %d\n", m_curr_path, m_curr_cal);
			return;
		}

		wm8993_reset(path);
		wm8993_set_register((wm8993_register_type *)amp_regs);
	}
#else
	int new_rx = 0, new_tx = 0;
	int new_cal = 0, new_path = 0;
	int isvoice = 0;
	bool reset = true;

	if(path <= AMP_PATH_NONE || path >= AMP_PATH_MAX)
	{
		APM_INFO("not support path on wm8993 path = %d\n", path);
		return;
	}

	if(path == AMP_PATH_MAINMIC || path == AMP_PATH_EARMIC)
	{
		new_tx = path;
	}
	else
	{
		new_rx = path;
#ifdef WM8993_DIRECT_AMP_CONTROL
		atomic_set(&wm8993->amp_enabled, 0);
#endif /*WM8993_DIRECT_AMP_CONTROL*/
	}

	isvoice = msm_device_get_isvoice();

	APM_INFO("[%d] NEW R:Tx[%d:%d] vs CUR R:Tx[%d:%d], CAL[%d]\n", 
			path, new_rx, new_tx, cur_rx, cur_tx, m_curr_cal);

	mutex_lock(&wm8993->path_lock);

	if(new_tx != AMP_PATH_NONE)
	{
		if(cur_tx != new_tx)
		{
			// Exception Case
		}
		m_prev_path = cur_tx;
		cur_tx = AMP_PATH_NONE;

		if(cur_rx != AMP_PATH_NONE)
		{
			new_path = cur_rx;
		}
	}

	if(new_rx != AMP_PATH_NONE)
	{
		if(cur_rx != new_rx)
		{
			// Exception Case
		}
		m_prev_path = cur_rx;
		cur_rx = AMP_PATH_NONE;

		if(cur_tx != AMP_PATH_NONE)
		{
			new_path = cur_tx;
		}
	}
	m_curr_path = new_path;

#if 0 // default Speaker Switch Status : Enable -> Disable
#ifdef WM8993_CHECK_SPEAKERSWITCH
	if((path == AMP_PATH_HANDSET) && (atomic_read(&wm8993->isspksw) == 1))
	{
		APM_INFO("HANDSET[%d]-Enable SpeakerSwitch\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 0);
		atomic_set(&wm8993->isspksw, 0);
	}
#else
	if((path == AMP_PATH_HANDSET) && (gpio_get_value(WM8993_SPK_SW_GPIO) == 1))
	{
		APM_INFO("HANDSET[%d]-SPK_SW_GPIO:LOW\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 0);
	}
#endif /*WM8993_CHECK_SPEAKERSWITCH*/
#else
	if (gpio_get_value(WM8993_SPK_SW_GPIO) != 1)
	{
		APM_INFO("Disable Speaker Switch : %d\n", path);
		gpio_set_value(WM8993_SPK_SW_GPIO, 1);
		usleep(100);
	}
#endif

	if(new_path != AMP_PATH_NONE && (isvoice != 1))
	{
		const wm8993_register_type *amp_regs = NULL;

		m_curr_cal = AMP_CAL_NORMAL;
		new_cal = m_curr_cal;
		APM_INFO("new apply path = %d -> %d, caltype = %d\n", path, new_path, new_cal);

		amp_regs = amp_sequence_path[new_path][new_cal];
		if(amp_regs == NULL)
		{
			mutex_unlock(&wm8993->path_lock);
			APM_INFO("not support path on wm8993 path = %d, cal = %d\n", new_path, new_cal);
			return;
		}

		if(reset) wm8993_reset(path);
		wm8993_set_register((wm8993_register_type *)amp_regs);
	}
	else
	{
		APM_INFO("Calling : apply path = %d -> %d, caltype = %d\n", path, new_path, new_cal);
#ifdef WM8993_STARTUPDOWN_CONTROL
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
		if(new_path == AMP_PATH_NONE) wm8993_reset(path);
#else
		wm8993_set_register((wm8993_register_type *)amp_shutdown_sequence_path[path]);
#endif
#else
#ifdef WM8993_POP_NOIZE_ZERO // DAC SOFT MUTE AND SOFT UN-MUTE
		if((path != AMP_PATH_MAINMIC) && (path != AMP_PATH_EARMIC)) // only rx devices
		{
			u8 dacctrl_addr = 0x0a;
			unsigned short dacctrl_value = 0x0004;
			wm8993_write(dacctrl_addr, dacctrl_value);
			msleep(1); // delay = 0.5625ms
		} while(0);
#endif /* WM8993_POP_NOIZE_ZERO */
		if(new_path == AMP_PATH_NONE) wm8993_reset(path);
#endif
	}

	mutex_unlock(&wm8993->path_lock);
#endif /*WM8993_SINGLE_PATH*/
}
EXPORT_SYMBOL(wm8993_disable);

void wm8993_reset(AMP_PATH_TYPE_E path)
{
#ifdef WM8993_DEFAULT_REGISTER_RESTORE
    const wm8993_register_type *amp_regs = amp_sequence_path[path][m_prev_cal];

	APM_INFO("setting register path = %d cal = %d\n", path, m_prev_cal);
    wm8993_restore_defaultregister((wm8993_register_type *)amp_regs);
#else
	struct snddev_wm8993 *wm8993 = &wm8993_modules;

#ifdef WM8993_CHECK_RESET // Reset 중복 방지를 위해 체크한다.
	// 레지스터 설정시 isreset은 0으로 변경한다.
	// 레지스터 리셋시 신규 레지스터를 설정한 경우만 리셋처리한다.
	if (atomic_read(&wm8993->isreset) == 0)
	{
		atomic_set(&wm8993->isreset, 1);
		wm8993_write(WM8993_RESET_REG, WM8993_RESET_VALUE);
		msleep(1);
	}
#else
	wm8993_write(WM8993_RESET_REG, WM8993_RESET_VALUE);
	msleep(1);
#endif /*WM8993_CHECK_RESET*/
	APM_INFO("========== WM8993 RESET ==========\n");
#endif
}
EXPORT_SYMBOL(wm8993_reset);

/********************************************************************/
/* WM8993 I2C Driver */
/********************************************************************/

/**
 * wm8993_write - Sets register in WM8993
 * @param wm8993: wm8993 structure pointer passed by client
 * @param reg: register address
 * @param value: buffer values to be written
 * @param num_bytes: n bytes to write
 *
 * @returns result of the operation.
 */
int wm8993_write(u8 reg, unsigned short value)
{
	int rc, retry;
	unsigned char buf[3];
	struct i2c_msg msg[2];
	struct snddev_wm8993 *wm8993;

	wm8993 = &wm8993_modules;

	mutex_lock(&wm8993->xfer_lock);

	memset(buf, 0x00, sizeof(buf));

	buf[0] = (reg & 0x00FF);
	buf[1] = (value & 0xFF00)>>8;
	buf[2] = (value & 0x00FF);

	msg[0].addr = wm8993->client->addr;
	msg[0].flags = 0;
	msg[0].len = 3;
	msg[0].buf = buf;

	for (retry = 0; retry <= 2; retry++)
	{
		rc = i2c_transfer(wm8993->client->adapter, msg, 1);
		if(rc > 0)
		{
			break;
		}
		else
		{
			pr_err("wm8993 i2c write failed. [%d], addr = 0x%x, val = 0x%x\n", retry, reg, value);
			msleep(10);
		}
	}

	mutex_unlock(&wm8993->xfer_lock);

	return rc;
} 

/**
 * wm8993_read - Reads registers in WM8993
 * @param wm8993: wm8993 structure pointer passed by client
 * @param reg: register address
 * @param value: i2c read of the register to be stored
 * @param num_bytes: n bytes to read.
 * @param mask: bit mask concerning its register
 *
 * @returns result of the operation.
 */
int wm8993_read(u8 reg, unsigned short *value)
{
	int rc, retry;
	struct i2c_msg msg[2];
	struct snddev_wm8993 *wm8993;

	wm8993 = &wm8993_modules;

	mutex_lock(&wm8993->xfer_lock);

	value[0] = (reg & 0x00FF);

	msg[0].addr = wm8993->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = (u8*)value;

	msg[1].addr = wm8993->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = (u8*)value;

	for (retry = 0; retry <= 2; retry++)
	{
		rc = i2c_transfer(wm8993->client->adapter, msg, 2);
		if(rc > 0)
		{
			break;
		}
		else
		{
			pr_err("wm8993 i2c read failed. [%d], addr=0x%x, val=0x%x\n", retry, reg, *value);
			msleep(10);
		}
	}

	*value = ((*value) >> 8 ) | (((*value) & 0x00FF) << 8); // switch MSB , LSB

	mutex_unlock(&wm8993->xfer_lock);

	return rc;
}

int wm8993_update(unsigned short reg, unsigned short mask, unsigned short value)
{
	short change;
	unsigned short old, new;
	int ret;

	ret = wm8993_read(reg, & old);
  if (ret < 0)
    return ret;

	new = (old & ~mask) | value;
	change = old != new;
	if (change) {
		ret = wm8993_write(reg, new);
		if (ret < 0)
			return ret;
	}

	return change;
}

static long wm8993_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct snddev_wm8993 *wm8993 = file->private_data;
	int rc = 0;

	pr_debug("%s\n", __func__);

	switch (cmd) {
		case AUDIO_START:
			{
				APM_INFO("AUDIO_START\n");
				wm8993_enable_amplifier();
			}
			break;

		case AUDIO_STOP:
			{
				APM_INFO("AUDIO_STOP\n");
				wm8993_disable_amplifier();
			}
			break;

		case AUDIO_SET_CONFIG:
			{
				struct msm_audio_config config;

				if (copy_from_user(&config, (void *) arg, sizeof(config))) {
					rc = -EFAULT;
					break;
				}
				APM_INFO("AUDIO_SET_CONFIG : Type %d\n", config.type);

				mutex_lock(&wm8993->lock);
				if (config.type == 0) {
					wm8993->noisegate_enabled = config.channel_count;
					APM_INFO("Unsupporte Type. Set Noisegate %s\n", 
							wm8993->noisegate_enabled ? "ON" : "OFF");
				} else if (config.type == 1) {
					wm8993->cal_type = config.channel_count;;
					APM_INFO("Set Calibration type = %d\n", wm8993->cal_type);
					if(wm8993->cal_type == 0) {
						wm8993_set_caltype(AMP_CAL_NORMAL);
					} else if(wm8993->cal_type == 1) {
						wm8993_set_caltype(AMP_CAL_VOICECALL);
					} else if(wm8993->cal_type == 2) {
						wm8993_set_caltype(AMP_CAL_VIDEOCALL);
					} else if(wm8993->cal_type == 3) {
						wm8993_set_caltype(AMP_CAL_VOIPCALL);
					} else if(wm8993->cal_type == 4) {
						wm8993_set_caltype(AMP_CAL_LOOPBACK);
					} else {
						wm8993_set_caltype(AMP_CAL_NORMAL);
					}
				} else if (config.type == 2) {
					wm8993->spksw_enabled = config.channel_count;
					APM_INFO("Set Speaker Switch %s\n",  
							wm8993->spksw_enabled ? "ON" : "OFF");

					APM_INFO("Current Device = %d, Speaker Switch State = %d\n", 
							m_curr_path, gpio_get_value(WM8993_SPK_SW_GPIO));
					if (m_curr_path > AMP_PATH_NONE && m_curr_path < AMP_PATH_MAINMIC) // RX Devices
					{
						if (wm8993->spksw_enabled == 1)
						{
							if (m_curr_path == AMP_PATH_HANDSET)
							{
								if (gpio_get_value(WM8993_SPK_SW_GPIO) != 1)
								{
									APM_INFO("Current Path = %d -> Disable Speaker Switch\n",
											m_curr_path);
									gpio_set_value(WM8993_SPK_SW_GPIO, 1);
								}
							}
							else
							{
								if (gpio_get_value(WM8993_SPK_SW_GPIO) != 0)
								{
									APM_INFO("Current Path = %d -> Enable Speaker Switch\n",
											m_curr_path);
									gpio_set_value(WM8993_SPK_SW_GPIO, 0);
								}
							}
						}
						else
						{
							if (m_curr_path != AMP_PATH_HANDSET)
							{
								if (gpio_get_value(WM8993_SPK_SW_GPIO) != 1)
								{
									APM_INFO("Current Path = %d -> Disable Speaker Switch\n",
											m_curr_path);
									gpio_set_value(WM8993_SPK_SW_GPIO, 1);
								}
							}
						}
					}
				}
				mutex_unlock(&wm8993->lock);
			}
			break;

		case AUDIO_GET_CONFIG:
			{
				struct msm_audio_config config;
				pr_debug("%s: AUDIO_GET_CONFIG\n", __func__);

				memset(&config, 0x00, sizeof(struct msm_audio_config));

				config.buffer_size = wm8993->noisegate_enabled; // Noisegate
				config.buffer_count = wm8993->cal_type; // Cal Type
				config.channel_count = wm8993->spksw_enabled; // Speaker Switch
				config.sample_rate = (uint32_t)atomic_read(&wm8993->amp_enabled); // Amp State
				config.type = (uint32_t)atomic_read(&wm8993->isreset); // Reset State

				if (copy_to_user((void *) arg, &config, sizeof(config)))
					rc = -EFAULT;
			}
			break;

		case AUDIO_SET_EQ:
			{
				struct msm_audio_config config;
				pr_debug("%s: AUDIO_SET_CONFIG\n", __func__);

				if (copy_from_user(&config, (void *) arg, sizeof(config))) {
					rc = -EFAULT;
					break;
				}

				mutex_lock(&wm8993->lock);
				wm8993->eq_type = config.type;
				mutex_unlock(&wm8993->lock);
				APM_INFO("Set EQ type = %d\n", wm8993->eq_type);
			}
			break;

		case AUDIO_SET_MUTE:
			{
				struct msm_audio_config config;
				pr_debug("%s: AUDIO_SET_MUTE\n", __func__);

				if (copy_from_user(&config, (void *) arg, sizeof(config))) {
					rc = -EFAULT;
					break;
				}

				mutex_lock(&wm8993->lock);
				wm8993->mute_enabled = config.type;
				mutex_unlock(&wm8993->lock);
				APM_INFO("Set Mute type = %d\n", wm8993->mute_enabled);
#if 0 // alsa api
				if(wm8993->mute_enabled == 1)
					msm_set_voice_tx_mute(1);
				else
					msm_set_voice_tx_mute(0);
#else
				// TODO: Set WM8993 Mute Register
#endif
			}
			break;

		case AUDIO_SET_VOLUME:
			{
				struct msm_audio_config config;
				APM_INFO("AUDIO_SET_VOLUME\n");

				if (copy_from_user(&config, (void *) arg, sizeof(config))) {
					rc = -EFAULT;
					break;
				}

				mutex_lock(&wm8993->lock);
				wm8993->voip_micgain = config.type;
				mutex_unlock(&wm8993->lock);
				APM_INFO("Set Voip Mic Gain type = %d\n", wm8993->voip_micgain);
#if 0 // alsa api
				if(wm8993->voip_micgain > 0)
					msm_set_voice_rx_vol(wm8993->voip_micgain);
#else
				// TODO: Set WM8993 Volume Register
#endif
			}
			break;

		default:
			rc = -EINVAL;
			break;
	}

	return rc;
}

static int wm8993_open(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	file->private_data = &wm8993_modules;
	return 0;
}

static int wm8993_release(struct inode *inode, struct file *file)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static struct file_operations wm8993_dev_fops = {
	.owner      = THIS_MODULE,
	.open		= wm8993_open,
	.release	= wm8993_release,
	.unlocked_ioctl = wm8993_ioctl,
};

struct miscdevice wm8993_control_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "snddev_wm8993",
	.fops   = &wm8993_dev_fops,
};

static int wm8993_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct wm8993_platform_data *pdata = client->dev.platform_data;
	struct snddev_wm8993 *wm8993;
	int status;

	pr_debug("%s\n", __func__);

	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0)
	{
		dev_err(&client->dev, "can't talk I2C?\n");
		return -ENODEV;
	}

	if (pdata->wm8993_setup != NULL)
	{
		status = pdata->wm8993_setup(&client->dev);
		if (status < 0)
		{
			pr_err("wm8993_probe: wm8993  setup power failed\n");
			return status;
		}
	}

	wm8993 = &wm8993_modules;
	wm8993->client = client;
	strlcpy(wm8993->client->name, id->name, sizeof(wm8993->client->name));
	mutex_init(&wm8993->xfer_lock);
	mutex_init(&wm8993->path_lock);
	mutex_init(&wm8993->lock);
	wm8993->cal_type = 0;
	wm8993->eq_type = 0;
	wm8993->mute_enabled = 0;
	wm8993->voip_micgain = 0;
	wm8993->noisegate_enabled = 1; // Only YDA165
	wm8993->spksw_enabled = 0;
#ifdef WM8993_CHECK_RESET
	atomic_set(&wm8993->isreset, 0);
#endif /*WM8993_CHECK_RESET*/
#ifdef WM8993_DIRECT_AMP_CONTROL
	atomic_set(&wm8993->amp_enabled, 0);
#endif /*WM8993_DIRECT_AMP_CONTROL*/
	wm8993->suspend_poweroff = 0;

#ifndef WM8993_SINGLE_PATH // 20110510 by ssgun
	cur_tx = 0;
	cur_rx = 0;
#endif /*WM8993_SINGLE_PATH*/

	status = misc_register(&wm8993_control_device);
	if (status)
	{
		pr_err("wm8993_probe: wm8993_control_device register failed\n");
		return status;
	}

#if 0 // 20110305 by ssgun - test code
	add_child(0x1A, "wm8993_codec", id->driver_data, NULL, 0);
#endif

#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
	wm8993_set_masterbias_level(WM8993_MASTER_BIAS_STANDBY);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int wm8993_suspend(struct i2c_client *client, pm_message_t mesg)
{
	if (m_curr_path == AMP_PATH_NONE)
	{
		struct wm8993_platform_data *pdata;
		struct snddev_wm8993 *wm8993;

		pdata = client->dev.platform_data;
		wm8993 = &wm8993_modules;

		if (pdata->wm8993_shutdown != NULL)
		{
#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
			wm8993_set_masterbias_level(WM8993_MASTER_BIAS_OFF);
#endif

			pdata->wm8993_shutdown(&client->dev);
			wm8993->suspend_poweroff = 1;
			APM_INFO("Current Path : %d\n", m_curr_path);
		}
	}

	return 0;
}

static int wm8993_resume(struct i2c_client *client)
{
	struct wm8993_platform_data *pdata;
	struct snddev_wm8993 *wm8993;
	int status;

	wm8993 = &wm8993_modules;

	if (wm8993->suspend_poweroff == 1)
	{
		pdata = client->dev.platform_data;

		if (pdata->wm8993_setup != NULL)
		{
			status = pdata->wm8993_setup(&client->dev);
			if (status < 0)
			{
				pr_err("%s : %d, fail PowerOn = %d\n", __func__, m_curr_path, status);
				return status;
			}
			wm8993->suspend_poweroff = 0;
			APM_INFO("Current Path : %d\n", m_curr_path);

#ifdef WM8993_STARTUPDOWN_CONTROL_CASE02
			wm8993_set_masterbias_level(WM8993_MASTER_BIAS_STANDBY); 
#endif
		}
	}
	return 0;
}
#endif

static int __devexit wm8993_remove(struct i2c_client *client)
{
	struct wm8993_platform_data *pdata;

	pr_debug("%s\n", __func__);

	pdata = client->dev.platform_data;

	i2c_unregister_device(wm8993_modules.client);
	wm8993_modules.client = NULL;

	if (pdata->wm8993_shutdown != NULL)
		pdata->wm8993_shutdown(&client->dev);

	misc_deregister(&wm8993_control_device);

	return 0;
}

static struct i2c_device_id wm8993_id_table[] = {
	{"wm8993", 0x0},
	{}
};
MODULE_DEVICE_TABLE(i2c, wm8993_id_table);

static struct i2c_driver wm8993_driver = {
	.driver			= {
		.owner		=	THIS_MODULE,
		.name		= 	"wm8993",
	},
	.id_table		=	wm8993_id_table,
	.probe			=	wm8993_probe,
#ifdef CONFIG_PM
	.suspend		=	wm8993_suspend,
	.resume			=	wm8993_resume,
#endif
	.remove			=	__devexit_p(wm8993_remove),
};

#if 0 // 20110416 by ssgun - GPIO_CFG_NO_PULL -> GPIO_CFG_PULL_DOWN
#define SPK_SW_CTRL_0 \
	GPIO_CFG(WM8993_SPK_SW_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA)
#else
#define SPK_SW_CTRL_0 \
	GPIO_CFG(WM8993_SPK_SW_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#endif

static int __init wm8993_codec_init(void)
{
	int rc;

	pr_debug("%s\n", __func__);

	rc = gpio_tlmm_config(SPK_SW_CTRL_0, GPIO_CFG_ENABLE);
	if (rc)
	{
		pr_err("%s] gpio  config failed: %d\n", __func__, rc);
		goto fail;
	}

	rc = i2c_add_driver(&wm8993_driver);
	return rc;

fail:
	return -ENODEV;
}
module_init(wm8993_codec_init);

static void __exit wm8993_codec_exit(void)
{
	pr_debug("%s\n", __func__);

	i2c_del_driver(&wm8993_driver);
}
module_exit(wm8993_codec_exit);

MODULE_DESCRIPTION("KTTECH's WM8993 Codec Sound Device driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Gun Song <ssgun@kttech.co.kr>");
