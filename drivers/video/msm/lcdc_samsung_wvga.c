/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

 #undef CONFIG_SPI_QUP

#include <linux/delay.h>
#include <linux/pwm.h>
#ifdef CONFIG_SPI_QUP
#include <linux/spi/spi.h>
#else
#include <mach/gpio.h>
#include <linux/gpio.h>
#endif
#include "msm_fb.h"
#include "tps61161_bl.h"

//#define DEBUG
#define LCDC_CABC
/* #define SYSFS_DEBUG_CMD */
#define DEFAULT_DIM 	11
extern u32 msm_fb_debug_enabled;

#ifdef CONFIG_SPI_QUP
#define LCDC_SAMSUNG_SPI_DEVICE_NAME	"lcdc_samsung_db7430"
static struct spi_device *lcdc_spi_client;
#else
static int spi_cs = 0;
static int spi_sclk = 0;
static int spi_sdi = 0;
static int spi_mosi = 0;
#endif

struct samsung_state_type {
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
	int brightness;
	int poweron_brightness;
};

struct samsung_spi_data {
	u8 addr;
	u8 len;
	u8 data[38];
};

static struct samsung_spi_data init_sequence1[] = {
	{ .addr = 0x36, .len =  1, .data = { 0x0a } },
	{ .addr = 0xb0, .len =  1, .data = { 0x00 } },
	{ .addr = 0xc0, .len =  2, .data = { 0x28, 0x08 }},
	{ .addr = 0xc1, .len =  5, .data = { 0x01, 0x30, 0x15, 0x05, 0x22 }},	
	{ .addr = 0xc4, .len =	3, .data = { 0x10, 0x01, 0x00 }},	
	{ .addr = 0xc5, .len =	9, .data = { 0x06, 0x55, 0x03, 0x07, 0x0b, 0x33,
	0x00, 0x01, 0x03 } },
	{ .addr = 0xc6, .len =  1, .data = { 0x01 } },
	{ .addr = 0xc8, .len = 38, .data = { 0x00, 0x42, 0x10, 0x1d, 0x31, 0x3a,
	 0x3a, 0x3d, 0x3b, 0x42, 0x49, 0x48, 0x48, 0x44, 0x48, 0x59, 0x55, 0x57, 
	 0x14, 0x00, 0x42, 0x10, 0x1d, 0x31, 0x3a, 0x3a, 0x3d, 0x3b, 0x42, 0x49,
	 0x48, 0x48, 0x44, 0x48, 0x59, 0x55, 0x57, 0x14} },	
	{ .addr = 0xc9, .len = 38, .data = { 0x00, 0x38, 0x14, 0x20, 0x35, 0x3e,
	 0x40, 0x44, 0x42, 0x48, 0x4f, 0x4e, 0x4e, 0x4a, 0x4d, 0x68, 0x5c, 0x5c,
	 0x16, 0x00, 0x38, 0x14, 0x20, 0x35, 0x3e, 0x40, 0x44, 0x42, 0x48, 0x4f,
	 0x4e, 0x4e, 0x4a, 0x4d, 0x68, 0x5c, 0x5c, 0x16} },
	{ .addr = 0xca, .len = 38, .data = { 0x00, 0x6b, 0x0b, 0x12, 0x22, 0x28,
	 0x28, 0x2b, 0x2a, 0x33, 0x3b, 0x3b, 0x3d, 0x3b, 0x44, 0x5d, 0x51, 0x51,
	 0x18, 0x00, 0x6b, 0x0b, 0x12, 0x22, 0x28, 0x28, 0x2b, 0x2a, 0x33, 0x3b, 
	 0x3b, 0x3d, 0x3b, 0x44, 0x5d, 0x51, 0x51, 0x18} },
};

