/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/bitops.h>
#include <mach/camera.h>
#include <media/msm_camera.h>
#include "s5k4e5-o6.h"

/* 16bit address - 8 bit context register structure */
#define Q8	0x00000100
#define Q10	0x00000400

/* MCLK */
#define S5K4E5_MASTER_CLK_RATE 24000000

/* AF Total steps parameters */
#define S5K4E5_TOTAL_STEPS_NEAR_TO_FAR	32

#define S5K4E5_REG_PREV_FRAME_LEN_1	2	// 31
#define S5K4E5_REG_PREV_FRAME_LEN_2	3	// 32
#define S5K4E5_REG_PREV_LINE_LEN_1	4	// 33
#define S5K4E5_REG_PREV_LINE_LEN_2	5	// 34

#define S5K4E5_REG_SNAP_FRAME_LEN_1	2	// 15
#define S5K4E5_REG_SNAP_FRAME_LEN_2	3	// 16
#define S5K4E5_REG_SNAP_LINE_LEN_1	4	// 17
#define S5K4E5_REG_SNAP_LINE_LEN_2	5	// 18
#define MSB                             1
#define LSB                             0

#if 1	// def CONFIG_CALIBRATION_FEATURE 
#define S5K4E5_EEP_I2C_ADDR			0xA0 	// GT24C16 : original 0xA0  
#define S5K4E5_EEP_PAGE_SIZE			256		// GT24C16
#define S5K4E5_EEP_CAL_DATA_SIZE		S5K4E5_EEP_PAGE_SIZE * 5
static char eeprom_data[S5K4E5_EEP_CAL_DATA_SIZE];
#endif

struct s5k4e5_work_t {
	struct work_struct work;
};

#define SENSOR_DEBUG 0 // jykim

//#undef CDBG boba.kim 3015
//#define CDBG(fmt, args...) printk(KERN_INFO "driver.s5k4e5: " fmt, ##args) 

static struct s5k4e5_work_t *s5k4e5_sensorw;
static struct s5k4e5_work_t *s5k4e5_af_sensorw;
static struct s5k4e5_work_t *s5k4e5_eeprom_sensorw; 

static struct i2c_client *s5k4e5_af_client;
static struct i2c_client *s5k4e5_client;
static struct i2c_client *s5k4e5_eeprom_client;

struct s5k4e5_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;

	uint32_t sensormode;
	uint32_t fps_divider;/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider;/* init to 1 * 0x00000400 */
	uint16_t fps;

	uint16_t curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;

	enum s5k4e5_resolution_t prev_res;
	enum s5k4e5_resolution_t pict_res;
	enum s5k4e5_resolution_t curr_res;
	enum s5k4e5_test_mode_t  set_test;
};

static bool CSI_CONFIG;
static struct s5k4e5_ctrl_t *s5k4e5_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(s5k4e5_wait_queue);
static DECLARE_WAIT_QUEUE_HEAD(s5k4e5_af_wait_queue);
static DECLARE_WAIT_QUEUE_HEAD(s5k4e5_eeprom_wait_queue);

DEFINE_MUTEX(s5k4e5_mut);

static uint16_t prev_line_length_pck;
static uint16_t prev_frame_length_lines;
static uint16_t snap_line_length_pck;
static uint16_t snap_frame_length_lines;

static int s5k4e5_i2c_rxdata(unsigned short saddr,
		unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 1,
			.buf   = rxdata,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = rxdata,
		},
	};
	if (i2c_transfer(s5k4e5_client->adapter, msgs, 2) < 0) {
		CDBG("s5k4e5_i2c_rxdata faild 0x%x\n", saddr);
		return -EIO;
	}
	return 0;
}

#if 1	// def CONFIG_CALIBRATION_FEATURE
static int s5k4e5_i2c_rxburst(unsigned short saddr, unsigned char *rxdata,
	int length)
{
	unsigned char tmp_buf = 0;

	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = 0,
			.len   = 1,
			.buf   = &tmp_buf,
		},
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = length,
			.buf   = rxdata,
		},
	};
	
	if (i2c_transfer(s5k4e5_eeprom_client->adapter, msgs, 2) < 0) {
		CDBG("s5k4e5_i2c_rxdata faild 0x%x\n", saddr);
		return -EIO;
	}

#if 0
{
		int32_t i = 0;
		for (i = 0; i < S5K4E5_EEP_PAGE_SIZE; i++)
			CDBG("============= %s: index = %d,   val=0x%x \n", __func__, i, *(rxdata+i));
}
#endif

	return 0;
}
#endif

static int32_t s5k4e5_i2c_txdata(unsigned short saddr,
		unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};
	if (i2c_transfer(s5k4e5_client->adapter, msg, 1) < 0) {
		CDBG("s5k4e5_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}

static int32_t s5k4e5_i2c_read(unsigned short raddr,
		unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));
	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);
	rc = s5k4e5_i2c_rxdata(s5k4e5_client->addr, buf, rlen);
	if (rc < 0) {
		CDBG("s5k4e5_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}
	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);
#if SENSOR_DEBUG
	CDBG("s5k4e5_i2c_read 0x%x val = 0x%x!\n", raddr, *rdata);
#endif
	return rc;
}

