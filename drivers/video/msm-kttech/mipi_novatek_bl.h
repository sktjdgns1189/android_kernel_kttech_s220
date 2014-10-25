/*
*  mipi_novatek_bl.h - TPS61161 Backlight Controller
*
*  Version 0.1a
*
*  Copyright (C) 2012-2010 KT Tech Inc.
*  Author : JhoonKim <jhoonkim@kttech.co.kr>
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

#ifndef TPS61161_BL_CTRL_KTTECH_H
#define TPS61161_BL_CTRL_KTTECH_H

#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <mach/pmic.h>
#include <asm/atomic.h>

/* Device Information */
#define BL_DIMING_DATA_CNT	4				// Protocol Length
#define BL_DELAY_1		3					// Backlight signal delay - positive
#define BL_DELAY_2		(BL_DELAY_1 << 1)	// Backlight signal delay - nagative

/* Backlight GPIO */
#define BL_ON			(31)

/* EasyScale mode setting */
#define EASYSCALE_MODE_SET()	\
{ 								\
	gpio_set_value(BL_ON, 1);	\
	udelay(120);				\
	gpio_set_value(BL_ON, 0); 	\
	udelay(280);				\
	gpio_set_value(BL_ON, 1);	\
}

/* Device Address : 0x72 */
#define TPS61161_DEVICE_ID()	\
{ 								\
	LOW_BIT();					\
	HIGH_BIT();					\
	HIGH_BIT();					\
	HIGH_BIT();					\
	LOW_BIT();					\
	LOW_BIT();					\
	HIGH_BIT();					\
	LOW_BIT();					\
}

#define HIGH_BIT()				\
{								\
	gpio_set_value(BL_ON, 0);	\
	udelay(BL_DELAY_1);			\
	gpio_set_value(BL_ON, 1);	\
	udelay(BL_DELAY_2);			\
}

#define LOW_BIT() 				\
{ 								\
	gpio_set_value(BL_ON, 0);	\
	udelay(BL_DELAY_2);			\
	gpio_set_value(BL_ON, 1);	\
	udelay(BL_DELAY_1);			\
}

#define T_START()				\
{								\
	gpio_set_value(BL_ON, 1);	\
	udelay(BL_DELAY_2);			\
}

#define T_EOS()   				\
{ 								\
	gpio_set_value(BL_ON, 0);	\
	udelay(BL_DELAY_2);			\
}

#define STATIC_HIGH()			\
{								\
	gpio_set_value(BL_ON, 1);	\
}

/* Debugging Information */
#define klogi(fmt, arg...)  printk(KERN_INFO "%s: " fmt "\n" , __func__, ## arg)
#define kloge(fmt, arg...)  printk(KERN_ERR "%s(%d): " fmt "\n" , __func__, __LINE__, ## arg)
#define klogi_if(fmt, arg...) if (debug) printk(KERN_INFO "%s: " fmt "\n" , __func__, ## arg)
#define kloge_if(fmt, arg...) if (debug) printk(KERN_ERR "%s(%d): " fmt "\n" , __func__, __LINE__, ## arg)

void tps61161_set_bl(struct msm_fb_data_type *mfd);
void tps61161_set_bl_native(int lcd_backlight_level);
void mipi_pwm_backlight(int on);

#endif /* TPS61161_BL_CTRL_KTTECH_H */
