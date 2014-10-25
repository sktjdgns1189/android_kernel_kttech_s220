/*
*  mipi_novatek_bl.c - TPS61161 Backlight Controller
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

#include "msm_fb.h"
#include "mipi_novatek_bl.h"

atomic_t enter_easyscale_set;
DEFINE_MUTEX(tps6116_bl);

/* Backlight Debugging Information */
int debug = 0; // 1;

void tps61161_set_bl_native(int lcd_backlight_level)
{
	int i,j, bit_coding;
	
	klogi_if("[BACKLIGHT] TPS61161 Native Baclight Level : %d", lcd_backlight_level);
	
	mutex_lock(&tps6116_bl);

	/* Backlight Off */
	if(lcd_backlight_level == 0) {
		atomic_set(&enter_easyscale_set, 1);
		preempt_disable();
		gpio_set_value(BL_ON, 0);
	 	mdelay(1);
		preempt_enable();
	}
	else {
		/* Set backlight Brightness */
		if ((atomic_read(&enter_easyscale_set)) == 1) {
			preempt_disable();
			gpio_set_value(BL_ON, 0);
		 	mdelay(1);			
			EASYSCALE_MODE_SET();
			mdelay(1);
			preempt_enable();
			atomic_set(&enter_easyscale_set, 0);
		}

		/* Workaround : Try re-initialize BL level for TPS61161 */
		for (j=0; j<1; j++) {
		preempt_disable();

		T_START();

		TPS61161_DEVICE_ID();

		T_EOS();	

		T_START();

		LOW_BIT();  // 0 Request for Ack
		LOW_BIT();  // 0 Address bit 1
		LOW_BIT();  // 0 Address bit 0

		for(i = BL_DIMING_DATA_CNT; i >= 0; i--)
		{
			bit_coding = ((lcd_backlight_level >> i) & 0x1); // (lcd_backlight_level & (2 ^ i));

			if(bit_coding) {
				HIGH_BIT(); // Data High
			} else {
				LOW_BIT();  // Data Low
			}
		}

		T_EOS();
		STATIC_HIGH();

			mdelay(1);

		preempt_enable();
	}
	}
	
	mutex_unlock(&tps6116_bl);

	return;
}
EXPORT_SYMBOL(tps61161_set_bl_native);

void tps61161_set_bl(struct msm_fb_data_type *mfd)
{
	tps61161_set_bl_native(mfd->bl_level);
}
EXPORT_SYMBOL(tps61161_set_bl);

static int __init tps61161_init(void)
{
	int rc = 0;

	rc += gpio_request(BL_ON, "tps61161_init");

	mutex_init(&tps6116_bl);
	atomic_set(&enter_easyscale_set, 0);

	if(!rc) 
		klogi("[BACKLIGHT] LCD Backlight controller initialize succeed.\n");
		
	return rc;
}

static void __exit tps61161_exit(void)
{
	gpio_free(BL_ON);
}

module_init(tps61161_init);
module_exit(tps61161_exit);