static int32_t s5k4e5_i2c_write_b_sensor(unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;
#if SENSOR_DEBUG
	CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
#endif
	CDBG("s5k4e5 cmd (0x%x, 0x%x)\n", waddr, bdata);

	rc = s5k4e5_i2c_txdata(s5k4e5_client->addr, buf, 3);
	if (rc < 0) {
		CDBG("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
				waddr, bdata);
	}
	return rc;
}

static int32_t s5k4e5_i2c_write_b_table(struct s5k4e5_i2c_reg_conf const
		*reg_conf_tbl, int num)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num; i++) {
		rc = s5k4e5_i2c_write_b_sensor(reg_conf_tbl->waddr,
				reg_conf_tbl->bdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}
	return rc;
}

static int32_t s5k4e5_af_i2c_txdata(unsigned short saddr,
		unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};
	if (i2c_transfer(s5k4e5_af_client->adapter, msg, 1) < 0) {
		pr_err("s5k4e5_af_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}

static int32_t s5k4e5_af_i2c_write_b_sensor(uint8_t waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = waddr;
	buf[1] = bdata;
	//CDBG("i2c_write_b addr = 0x%x, val = 0x%x\n", waddr, bdata);
	rc = s5k4e5_af_i2c_txdata(s5k4e5_af_client->addr, buf, 2);
	if (rc < 0) {
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
				waddr, bdata);
	}
	return rc;
}

static void s5k4e5_start_stream(void)
{
	s5k4e5_i2c_write_b_sensor(0x0100, 0x01);/* streaming on */
}

static void s5k4e5_stop_stream(void)
{
	s5k4e5_i2c_write_b_sensor(0x0100, 0x00);/* streaming off */
}

static void s5k4e5_group_hold_on(void)
{
	s5k4e5_i2c_write_b_sensor(0x0104, 0x01);
}

static void s5k4e5_group_hold_off(void)
{
	s5k4e5_i2c_write_b_sensor(0x0104, 0x0);
}

static void s5k4e5_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider, d1, d2;

	d1 = (prev_frame_length_lines * 0x00000400) / snap_frame_length_lines;
	d2 = (prev_line_length_pck * 0x00000400) / snap_line_length_pck;
	divider = (d1 * d2) / 0x400;

	/*Verify PCLK settings and frame sizes.*/
	*pfps = (uint16_t) (fps * divider / 0x400);
}

static uint16_t s5k4e5_get_prev_lines_pf(void)
{
	if (s5k4e5_ctrl->prev_res == QTR_SIZE)
		return prev_frame_length_lines;
	else
		return snap_frame_length_lines;
}

static uint16_t s5k4e5_get_prev_pixels_pl(void)
{
	if (s5k4e5_ctrl->prev_res == QTR_SIZE)
		return prev_line_length_pck;
	else
		return snap_line_length_pck;
}

static uint16_t s5k4e5_get_pict_lines_pf(void)
{
	if (s5k4e5_ctrl->pict_res == QTR_SIZE)
		return prev_frame_length_lines;
	else
		return snap_frame_length_lines;
}

static uint16_t s5k4e5_get_pict_pixels_pl(void)
{
	if (s5k4e5_ctrl->pict_res == QTR_SIZE)
		return prev_line_length_pck;
	else
		return snap_line_length_pck;
}

static uint32_t s5k4e5_get_pict_max_exp_lc(void)
{
	return snap_frame_length_lines * 24;
}

static int32_t s5k4e5_set_fps(struct fps_cfg   *fps)
{
	uint16_t total_lines_per_frame;
	int32_t rc = 0;

	s5k4e5_ctrl->fps_divider = fps->fps_div;
	s5k4e5_ctrl->pict_fps_divider = fps->pict_fps_div;

	if (s5k4e5_ctrl->sensormode == SENSOR_PREVIEW_MODE) {
		total_lines_per_frame = (uint16_t)
		((prev_frame_length_lines * s5k4e5_ctrl->fps_divider) / 0x400);
	} else {
		total_lines_per_frame = (uint16_t)
		((snap_frame_length_lines * s5k4e5_ctrl->fps_divider) / 0x400);
	}

	s5k4e5_group_hold_on();
	rc = s5k4e5_i2c_write_b_sensor(0x0340,
			((total_lines_per_frame & 0xFF00) >> 8));
	rc = s5k4e5_i2c_write_b_sensor(0x0341,
			(total_lines_per_frame & 0x00FF));
	s5k4e5_group_hold_off();

	return rc;
}

static inline uint8_t s5k4e5_byte(uint16_t word, uint8_t offset)
{
	return word >> (offset * BITS_PER_BYTE);
}