static struct samsung_spi_data init_sequence[] = {		 
	{ .addr = 0xd1, .len =	2, .data = { 0x33, 0x13 }},
	{ .addr = 0xd2, .len =	3, .data = { 0x11, 0x00, 0x00 }},
	{ .addr = 0xd3, .len =	2, .data = { 0x50, 0x50 }},
	{ .addr = 0xd5, .len =	4, .data = { 0x2f, 0x11, 0x1e, 0x46 }},	
	{ .addr = 0xd6, .len =	2, .data = { 0x11, 0x0a }},	
};


#ifdef LCDC_CABC //defined(CONFIG_KTTECH_BOARD_O4_ES2)
static boolean fisrt_bl_on = TRUE;

static struct samsung_spi_data cabc_on_sequence[] = {
	{ .addr = 0xb4, .len =  3, .data = { 0x0f, 0x00, 0x50 } },
	{ .addr = 0xb5, .len =  1, .data = { 0x5d } },  // user select		
	{ .addr = 0xb7, .len =  1, .data = { 0x24 } },
	{ .addr = 0xb8, .len =  1, .data = { 0x01 } },		
};

static struct samsung_spi_data bl_ctl_sequence[] = {
	{ .addr = 0xb5, .len =  1, .data = { 0x5d } },  // user select
};

static struct samsung_spi_data cabc_off_sequence[] = {
	{ .addr = 0xb7, .len =  1, .data = { 0x00 } },
	{ .addr = 0xb8, .len =  1, .data = { 0x00 } },		
};

// pwm manual setting without cabc
static struct samsung_spi_data manual_bl_on_sequence[] = {
	{ .addr = 0xb4, .len =  3, .data = { 0x0f, 0x00, 0x50 } },
	{ .addr = 0xb5, .len =  1, .data = { 0x5d } },  // user select		
	{ .addr = 0xb7, .len =  1, .data = { 0x24 } },		
	{ .addr = 0xfa, .len = 23, .data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0xff }},
};

static struct samsung_spi_data manual_bl_ctl_sequence[] = {
	{ .addr = 0xfa, .len = 23, .data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0xff }},
};

static struct samsung_spi_data manual_cabc_on_sequence[] = {
	{ .addr = 0xfa, .len = 23, .data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00 }},		
	{ .addr = 0xb5, .len =  1, .data = { 0x5d } },  // user select			
	{ .addr = 0xb8, .len =	1, .data = { 0x01 } },	
};

static struct samsung_spi_data manual_cabc_off_sequence[] = {
	{ .addr = 0xb8, .len =	1, .data = { 0x00 } },	
	{ .addr = 0xfa, .len = 23, .data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	 0x00, 0x00, 0x00, 0x00, 0xff }},		
};

static uint32 cabc_enable = 1; 

static DEFINE_MUTEX(bl_set_lock);

#endif

static struct samsung_state_type samsung_state = { .brightness = 0 };
static struct msm_panel_common_pdata *lcdc_samsung_pdata;

#ifndef CONFIG_SPI_QUP
static void samsung_spi_write_byte(boolean dc, u8 data)
{
	uint32 bit;
	int bnum;

	gpio_set_value(spi_sclk, 0);
	gpio_set_value(spi_mosi, dc ? 1 : 0);
	udelay(3);			/* at least 20 ns */
	gpio_set_value(spi_sclk, 1);	/* clk high */
	udelay(3);			/* at least 20 ns */

	bnum = 8;			/* 8 data bits */
	bit = 0x80;
	while (bnum--) {
		gpio_set_value(spi_sclk, 0); /* clk low */
		gpio_set_value(spi_mosi, (data & bit) ? 1 : 0);
		udelay(3);
		gpio_set_value(spi_sclk, 1); /* clk high */
		udelay(3);
		bit >>= 1;
	}
	gpio_set_value(spi_mosi, 0);

}

