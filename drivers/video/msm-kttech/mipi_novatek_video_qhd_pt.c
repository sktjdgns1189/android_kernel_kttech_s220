/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_novatek.h"

static struct msm_panel_info pinfo;

#undef DSI_BIT_CLK_500MHZ // Original Setting
#define DSI_BIT_CLK_480MHZ

#if defined(DSI_BIT_CLK_500MHZ)
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
/* DSI_BIT_CLK at 500MHz, 2 lane, RGB888 */
		{0x03, 0x01, 0x01, 0x00},	/* regulator */
		/* timing   */
		{0x82, 0x31, 0x13, 0x0, 0x42, 0x4D, 0x18,
		0x35, 0x21, 0x03, 0x04},
		{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */
		{0xee, 0x02, 0x86, 0x00},	/* strength */
		/* pll control */
		{0x40, 0xf9, 0xb0, 0xda, 0x00, 0x50, 0x48, 0x63,
#if defined(NOVATEK_TWO_LANE)
		0x30, 0x07, 0x03,
#else           /* default set to 1 lane */
		0x30, 0x07, 0x07,
#endif
		0x05, 0x14, 0x03, 0x0, 0x0, 0x54, 0x06, 0x10, 0x04, 0x0},
};
#else

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
/* DSI_BIT_CLK at 480MHz, 2 lane, RGB888 */
		{0x03, 0x01, 0x01, 0x00},	/* regulator */
		/* timing   */
		{0x79, 0x25, 0x1F, 0x00, 0x4C, 0x34, 0x25, 0x25,
		0x1C, 0x03, 0x04},
		{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */
		{0xee, 0x03, 0x86, 0x03},	/* strength */

#if defined(DSI_BIT_CLK_366MHZ)
		{0x41, 0xdb, 0xb2, 0xf5, 0x00, 0x50, 0x48, 0x63,
		0x31, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#elif defined(DSI_BIT_CLK_380MHZ)
		{0x41, 0xf7, 0xb2, 0xf5, 0x00, 0x50, 0x48, 0x63,
		0x31, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#elif defined(DSI_BIT_CLK_480MHZ)
		{0x41, 0xDF, 0x31, 0xda, 0x00, 0x50, 0x48, 0x63,
		0x31, 0x0f, 0x07,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#else		/* 200 mhz */
		{0x41, 0x8f, 0xb1, 0xda, 0x00, 0x50, 0x48, 0x63,
		0x33, 0x1f, 0x0f,
		0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 },
#endif
};
#endif

static int __init mipi_video_novatek_qhd_pt_init(void)
{
	int ret;

	if (msm_fb_detect_client("mipi_video_novatek_qhd"))
		return 0;

	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;
#if defined(DSI_BIT_CLK_500MHZ)
	pinfo.lcdc.h_back_porch = 80;
	pinfo.lcdc.h_front_porch = 24;
	pinfo.lcdc.h_pulse_width = 8;
	pinfo.lcdc.v_back_porch = 16;
	pinfo.lcdc.v_front_porch = 8;
	pinfo.lcdc.v_pulse_width = 1;
#else
	pinfo.lcdc.h_back_porch = 96;
	pinfo.lcdc.h_front_porch = 32;
	pinfo.lcdc.h_pulse_width = 10;
	pinfo.lcdc.v_back_porch = 12;
	pinfo.lcdc.v_front_porch = 4;
	pinfo.lcdc.v_pulse_width = 10;
#endif
	pinfo.lcdc.border_clr = 0;	/* blk */
//KT Tech. LCD
	pinfo.lcdc.underflow_clr = 0;	/* blk */	/* blue 0xff */
	pinfo.lcdc.hsync_skew = 0;
//KT Tech. LCD
	pinfo.bl_max = 28;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

#if defined(DSI_BIT_CLK_480MHZ)
	pinfo.clk_rate = 480000000;
#elif defined(DSI_BIT_CLK_380MHZ)
	pinfo.clk_rate = 380000000;
#elif defined(DSI_BIT_CLK_366MHZ)
	pinfo.clk_rate = 366000000;
#endif

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR;
	pinfo.mipi.data_lane0 = TRUE;
#if defined(NOVATEK_TWO_LANE)
	pinfo.mipi.data_lane1 = TRUE;
#endif
	pinfo.mipi.tx_eot_append = TRUE;
#if defined(DSI_BIT_CLK_500MHZ)
	pinfo.mipi.t_clk_post = 0x04;
	pinfo.mipi.t_clk_pre = 0x1c;
#else
	pinfo.mipi.t_clk_post = 10;
	pinfo.mipi.t_clk_pre = 30;
#endif
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;

	ret = mipi_novatek_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_QHD_PT);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_novatek_qhd_pt_init);