static int32_t s5k4e5_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	int32_t rc = 0;
	static uint32_t fl_lines;

	if (gain > max_legal_gain) {
		pr_debug("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}
	/* Analogue Gain */
	s5k4e5_i2c_write_b_sensor(0x0204, s5k4e5_byte(gain, MSB));
	s5k4e5_i2c_write_b_sensor(0x0205, s5k4e5_byte(gain, LSB));

	if (line > (prev_frame_length_lines - 4)) {
		fl_lines = line+4;
		s5k4e5_group_hold_on();
		s5k4e5_i2c_write_b_sensor(0x0340, s5k4e5_byte(fl_lines, MSB));
		s5k4e5_i2c_write_b_sensor(0x0341, s5k4e5_byte(fl_lines, LSB));
		/* Coarse Integration Time */
		s5k4e5_i2c_write_b_sensor(0x0202, s5k4e5_byte(line, MSB));
		s5k4e5_i2c_write_b_sensor(0x0203, s5k4e5_byte(line, LSB));
		s5k4e5_group_hold_off();
	} else if (line < (fl_lines - 4)) {
		fl_lines = line+4;
		if (fl_lines < prev_frame_length_lines)
			fl_lines = prev_frame_length_lines;

		s5k4e5_group_hold_on();
		/* Coarse Integration Time */
		s5k4e5_i2c_write_b_sensor(0x0202, s5k4e5_byte(line, MSB));
		s5k4e5_i2c_write_b_sensor(0x0203, s5k4e5_byte(line, LSB));
		s5k4e5_i2c_write_b_sensor(0x0340, s5k4e5_byte(fl_lines, MSB));
		s5k4e5_i2c_write_b_sensor(0x0341, s5k4e5_byte(fl_lines, LSB));
		s5k4e5_group_hold_off();
	} else {
		fl_lines = line+4;
		s5k4e5_group_hold_on();
		/* Coarse Integration Time */
		s5k4e5_i2c_write_b_sensor(0x0202, s5k4e5_byte(line, MSB));
		s5k4e5_i2c_write_b_sensor(0x0203, s5k4e5_byte(line, LSB));
		s5k4e5_group_hold_off();
	}
	return rc;
}

static int32_t s5k4e5_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	uint16_t min_ll_pck = 0x0AB2;
	uint32_t ll_pck, fl_lines;
	uint32_t ll_ratio;
	int32_t rc = 0;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t ll_pck_msb, ll_pck_lsb;

	if (gain > max_legal_gain) {
		pr_debug("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	pr_debug("s5k4e5_write_exp_gain : gain = %d line = %d\n", gain, line);
	line = (uint32_t) (line * s5k4e5_ctrl->pict_fps_divider);
	fl_lines = snap_frame_length_lines;
	ll_pck = snap_line_length_pck;

	if (fl_lines < (line / 0x400))
		ll_ratio = (line / (fl_lines - 4));
	else
		ll_ratio = 0x400;

	ll_pck = ll_pck * ll_ratio / 0x400;
	line = line / ll_ratio;
	if (ll_pck < min_ll_pck)
		ll_pck = min_ll_pck;

	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);

	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line & 0x00FF);

	ll_pck_msb = (uint8_t) ((ll_pck & 0xFF00) >> 8);
	ll_pck_lsb = (uint8_t) (ll_pck & 0x00FF);

	s5k4e5_group_hold_on();
	s5k4e5_i2c_write_b_sensor(0x0204, gain_msb); /* Analogue Gain */
	s5k4e5_i2c_write_b_sensor(0x0205, gain_lsb);

	s5k4e5_i2c_write_b_sensor(0x0342, ll_pck_msb);
	s5k4e5_i2c_write_b_sensor(0x0343, ll_pck_lsb);

	/* Coarse Integration Time */
	s5k4e5_i2c_write_b_sensor(0x0202, intg_time_msb);
	s5k4e5_i2c_write_b_sensor(0x0203, intg_time_lsb);
	s5k4e5_group_hold_off();

	return rc;
}

static int32_t s5k4e5_move_focus(int direction,
		int32_t num_steps)
{
	int16_t step_direction, actual_step, next_position;
	uint8_t code_val_msb, code_val_lsb;

	if (direction == MOVE_NEAR)
		step_direction = 28;//16
	else
		step_direction = -28;//-16

	actual_step = (int16_t) (step_direction * num_steps);
	next_position = (int16_t) (s5k4e5_ctrl->curr_lens_pos + actual_step);

	if (next_position > 1023)
		next_position = 1023;
	else if (next_position < 0)
		next_position = 0;

	code_val_msb = next_position >> 4;
	code_val_lsb = ((next_position & 0x000F) << 4) | 0x0F ;	

	if (s5k4e5_af_i2c_write_b_sensor(code_val_msb, code_val_lsb) < 0) {
		pr_err("move_focus failed at line %d ...\n", __LINE__);
		return -EBUSY;
	}

	s5k4e5_ctrl->curr_lens_pos = next_position;
	return 0;
}