#ifdef DEBUG
static void samsung_spi_read_bytes(u8 cmd, u8 *data, int num)
{
	int bnum;

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(3);

	/* command byte first */
	samsung_spi_write_byte(0, cmd);
	udelay(3);

	//gpio_direction_input(spi_sdi);

	if (num > 1) {
		/* extra dummy clock */
		gpio_set_value(spi_sclk, 0);
		udelay(2);
		gpio_set_value(spi_sclk, 1);
		udelay(2);
	}

	/* followed by data bytes */
	bnum = num * 8;	/* number of bits */
	*data = 0;
	while (bnum) {
		gpio_set_value(spi_sclk, 0); /* clk low */
		udelay(1);
		*data <<= 1;
		*data |= gpio_get_value(spi_sdi) ? 1 : 0;
		gpio_set_value(spi_sclk, 1); /* clk high */
		udelay(1);
		--bnum;
		if ((bnum % 8) == 0)
			++data;
	}

	//gpio_direction_output(spi_sdi, 0);

	/* Chip Select - high */
	udelay(3);
	gpio_set_value(spi_cs, 1);
}
#endif
#endif

#if 0//def DEBUG
static const char *byte_to_binary(const u8 *buf, int len)
{
	static char b[32*8+1];
	char *p = b;
	int i, z;

	for (i = 0; i < len; ++i) {
		u8 val = *buf++;
		for (z = 1 << 7; z > 0; z >>= 1)
			*p++ = (val & z) ? '1' : '0';
	}
	*p = 0;

	return b;
}
#endif

#define BIT_OFFSET	(bit_size % 8)
#define ADD_BIT(val) do { \
		tx_buf[bit_size / 8] |= \
			(u8)((val ? 1 : 0) << (7 - BIT_OFFSET)); \
		++bit_size; \
	} while (0)

#define ADD_BYTE(data) do { \
		tx_buf[bit_size / 8] |= (u8)(data >> BIT_OFFSET); \
		bit_size += 8; \
		if (BIT_OFFSET != 0) \
			tx_buf[bit_size / 8] |= (u8)(data << (8 - BIT_OFFSET));\
	} while (0)