static int32_t s5k4e5_set_default_focus(uint8_t af_step)
{
	int32_t rc = 0;

	CDBG("s5k4e5_set_default_focus af_step=%d   \n", af_step);

	if(af_step != 0) {
	 	    	s5k4e5_af_i2c_write_b_sensor(0x00, 0x0F);
	  	}
	else {
 	uint16_t start_lens_pos = s5k4e5_ctrl->curr_lens_pos;

	s5k4e5_af_i2c_write_b_sensor(0x00, 0x0F);
 
	CDBG("s5k4e5_set_default_focus  start_lens_pos=%d \n", start_lens_pos);
	if(start_lens_pos > 800)
	    mdelay(110);
	else if(start_lens_pos > 700 && start_lens_pos <= 800)
	    mdelay(100); 	   
	else if(start_lens_pos > 600 && start_lens_pos <= 700)
	    mdelay(90); 	   
	else if(start_lens_pos > 400 && start_lens_pos <= 600)
	    mdelay(70); 	   
 	else if(start_lens_pos > 200 && start_lens_pos <= 400)
	    mdelay(50); 	   
 	else if(start_lens_pos > 100 && start_lens_pos <= 200)
	    mdelay(30); 	   
	else 
	    mdelay(100);
	}
	s5k4e5_ctrl->curr_lens_pos = 0;
	s5k4e5_ctrl->curr_step_pos = 0;
	return rc;
}

static int32_t s5k4e5_test(enum s5k4e5_test_mode_t mo)
{
	int32_t rc = 0;

	if (mo != TEST_OFF)
		rc = s5k4e5_i2c_write_b_sensor(0x0601, (uint8_t) mo);

	return rc;
}

static void s5k4e5_reset_sensor(void)
{
	s5k4e5_i2c_write_b_sensor(0x103, 0x1);
}

static int32_t s5k4e5_sensor_setting(int update_type, int rt)
{

	int32_t rc = 0;
	struct msm_camera_csi_params s5k4e5_csi_params;

	CDBG("sensor_settings  update_type=%d\n", update_type);

	s5k4e5_stop_stream();
	msleep(30);

	if (update_type == REG_INIT) {
		s5k4e5_reset_sensor();
		s5k4e5_i2c_write_b_table(s5k4e5_regs.rec_settings,
				s5k4e5_regs.rec_size);
		s5k4e5_i2c_write_b_table(s5k4e5_regs.reg_mipi,
				s5k4e5_regs.reg_mipi_size);
		s5k4e5_i2c_write_b_table(s5k4e5_regs.reg_pll_p,
				s5k4e5_regs.reg_pll_p_size);
		CSI_CONFIG = 0;

		if (rt == RES_PREVIEW) {
			s5k4e5_i2c_write_b_table(s5k4e5_regs.reg_prev,
					s5k4e5_regs.reg_prev_size);
			s5k4e5_i2c_write_b_sensor(S5K4E5_REG_PREV_FRAME_LEN_1,
					(prev_frame_length_lines & 0xFF00)<<8);
			s5k4e5_i2c_write_b_sensor(S5K4E5_REG_PREV_FRAME_LEN_2,
					(prev_frame_length_lines & 0xFF));
		} else {
			s5k4e5_i2c_write_b_table(s5k4e5_regs.reg_snap,
					s5k4e5_regs.reg_snap_size);
			s5k4e5_i2c_write_b_sensor(S5K4E5_REG_SNAP_FRAME_LEN_1,
					(snap_frame_length_lines & 0xFF00)<<8);
			s5k4e5_i2c_write_b_sensor(S5K4E5_REG_SNAP_FRAME_LEN_2,
					(snap_frame_length_lines & 0xFF));
		}
					
		msleep(20);
		
	} else if (update_type == UPDATE_PERIODIC) {
		
		if (!CSI_CONFIG) {
			msm_camio_vfe_clk_rate_set(192000000);
			s5k4e5_csi_params.data_format = CSI_10BIT;
			s5k4e5_csi_params.lane_cnt = 2;
			s5k4e5_csi_params.lane_assign = 0xe4;
			s5k4e5_csi_params.dpcm_scheme = 0;
			s5k4e5_csi_params.settle_cnt = 0x18;
			rc = msm_camio_csi_config(&s5k4e5_csi_params);
			msleep(20);
			CSI_CONFIG = 1;
		}
		s5k4e5_start_stream();
		msleep(30);
	}
	return rc;
}

static int32_t s5k4e5_video_config(int mode)
{

	int32_t rc = 0;
	int rt;
	CDBG("video config\n");
	/* change sensor resolution if needed */
	if (s5k4e5_ctrl->prev_res == QTR_SIZE)
		rt = RES_PREVIEW;
	else
		rt = RES_CAPTURE;
	if (s5k4e5_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;
	if (s5k4e5_ctrl->set_test) {
		if (s5k4e5_test(s5k4e5_ctrl->set_test) < 0)
			return  rc;
	}

	s5k4e5_ctrl->curr_res = s5k4e5_ctrl->prev_res;
	s5k4e5_ctrl->sensormode = mode;
	return rc;
}

static int32_t s5k4e5_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;

	/*change sensor resolution if needed */
	if (s5k4e5_ctrl->curr_res != s5k4e5_ctrl->pict_res) {
		if (s5k4e5_ctrl->pict_res == QTR_SIZE)
			rt = RES_PREVIEW;
		else
			rt = RES_CAPTURE;
		if (s5k4e5_sensor_setting(UPDATE_PERIODIC, rt) < 0)
			return rc;
	}

	s5k4e5_ctrl->curr_res = s5k4e5_ctrl->pict_res;
	s5k4e5_ctrl->sensormode = mode;
	return rc;
}

static int32_t s5k4e5_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	int rt;

	/* change sensor resolution if needed */
	if (s5k4e5_ctrl->curr_res != s5k4e5_ctrl->pict_res) {
		if (s5k4e5_ctrl->pict_res == QTR_SIZE)
			rt = RES_PREVIEW;
		else
			rt = RES_CAPTURE;
		if (s5k4e5_sensor_setting(UPDATE_PERIODIC, rt) < 0)
			return rc;
	}

	s5k4e5_ctrl->curr_res = s5k4e5_ctrl->pict_res;
	s5k4e5_ctrl->sensormode = mode;
	return rc;
}

static int32_t s5k4e5_set_sensor_mode(int mode,
		int res)
{
	int32_t rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = s5k4e5_video_config(mode);
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = s5k4e5_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = s5k4e5_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t s5k4e5_power_down(void)
{
	s5k4e5_stop_stream();

	if (s5k4e5_ctrl->sensordata->cam_shutdown != NULL)
	  s5k4e5_ctrl->sensordata->cam_shutdown(&s5k4e5_client->dev, 0); // Back Camera

	return 0;
}

static int s5k4e5_probe_init_done(const struct msm_camera_sensor_info *data)
{
	CDBG("%s \n", __func__);

#if 0
	gpio_set_value_cansleep(data->sensor_reset, 1);
	gpio_free(data->sensor_reset);
#endif

	return 0;
}

static int s5k4e5_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	uint16_t chipid1 = 0;
	uint16_t chipid2 = 0;

	CDBG("%s: %d  cam_setup\n", __func__, __LINE__);
	data->cam_setup(&s5k4e5_client->dev, 0); // Back Camera

	if(data->sensor_other_reset_supported)	 {
		gpio_direction_output(data->sensor_other_reset, 0);
		gpio_set_value_cansleep(data->sensor_other_reset, 1);	
	}
	
	gpio_direction_output(data->sensor_reset, 0);
	gpio_set_value_cansleep(data->sensor_reset, 1);
	msleep(10);
	gpio_set_value_cansleep(data->sensor_reset, 0);
	msleep(10);
	gpio_set_value_cansleep(data->sensor_reset, 1);
	msleep(10);

	if(s5k4e5_ctrl == NULL) {
	  s5k4e5_ctrl = kzalloc(sizeof(struct s5k4e5_ctrl_t), GFP_KERNEL);
	  if (!s5k4e5_ctrl) {
	  	CDBG("s5k4e5_init failed!\n");
	  	rc = -ENOMEM;
	  	goto init_probe_fail;
	  }
	}

	if (data)
		s5k4e5_ctrl->sensordata = data;

	s5k4e5_i2c_read(0x0000, &chipid1, 1);
	CDBG("ID 1: 0x%x\n", chipid1);
	if (chipid1 != 0x4E) {
		rc = -ENODEV;
		CDBG("s5k4e5_probe_init_sensor fail chip id doesnot match\n");
		goto init_probe_fail;
	}

	s5k4e5_i2c_read(0x0001, &chipid2 , 1);
	CDBG("ID 2: 0x%x\n", chipid2);
	if (chipid2 != 0x50) {
		rc = -ENODEV;
		CDBG("s5k4e5_probe_init_sensor fail chip id doesnot match\n");
		goto init_probe_fail;
	}


	goto init_probe_done;

init_probe_fail:
	CDBG(" s5k4e5_probe_init_sensor fails\n");
	gpio_set_value_cansleep(data->sensor_reset, 0);
	s5k4e5_probe_init_done(data);

	if (s5k4e5_ctrl->sensordata->cam_shutdown != NULL)
	  s5k4e5_ctrl->sensordata->cam_shutdown(&s5k4e5_client->dev, 0); // Back Camera

	if (data->vcm_enable) {
		int ret = gpio_request(data->vcm_pwd, "s5k4e5_af");
		if (!ret) {
			gpio_direction_output(data->vcm_pwd, 0);
			msleep(20);
			gpio_free(data->vcm_pwd);
		}
	}

init_probe_done:
	if(data->sensor_other_reset_supported)	 {
		gpio_set_value_cansleep(data->sensor_other_reset, 0);	
	}

	CDBG(" s5k4e5_probe_init_sensor finishes\n");
	return rc;
}

#if 1	// def CONFIG_CALIBRATION_FEATURE 
static void s5k4e5_get_eeprom_data(void)
{
	int32_t pageAddr = 0, rc = 0;
	char buf[S5K4E5_EEP_PAGE_SIZE];

  memset(buf, 0, sizeof(buf));
	memset(eeprom_data, 0, MAX_CAL_DATA_PACKET_LEN);

  
  CDBG("%s: \n", __func__);
  
	while(pageAddr < 5)
	{
		rc = s5k4e5_i2c_rxburst(s5k4e5_eeprom_client->addr | pageAddr, buf, S5K4E5_EEP_PAGE_SIZE);
		if (rc < 0) {
			CDBG("s5k4e5_i2c_read 0x%x failed!\n", s5k4e5_eeprom_client->addr);
			return;
		}

		memcpy(eeprom_data+pageAddr*S5K4E5_EEP_PAGE_SIZE, buf, S5K4E5_EEP_PAGE_SIZE);
		pageAddr++;
	}

#if 0
{
		int32_t i = 0;
		for (i = 0; i < pageAddr*S5K4E5_EEP_PAGE_SIZE; i++)
			CDBG("%s: index = %d,   val=0x%x ", __func__, i, *(eeprom_data+i));
}
#endif


}
#endif