static int samsung_serigo(struct samsung_spi_data data)
{
#ifdef CONFIG_SPI_QUP
	char                tx_buf[32];
	int                 bit_size = 0, i, rc;
	struct spi_message  m;
	struct spi_transfer t;

	if (!lcdc_spi_client) {
		pr_err("%s lcdc_spi_client is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	spi_setup(lcdc_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ADD_BIT(FALSE);
	ADD_BYTE(data.addr);
	for (i = 0; i < data.len; ++i) {
		ADD_BIT(TRUE);
		ADD_BYTE(data.data[i]);
	}

	/* add padding bits so we round to next byte */
	t.len = (bit_size+7) / 8;
	if (t.len <= 4)
		t.bits_per_word = bit_size;

	rc = spi_sync(lcdc_spi_client, &m);
#ifdef DEBUG
	pr_info("%s: addr=0x%02x, #args=%d[%d] [%s], rc=%d\n",
		__func__, data.addr, t.len, t.bits_per_word,
		byte_to_binary(tx_buf, t.len), rc);
#endif
	return rc;
#else
	int i;

	lcdc_samsung_pdata->panel_config_gpio(1);

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(3);

	samsung_spi_write_byte(FALSE, data.addr);
	udelay(3);

	for (i = 0; i < data.len; ++i) {
		samsung_spi_write_byte(TRUE, data.data[i]);
		udelay(3);
	}

	/* Chip Select - high */
	gpio_set_value(spi_cs, 1);
#ifdef DEBUG
	pr_info("%s: cmd=0x%02x, #args=%d\n", __func__, data.addr, data.len);
#endif
	return 0;
#endif
}

static int samsung_write_cmd(u8 cmd)
{
#ifdef CONFIG_SPI_QUP
	char                tx_buf[2];
	int                 bit_size = 0, rc;
	struct spi_message  m;
	struct spi_transfer t;

	if (!lcdc_spi_client) {
		pr_err("%s lcdc_spi_client is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	spi_setup(lcdc_spi_client);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ADD_BIT(FALSE);
	ADD_BYTE(cmd);

	t.len = 2;
	t.bits_per_word = 9;

	rc = spi_sync(lcdc_spi_client, &m);
#ifdef DEBUG
	pr_info("%s: addr=0x%02x, #args=%d[%d] [%s], rc=%d\n",
		__func__, cmd, t.len, t.bits_per_word,
		byte_to_binary(tx_buf, t.len), rc);
#endif
	return rc;
#else

	lcdc_samsung_pdata->panel_config_gpio(1);

	/* Chip Select - low */
	gpio_set_value(spi_cs, 0);
	udelay(4);

	samsung_spi_write_byte(FALSE, cmd);

	/* Chip Select - high */
	udelay(4);
	gpio_set_value(spi_cs, 1);
#ifdef DEBUG
	pr_info("%s: cmd=0x%02x\n", __func__, cmd);
#endif
	return 0;
#endif
}

static int samsung_serigo_list(struct samsung_spi_data *data, int count)
{
	int i, rc;	
	for (i = 0; i < count; ++i, ++data) {
		rc = samsung_serigo(*data);
		if (rc)
			return rc;
		msleep(15);
	}
	return 0;
}

#ifndef CONFIG_SPI_QUP
static void samsung_spi_init(void)
{
	spi_sclk = *(lcdc_samsung_pdata->gpio_num);
	spi_cs   = *(lcdc_samsung_pdata->gpio_num + 1);
	spi_sdi = *(lcdc_samsung_pdata->gpio_num + 2);		
	spi_mosi = *(lcdc_samsung_pdata->gpio_num + 3);

	/* Set the output so that we don't disturb the slave device */
	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_mosi, 0);

	/* Set the Chip Select deasserted (active low) */
	gpio_set_value(spi_cs, 1);
}
#endif

static void samsung_disp_powerup(void)
{
	if (!samsung_state.disp_powered_up && !samsung_state.display_on)
		samsung_state.disp_powered_up = TRUE;
}

static struct work_struct disp_on_delayed_work;
static void samsung_disp_on_delayed_work(struct work_struct *work_ptr)
{
	if (msm_fb_debug_enabled) {
	pr_info("%s: start =[%d] =========== \n", __func__, __LINE__);
	}
	mutex_lock(&bl_set_lock);	



	/* Initializing Sequence */
	samsung_serigo_list(init_sequence,
		sizeof(init_sequence)/sizeof(*init_sequence));

	/* 0x11: Sleep Out */
	samsung_write_cmd(0x11);
	msleep(20);

	/* 0x29: Display On */
	samsung_write_cmd(0x29);

	if (msm_fb_debug_enabled) {
	pr_info("%s: end =[%d] =========== \n", __func__, __LINE__);
	}
	
#if !defined(CONFIG_SPI_QUP) && defined (DEBUG)
	{
		u8 data;

		msleep(120);
		/* 0x0A: Read Display Power Mode */
		samsung_spi_read_bytes(0x0A, &data, 1);
		pr_info("%s: power=[0x%x]\n", __func__, data);

		msleep(120);
		/* 0x0C: Read Display Pixel Format */
		samsung_spi_read_bytes(0x0C, &data, 1);
		pr_info("%s: pixel-format=[0x%x]\n", __func__, data);
	}
#endif

#ifdef LCDC_CABC //defined(CONFIG_KTTECH_BOARD_O4_ES2)
	if ((get_kttech_hw_version() >= ES2))
	{
		// 예외 - LCDC on 전에 BL을 on한 경우 
		#if 0
		 if (samsung_state.brightness > 0)
		{
			cabc_on_sequence[1].data[0] = samsung_state.brightness;
			samsung_serigo_list(cabc_on_sequence,
				sizeof(cabc_on_sequence)/sizeof(*cabc_on_sequence));			
		}
                else 
		#endif
		if (samsung_state.poweron_brightness > 0)
		{
			cabc_on_sequence[1].data[0] = samsung_state.poweron_brightness;
			samsung_serigo_list(cabc_on_sequence,
				sizeof(cabc_on_sequence)/sizeof(*cabc_on_sequence));			
		}
	}
	mutex_unlock(&bl_set_lock);	
	
#endif	

}

static void samsung_disp_on(void)
{
	if (samsung_state.disp_powered_up && !samsung_state.display_on) {
		/* Initializing Sequence */
		mutex_lock(&bl_set_lock);	

		samsung_serigo_list(init_sequence1,
			sizeof(init_sequence1)/sizeof(*init_sequence1));
		
		mutex_unlock(&bl_set_lock);	
		
//		if (fisrt_bl_on || (samsung_state.brightness > 0)) samsung_disp_on_delayed_work(NULL);
		if (fisrt_bl_on) samsung_disp_on_delayed_work(NULL);
		else			
		{
		INIT_WORK(&disp_on_delayed_work, samsung_disp_on_delayed_work);
		schedule_work(&disp_on_delayed_work);
		}
		samsung_state.display_on = TRUE;
	}
}

static int lcdc_samsung_panel_on(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	if (!samsung_state.disp_initialized) {
#ifndef CONFIG_SPI_QUP
		lcdc_samsung_pdata->panel_config_gpio(1);
		samsung_spi_init();
#endif
		samsung_disp_powerup();
		samsung_disp_on();
		samsung_state.disp_initialized = TRUE;
	}
	return 0;
}

static int lcdc_samsung_panel_off(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	if (samsung_state.disp_powered_up && samsung_state.display_on) {
		/* 0x28: Display Off */
		samsung_write_cmd(0x28);		
		/* 0x10: Sleep In */
		samsung_write_cmd(0x10);
		msleep(120);

		samsung_state.display_on = FALSE;
		samsung_state.disp_initialized = FALSE;
		samsung_state.brightness = 0;	
	}
	return 0;
}

#ifdef SYSFS_DEBUG_CMD
static ssize_t samsung_rda_cmd(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "n/a\n");
	pr_info("%s: 'n/a'\n", __func__);
	return ret;
}

static ssize_t samsung_wta_cmd(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	uint32 cmd;

	sscanf(buf, "%x", &cmd);
	samsung_write_cmd((u8)cmd);

	return ret;
}

static DEVICE_ATTR(cmd, S_IRUGO | S_IWUGO, samsung_rda_cmd, samsung_wta_cmd);
static struct attribute *fs_attrs[] = {
	&dev_attr_cmd.attr,
	NULL,
};
static struct attribute_group fs_attr_group = {
	.attrs = fs_attrs,
};
#endif

void msm_fb_first_backlight(void)
{
	msleep(36); // to blocking a flash light
	samsung_serigo_list(cabc_on_sequence,
		sizeof(cabc_on_sequence)/sizeof(*cabc_on_sequence));
	samsung_state.brightness = 0x5d;		
	samsung_state.poweron_brightness=0x5d;
	fisrt_bl_on = FALSE;
}
EXPORT_SYMBOL(msm_fb_first_backlight);

static void lcdc_samsung_panel_set_backlight(struct msm_fb_data_type *mfd)
{
	int bl_level;

	bl_level = mfd->bl_level;

	if ((samsung_state.brightness == 0) ||(bl_level==0))
		pr_info("%s:%d  bl_level : %d \n", __func__, __LINE__, bl_level);		

#ifdef LCDC_CABC
	if (get_kttech_hw_version() >= ES2)
	{
		// 예외 - LCDC on 전에 BL을 on한 경우 
		if (samsung_state.display_on == FALSE)
		{
			samsung_state.brightness = bl_level;
			if (bl_level > 0 && bl_level != DEFAULT_DIM)
			{
				samsung_state.poweron_brightness = bl_level;
			}
			return;
		}
	
		mutex_lock(&bl_set_lock);	
		if(bl_level == 0) 
		{   
			samsung_serigo_list(cabc_off_sequence,
				sizeof(cabc_off_sequence)/sizeof(*cabc_off_sequence));
		}
		else 
		{
			if (cabc_enable) 
			{		
				if (samsung_state.brightness == 0)
				{
					cabc_on_sequence[1].data[0] = bl_level; 		
					samsung_serigo_list(cabc_on_sequence,
						sizeof(cabc_on_sequence)/sizeof(*cabc_on_sequence));
				}
				else
				{
					bl_ctl_sequence[0].data[0] = bl_level;			
					samsung_serigo_list(bl_ctl_sequence,
						sizeof(bl_ctl_sequence)/sizeof(*bl_ctl_sequence));
				}
			}
			else 
			{
				if (samsung_state.brightness == 0)
				{
					manual_bl_on_sequence[3].data[22] = bl_level; 		
					samsung_serigo_list(manual_bl_on_sequence,
						sizeof(manual_bl_on_sequence)/sizeof(*manual_bl_on_sequence));
								
				}
				else
				{
					manual_bl_ctl_sequence[0].data[22] = bl_level;			
					samsung_serigo_list(manual_bl_ctl_sequence,
						sizeof(manual_bl_ctl_sequence)/sizeof(*manual_bl_ctl_sequence));
				}
			}
		}
		mutex_unlock(&bl_set_lock);			
		samsung_state.brightness = bl_level;	
		
		if (bl_level > 0 && bl_level != DEFAULT_DIM)
		{
			samsung_state.poweron_brightness = samsung_state.brightness;
		}
		
	}		
	else 
#endif		
	{		
		tps61161_set_bl_native(bl_level);
	}		
	
}

static void lcdc_samsung_panel_pre_bl_conrol(struct msm_fb_data_type *mfd, bool on)
{
}

#ifdef LCDC_CABC
// TODO : msm_fb driver에 의해 구동 되도록 수정
void lcdc_samsung_panel_set_cabc(uint32 enable)
{
	pr_info("%s:%d  old : %d new %d \n", __func__, __LINE__, cabc_enable, enable);

	mutex_lock(&bl_set_lock);		
	if (cabc_enable != enable)
	{
		if (enable)
		{
			manual_cabc_on_sequence[1].data[0] = samsung_state.brightness;				
			samsung_serigo_list(manual_cabc_on_sequence,
				sizeof(manual_cabc_on_sequence)/sizeof(*manual_cabc_on_sequence));				
		}
		else
		{
			manual_cabc_off_sequence[1].data[22] = samsung_state.brightness;				
			samsung_serigo_list(manual_cabc_off_sequence,
				sizeof(manual_cabc_off_sequence)/sizeof(*manual_cabc_off_sequence));	
		}		
	}
	cabc_enable = enable;	
	mutex_unlock(&bl_set_lock); 
}

EXPORT_SYMBOL(lcdc_samsung_panel_set_cabc);

uint32 lcdc_samsung_panel_get_cabc(void)
{
	return cabc_enable;
}

EXPORT_SYMBOL(lcdc_samsung_panel_get_cabc);
#endif

static struct msm_fb_panel_data samsung_panel_data = {
	.on = lcdc_samsung_panel_on,
	.off = lcdc_samsung_panel_off,
	.set_backlight = lcdc_samsung_panel_set_backlight,	
	.pre_bl_conrol = lcdc_samsung_panel_pre_bl_conrol,  // guard code -  pre_bl_conrol null check 없이 호출하는 경우 kernel panic 발생	
};

static int __devinit samsung_probe(struct platform_device *pdev)
{
	struct msm_panel_info *pinfo;
#ifdef SYSFS_DEBUG_CMD
	struct platform_device *fb_dev;
	struct msm_fb_data_type *mfd;
	int rc;
#endif

	pr_info("%s: id=%d\n", __func__, pdev->id);
	lcdc_samsung_pdata = pdev->dev.platform_data;

	pinfo = &samsung_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 24700000; /* 24.7MHz - DMB 성능 저하로 pclk 수정 */  //25600000; /* Max 27.77MHz */
#ifdef LCDC_CABC	
	if (get_kttech_hw_version() >= ES2)
	{
		pinfo->bl_max = 255;
	}
	else
#endif		
	{
		pinfo->bl_max = 31;
	}
	pinfo->bl_min = 1;

	/* AMS367PE02 Operation Manual, Page 7 */
	pinfo->lcdc.h_back_porch = 50;	/* HBP-HLW */
	pinfo->lcdc.h_front_porch = 13;
	pinfo->lcdc.h_pulse_width = 2;
	/* AMS367PE02 Operation Manual, Page 6 */
	pinfo->lcdc.v_back_porch = 7;		/* VBP-VLW */
	pinfo->lcdc.v_front_porch = 6;
	pinfo->lcdc.v_pulse_width = 1;

	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0;//0xff;
	pinfo->lcdc.hsync_skew = 0;
	pdev->dev.platform_data = &samsung_panel_data;

#ifndef SYSFS_DEBUG_CMD
	msm_fb_add_device(pdev);
#else
	fb_dev = msm_fb_add_device(pdev);
	mfd = platform_get_drvdata(fb_dev);
	rc = sysfs_create_group(&mfd->fbi->dev->kobj, &fs_attr_group);
	if (rc) {
		pr_err("%s: sysfs group creation failed, rc=%d\n", __func__,
			rc);
		return rc;
	}
#endif

#ifdef LCDC_CABC
	if (get_kttech_hw_version() >= ES2)
	{
	}
	else
#endif		
	{
		// KT TECH : Set Backlight
		tps61161_set_bl_native(17);
	}
	return 0;
}

#ifdef CONFIG_SPI_QUP
static int __devinit lcdc_samsung_spi_probe(struct spi_device *spi)
{
	pr_info("%s\n", __func__);
	lcdc_spi_client = spi;
	lcdc_spi_client->bits_per_word = 32;
	return 0;
}
static int __devexit lcdc_samsung_spi_remove(struct spi_device *spi)
{
	lcdc_spi_client = NULL;
	return 0;
}
static struct spi_driver lcdc_samsung_spi_driver = {
	.driver.name   = LCDC_SAMSUNG_SPI_DEVICE_NAME,
	.driver.owner  = THIS_MODULE,
	.probe         = lcdc_samsung_spi_probe,
	.remove        = __devexit_p(lcdc_samsung_spi_remove),
};
#endif

static struct platform_driver this_driver = {
	.probe		= samsung_probe,
	.driver.name	= "lcdc_samsung_wvga",
};

static int __init lcdc_samsung_panel_init(void)
{
	int ret;

#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
	if (msm_fb_detect_client("lcdc_samsung_wvga")) {
		pr_err("%s: detect failed\n", __func__);
		return 0;
	}
#endif

	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("%s: driver register failed, rc=%d\n", __func__, ret);
		return ret;
	}

#ifdef CONFIG_SPI_QUP
	ret = spi_register_driver(&lcdc_samsung_spi_driver);

	if (ret) {
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);
		platform_driver_unregister(&this_driver);
	} else
		pr_info("%s: SUCCESS (SPI)\n", __func__);
#else
	pr_info("%s: SUCCESS (BitBang)\n", __func__);
#endif
	return ret;
}

module_init(lcdc_samsung_panel_init);
static void __exit lcdc_samsung_panel_exit(void)
{
	pr_info("%s\n", __func__);
#ifdef CONFIG_SPI_QUP
	spi_unregister_driver(&lcdc_samsung_spi_driver);
#endif
	platform_driver_unregister(&this_driver);
}
module_exit(lcdc_samsung_panel_exit);