int s5k4e5_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;

	CDBG("%s: %d\n", __func__, __LINE__);
	CDBG("Calling s5k4e5_sensor_open_init\n");

	s5k4e5_ctrl = kzalloc(sizeof(struct s5k4e5_ctrl_t), GFP_KERNEL);
	if (!s5k4e5_ctrl) {
		CDBG("s5k4e5_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	s5k4e5_ctrl->fps_divider = 1 * 0x00000400;
	s5k4e5_ctrl->pict_fps_divider = 1 * 0x00000400;
	s5k4e5_ctrl->set_test = TEST_OFF;
	s5k4e5_ctrl->prev_res = QTR_SIZE;
	s5k4e5_ctrl->pict_res = FULL_SIZE;

	if (data)
		s5k4e5_ctrl->sensordata = data;

	prev_frame_length_lines =
	((s5k4e5_regs.reg_prev[S5K4E5_REG_PREV_FRAME_LEN_1].bdata << 8) |
		s5k4e5_regs.reg_prev[S5K4E5_REG_PREV_FRAME_LEN_2].bdata);

	prev_line_length_pck =
	(s5k4e5_regs.reg_prev[S5K4E5_REG_PREV_LINE_LEN_1].bdata << 8) |
		s5k4e5_regs.reg_prev[S5K4E5_REG_PREV_LINE_LEN_2].bdata;

	snap_frame_length_lines =
	(s5k4e5_regs.reg_snap[S5K4E5_REG_SNAP_FRAME_LEN_1].bdata << 8) |
		s5k4e5_regs.reg_snap[S5K4E5_REG_SNAP_FRAME_LEN_2].bdata;

	snap_line_length_pck =
	(s5k4e5_regs.reg_snap[S5K4E5_REG_SNAP_LINE_LEN_1].bdata << 8) |
		s5k4e5_regs.reg_snap[S5K4E5_REG_SNAP_LINE_LEN_2].bdata;

	/* enable mclk first */
	msm_camio_clk_rate_set(S5K4E5_MASTER_CLK_RATE);
	rc = s5k4e5_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	CDBG("init settings\n");
	if (s5k4e5_ctrl->prev_res == QTR_SIZE)
		rc = s5k4e5_sensor_setting(REG_INIT, RES_PREVIEW);
	else
		rc = s5k4e5_sensor_setting(REG_INIT, RES_CAPTURE);
	s5k4e5_ctrl->fps = 30 * Q8;

	/* enable AF actuator */
	if (s5k4e5_ctrl->sensordata->vcm_enable) {
		CDBG("enable AF actuator, gpio = %d\n",
			 s5k4e5_ctrl->sensordata->vcm_pwd);
		rc = gpio_request(s5k4e5_ctrl->sensordata->vcm_pwd,
						"s5k4e5_af");
		if (!rc)
			gpio_direction_output(
				s5k4e5_ctrl->sensordata->vcm_pwd,
				 1);
		else {
			pr_err("s5k4e5_ctrl gpio request failed!\n");
			goto init_fail;
		}
		msleep(20);
		rc = s5k4e5_set_default_focus(0);
		if (rc < 0) {
			gpio_direction_output(s5k4e5_ctrl->sensordata->vcm_pwd,
								0);
			gpio_free(s5k4e5_ctrl->sensordata->vcm_pwd);
		}
	}

#if 1	// def CONFIG_CALIBRATION_FEATURE 
	s5k4e5_get_eeprom_data();
#endif	
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
init_fail:
	CDBG("init_fail\n");
	s5k4e5_probe_init_done(data);
init_done:
	CDBG("init_done\n");
	return rc;
}

static int s5k4e5_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k4e5_wait_queue);
	return 0;
}

static int s5k4e5_af_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k4e5_af_wait_queue);
	return 0;
}

static int s5k4e5_eeprom_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k4e5_eeprom_wait_queue);
	return 0;
}

static const struct i2c_device_id s5k4e5_eeprom_i2c_id[] = {
	{"s5k4e5_eeprom", 0},
	{ }
};

static const struct i2c_device_id s5k4e5_af_i2c_id[] = {
	{"s5k4e5_af", 0},
	{ }
};

static const struct i2c_device_id s5k4e5_i2c_id[] = {
	{"s5k4e5", 0},
	{ }
};


static int s5k4e5_eeprom_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("s5k4e5_eeprom_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	s5k4e5_eeprom_sensorw = kzalloc(sizeof(struct s5k4e5_work_t), GFP_KERNEL);
	if (!s5k4e5_eeprom_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k4e5_eeprom_sensorw);
	s5k4e5_eeprom_init_client(client);
	s5k4e5_eeprom_client = client;

	msleep(50);

	CDBG("s5k4e5_eeprom_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("s5k4e5_eeprom_probe failed! rc = %d\n", rc);
	return rc;
}

static int s5k4e5_af_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("s5k4e5_af_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	s5k4e5_af_sensorw = kzalloc(sizeof(struct s5k4e5_work_t), GFP_KERNEL);
	if (!s5k4e5_af_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k4e5_af_sensorw);
	s5k4e5_af_init_client(client);
	s5k4e5_af_client = client;

	msleep(50);

	CDBG("s5k4e5_af_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("s5k4e5_af_probe failed! rc = %d\n", rc);
	return rc;
}

static int s5k4e5_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("s5k4e5_probe called!\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	s5k4e5_sensorw = kzalloc(sizeof(struct s5k4e5_work_t), GFP_KERNEL);
	if (!s5k4e5_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k4e5_sensorw);
	s5k4e5_init_client(client);
	s5k4e5_client = client;

	msleep(50);

	CDBG("s5k4e5_probe successed! rc = %d\n", rc);
	return 0;

probe_failure:
	CDBG("s5k4e5_probe failed! rc = %d\n", rc);
	return rc;
}

static int __devexit s5k4e5_remove(struct i2c_client *client)
{
	struct s5k4e5_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	s5k4e5_client = NULL;
	kfree(sensorw);
	return 0;
}

static int __devexit s5k4e5_af_remove(struct i2c_client *client)
{
	struct s5k4e5_work_t *s5k4e5_af = i2c_get_clientdata(client);
	free_irq(client->irq, s5k4e5_af);
	s5k4e5_af_client = NULL;
	kfree(s5k4e5_af);
	return 0;
}

static int __devexit s5k4e5_eeprom_remove(struct i2c_client *client)
{
	struct s5k4e5_work_t *s5k4e5_eeprom = i2c_get_clientdata(client);
	free_irq(client->irq, s5k4e5_eeprom);
	s5k4e5_eeprom_client = NULL;
	kfree(s5k4e5_eeprom);
	return 0;
}

static struct i2c_driver s5k4e5_i2c_driver = {
	.id_table = s5k4e5_i2c_id,
	.probe  = s5k4e5_i2c_probe,
	.remove = __exit_p(s5k4e5_i2c_remove),
	.driver = {
		.name = "s5k4e5",
	},
};

static struct i2c_driver s5k4e5_af_i2c_driver = {
	.id_table = s5k4e5_af_i2c_id,
	.probe  = s5k4e5_af_i2c_probe,
	.remove = __exit_p(s5k4e5_af_i2c_remove),
	.driver = {
		.name = "s5k4e5_af",
	},
};

static struct i2c_driver s5k4e5_eeprom_i2c_driver = {
	.id_table = s5k4e5_eeprom_i2c_id,
	.probe  = s5k4e5_eeprom_i2c_probe,
	.remove = __exit_p(s5k4e5_eeprom_i2c_remove),
	.driver = {
	.name = "s5k4e5_eeprom",
	},
};

int s5k4e5_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	mutex_lock(&s5k4e5_mut);
	//CDBG("s5k4e5_sensor_config: cfgtype = %d\n",
	//		cdata.cfgtype);
	switch (cdata.cfgtype) {
		
#if 1	// def CONFIG_CALIBRATION_FEATURE
	case CFG_GET_CAL_DATA:	
  CDBG("%s:  CFG_GET_CAL_DATA     cal_data_idx=%d\n", __func__, cdata.cfg.eeprom_value.cal_data_idx);
		if (cdata.cfg.eeprom_value.cal_data_idx > 1)
			rc = -EFAULT;
		else {
			memcpy(cdata.cfg.eeprom_value.cal_v, eeprom_data + (cdata.cfg.eeprom_value.cal_data_idx	* MAX_CAL_DATA_PACKET_LEN), MAX_CAL_DATA_PACKET_LEN);
			if (copy_to_user((void *)argp, &cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			}
		break;
#endif

	case CFG_GET_PICT_FPS:
		s5k4e5_get_pict_fps(
			cdata.cfg.gfps.prevfps,
			&(cdata.cfg.gfps.pictfps));

		if (copy_to_user((void *)argp,
			&cdata,
			sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PREV_L_PF:
		cdata.cfg.prevl_pf =
			s5k4e5_get_prev_lines_pf();

		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PREV_P_PL:
		cdata.cfg.prevp_pl =
			s5k4e5_get_prev_pixels_pl();

		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_L_PF:
		cdata.cfg.pictl_pf =
			s5k4e5_get_pict_lines_pf();

		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_P_PL:
		cdata.cfg.pictp_pl =
			s5k4e5_get_pict_pixels_pl();
		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_GET_PICT_MAX_EXP_LC:
		cdata.cfg.pict_max_exp_lc =
			s5k4e5_get_pict_max_exp_lc();

		if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_SET_FPS:
	case CFG_SET_PICT_FPS:
		rc = s5k4e5_set_fps(&(cdata.cfg.fps));
		break;
	case CFG_SET_EXP_GAIN:
		rc = s5k4e5_write_exp_gain(cdata.cfg.exp_gain.gain,
				cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_PICT_EXP_GAIN:
		rc = s5k4e5_set_pict_exp_gain(cdata.cfg.exp_gain.gain,
				cdata.cfg.exp_gain.line);
		break;
	case CFG_SET_MODE:
		rc = s5k4e5_set_sensor_mode(cdata.mode, cdata.rs);
		break;
	case CFG_PWR_DOWN:
		rc = s5k4e5_power_down();
		break;
	case CFG_MOVE_FOCUS:
		rc = s5k4e5_move_focus(cdata.cfg.focus.dir,
				cdata.cfg.focus.steps);
		break;
	case CFG_SET_DEFAULT_FOCUS:
		rc = s5k4e5_set_default_focus(cdata.cfg.focus.steps);
		break;
	case CFG_GET_AF_MAX_STEPS:
		cdata.max_steps = S5K4E5_TOTAL_STEPS_NEAR_TO_FAR;
		if (copy_to_user((void *)argp,
					&cdata,
				sizeof(struct sensor_cfg_data)))
			rc = -EFAULT;
		break;
	case CFG_SET_EFFECT:
		//rc = s5k4e5_set_default_focus(cdata.cfg.effect);
		break;
	default:
		rc = -EFAULT;
		break;
	}
	mutex_unlock(&s5k4e5_mut);

	return rc;
}

static int s5k4e5_sensor_release(void)
{
	int rc = -EBADF;

	CDBG("s5k4e5_release completed    s\n");

	mutex_lock(&s5k4e5_mut);
	s5k4e5_set_default_focus(0);
	mutex_unlock(&s5k4e5_mut);

	mutex_lock(&s5k4e5_mut);
	s5k4e5_power_down();

#if 0
	msleep(20);
	gpio_set_value_cansleep(s5k4e5_ctrl->sensordata->sensor_reset, 0);
	usleep_range(5000, 5100);
	gpio_free(s5k4e5_ctrl->sensordata->sensor_reset);
#endif

	if (s5k4e5_ctrl->sensordata->vcm_enable) {
		gpio_set_value_cansleep(s5k4e5_ctrl->sensordata->vcm_pwd, 0);
		gpio_free(s5k4e5_ctrl->sensordata->vcm_pwd);
	}

	kfree(s5k4e5_ctrl);
	s5k4e5_ctrl = NULL;
	CDBG("s5k4e5_release completed  x\n");
	mutex_unlock(&s5k4e5_mut);

	return rc;
}

static int s5k4e5_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;

	rc = i2c_add_driver(&s5k4e5_i2c_driver);
	if (rc < 0 || s5k4e5_client == NULL) {
		rc = -ENOTSUPP;
		CDBG("%s: LINE=%d, I2C add driver failed", __func__, __LINE__);
		goto probe_fail_1;
	}

	rc = i2c_add_driver(&s5k4e5_af_i2c_driver);
	if (rc < 0 || s5k4e5_af_client == NULL) {
		rc = -ENOTSUPP;
		CDBG("%s: LINE=%d, I2C add driver failed", __func__, __LINE__);
		goto probe_fail_2;
	}

	rc = i2c_add_driver(&s5k4e5_eeprom_i2c_driver);
	if (rc < 0 || s5k4e5_eeprom_client == NULL) {
		rc = -ENOTSUPP;
		CDBG("%s: LINE=%d, I2C add driver failed", __func__, __LINE__);
		goto probe_fail_3; 
	}
	
	msm_camio_clk_rate_set(S5K4E5_MASTER_CLK_RATE);

	rc = s5k4e5_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail_4;

	s->s_init = s5k4e5_sensor_open_init;
	s->s_release = s5k4e5_sensor_release;
	s->s_config  = s5k4e5_sensor_config;
	s->s_mount_angle = info->sensor_platform_info->mount_angle;
	gpio_set_value_cansleep(info->sensor_reset, 0);

	//s5k4e5_probe_init_done(info);
	/* Keep vcm_pwd to OUT Low */
	if (info->vcm_enable) {
		rc = gpio_request(info->vcm_pwd, "s5k4e5_af");
		if (!rc) {
			gpio_direction_output(info->vcm_pwd, 0);
			msleep(20);
			gpio_free(info->vcm_pwd);
		} else
			return rc;
	}
	goto probe_done;

probe_fail_4:
	i2c_del_driver(&s5k4e5_eeprom_i2c_driver);
probe_fail_3:
	i2c_del_driver(&s5k4e5_af_i2c_driver);
probe_fail_2:
	i2c_del_driver(&s5k4e5_i2c_driver);
probe_fail_1:
	CDBG("s5k4e5_sensor_probe: SENSOR PROBE FAILS!\n");
	return rc;

probe_done:
#if 1 // PP에서 #if 0 으로 rollback
 if (s5k4e5_ctrl->sensordata->cam_shutdown != NULL)
	  s5k4e5_ctrl->sensordata->cam_shutdown(&s5k4e5_client->dev, 0); 
#endif
	return rc;
}

static int __devinit s5k4e5_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, s5k4e5_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = s5k4e5_probe,
	.driver = {
		.name = "msm_camera_s5k4e5",
		.owner = THIS_MODULE,
	},
};

static int __init s5k4e5_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(s5k4e5_init);
MODULE_DESCRIPTION("Samsung 5 MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
