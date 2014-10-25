/* Copyright (c) 2012, KT Tech Inc. All rights reserved.
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

/* Gamma -0.2, y=1.9 Initialize code */
static char Positive_Gamma_Curve_for_Red_00[2] = {0x24, 0x00};
static char Positive_Gamma_Curve_for_Red_01[2] = {0x25, 0x02};
static char Positive_Gamma_Curve_for_Red_02[2] = {0x26, 0x08};
static char Positive_Gamma_Curve_for_Red_03[2] = {0x27, 0x13};
static char Positive_Gamma_Curve_for_Red_04[2] = {0x28, 0x18};
static char Positive_Gamma_Curve_for_Red_05[2] = {0x29, 0x2A};
static char Positive_Gamma_Curve_for_Red_06[2] = {0x2A, 0x5C};
static char Positive_Gamma_Curve_for_Red_07[2] = {0x2B, 0x0A};
static char Positive_Gamma_Curve_for_Red_08[2] = {0x2D, 0x1E};
static char Positive_Gamma_Curve_for_Red_09[2] = {0x2F, 0x26};
static char Positive_Gamma_Curve_for_Red_10[2] = {0x30, 0x49};
static char Positive_Gamma_Curve_for_Red_11[2] = {0x31, 0x1B};
static char Positive_Gamma_Curve_for_Red_12[2] = {0x32, 0x43};
static char Positive_Gamma_Curve_for_Red_13[2] = {0x33, 0x57};
static char Positive_Gamma_Curve_for_Red_14[2] = {0x34, 0x36};
static char Positive_Gamma_Curve_for_Red_15[2] = {0x35, 0x57};
static char Positive_Gamma_Curve_for_Red_16[2] = {0x36, 0x65};
static char Positive_Gamma_Curve_for_Red_17[2] = {0x37, 0x08};

static char Negative_Gamma_Curve_for_Red_00[2] = {0x38, 0x01};
static char Negative_Gamma_Curve_for_Red_01[2] = {0x39, 0x04};
static char Negative_Gamma_Curve_for_Red_02[2] = {0x3A, 0x0E};
static char Negative_Gamma_Curve_for_Red_03[2] = {0x3B, 0x17};
static char Negative_Gamma_Curve_for_Red_04[2] = {0x3D, 0x17};
static char Negative_Gamma_Curve_for_Red_05[2] = {0x3F, 0x2A};
static char Negative_Gamma_Curve_for_Red_06[2] = {0x40, 0x5C};
static char Negative_Gamma_Curve_for_Red_07[2] = {0x41, 0x19};
static char Negative_Gamma_Curve_for_Red_08[2] = {0x42, 0x1F};
static char Negative_Gamma_Curve_for_Red_09[2] = {0x43, 0x26};
static char Negative_Gamma_Curve_for_Red_10[2] = {0x44, 0x61};
static char Negative_Gamma_Curve_for_Red_11[2] = {0x45, 0x1B};
static char Negative_Gamma_Curve_for_Red_12[2] = {0x46, 0x43};
static char Negative_Gamma_Curve_for_Red_13[2] = {0x47, 0x57};
static char Negative_Gamma_Curve_for_Red_14[2] = {0x48, 0x5A};
static char Negative_Gamma_Curve_for_Red_15[2] = {0x49, 0x7F};
static char Negative_Gamma_Curve_for_Red_16[2] = {0x4A, 0x8F};
static char Negative_Gamma_Curve_for_Red_17[2] = {0x4B, 0x35};

static char Positive_Gamma_Curve_for_Green_00[2] = {0x4C, 0x17};
static char Positive_Gamma_Curve_for_Green_01[2] = {0x4D, 0x19};
static char Positive_Gamma_Curve_for_Green_02[2] = {0x4E, 0x21};
static char Positive_Gamma_Curve_for_Green_03[2] = {0x4F, 0x28};
static char Positive_Gamma_Curve_for_Green_04[2] = {0x50, 0x18};
static char Positive_Gamma_Curve_for_Green_05[2] = {0x51, 0x2B};
static char Positive_Gamma_Curve_for_Green_06[2] = {0x52, 0x5C};
static char Positive_Gamma_Curve_for_Green_07[2] = {0x53, 0x14};
static char Positive_Gamma_Curve_for_Green_08[2] = {0x54, 0x1E};
static char Positive_Gamma_Curve_for_Green_09[2] = {0x55, 0x26};
static char Positive_Gamma_Curve_for_Green_10[2] = {0x56, 0x4E};
static char Positive_Gamma_Curve_for_Green_11[2] = {0x57, 0x19};
static char Positive_Gamma_Curve_for_Green_12[2] = {0x58, 0x3E};
static char Positive_Gamma_Curve_for_Green_13[2] = {0x59, 0x54};
static char Positive_Gamma_Curve_for_Green_14[2] = {0x5A, 0x41};
static char Positive_Gamma_Curve_for_Green_15[2] = {0x5B, 0x6B};
static char Positive_Gamma_Curve_for_Green_16[2] = {0x5C, 0xB0};
static char Positive_Gamma_Curve_for_Green_17[2] = {0x5D, 0x37};

static char Negative_Gamma_Curve_for_Green_00[2] = {0x5E, 0x1A};
static char Negative_Gamma_Curve_for_Green_01[2] = {0x5F, 0x1D};
static char Negative_Gamma_Curve_for_Green_02[2] = {0x60, 0x26};
static char Negative_Gamma_Curve_for_Green_03[2] = {0x61, 0x2E};
static char Negative_Gamma_Curve_for_Green_04[2] = {0x62, 0x17};
static char Negative_Gamma_Curve_for_Green_05[2] = {0x63, 0x2A};
static char Negative_Gamma_Curve_for_Green_06[2] = {0x64, 0x5B};
static char Negative_Gamma_Curve_for_Green_07[2] = {0x65, 0x25};
static char Negative_Gamma_Curve_for_Green_08[2] = {0x66, 0x1E};
static char Negative_Gamma_Curve_for_Green_09[2] = {0x67, 0x25};
static char Negative_Gamma_Curve_for_Green_10[2] = {0x68, 0x67};
static char Negative_Gamma_Curve_for_Green_11[2] = {0x69, 0x19};
static char Negative_Gamma_Curve_for_Green_12[2] = {0x6A, 0x3E};
static char Negative_Gamma_Curve_for_Green_13[2] = {0x6B, 0x54};
static char Negative_Gamma_Curve_for_Green_14[2] = {0x6C, 0x66};
static char Negative_Gamma_Curve_for_Green_15[2] = {0x6D, 0x95};
static char Negative_Gamma_Curve_for_Green_16[2] = {0x6E, 0xE2};
static char Negative_Gamma_Curve_for_Green_17[2] = {0x6F, 0x68};

static char Positive_Gamma_Curve_for_Blue_00[2] = {0x70, 0x4B};
static char Positive_Gamma_Curve_for_Blue_01[2] = {0x71, 0x4D};
static char Positive_Gamma_Curve_for_Blue_02[2] = {0x72, 0x53};
static char Positive_Gamma_Curve_for_Blue_03[2] = {0x73, 0x57};
static char Positive_Gamma_Curve_for_Blue_04[2] = {0x74, 0x15};
static char Positive_Gamma_Curve_for_Blue_05[2] = {0x75, 0x28};
static char Positive_Gamma_Curve_for_Blue_06[2] = {0x76, 0x59};
static char Positive_Gamma_Curve_for_Blue_07[2] = {0x77, 0x28};
static char Positive_Gamma_Curve_for_Blue_08[2] = {0x78, 0x1D};
static char Positive_Gamma_Curve_for_Blue_09[2] = {0x79, 0x25};
static char Positive_Gamma_Curve_for_Blue_10[2] = {0x7A, 0x57};
static char Positive_Gamma_Curve_for_Blue_11[2] = {0x7B, 0x1A};
static char Positive_Gamma_Curve_for_Blue_12[2] = {0x7C, 0x41};
static char Positive_Gamma_Curve_for_Blue_13[2] = {0x7D, 0x5A};
static char Positive_Gamma_Curve_for_Blue_14[2] = {0x7E, 0x3D};
static char Positive_Gamma_Curve_for_Blue_15[2] = {0x7F, 0x4D};
static char Positive_Gamma_Curve_for_Blue_16[2] = {0x80, 0x83};
static char Positive_Gamma_Curve_for_Blue_17[2] = {0x81, 0x08};

static char Negative_Gamma_Curve_for_Blue_00[2] = {0x82, 0x54};
static char Negative_Gamma_Curve_for_Blue_01[2] = {0x83, 0x56};
static char Negative_Gamma_Curve_for_Blue_02[2] = {0x84, 0x5D};
static char Negative_Gamma_Curve_for_Blue_03[2] = {0x85, 0x62};
static char Negative_Gamma_Curve_for_Blue_04[2] = {0x86, 0x15};
static char Negative_Gamma_Curve_for_Blue_05[2] = {0x87, 0x27};
static char Negative_Gamma_Curve_for_Blue_06[2] = {0x88, 0x59};
static char Negative_Gamma_Curve_for_Blue_07[2] = {0x89, 0x3B};
static char Negative_Gamma_Curve_for_Blue_08[2] = {0x8A, 0x1D};
static char Negative_Gamma_Curve_for_Blue_09[2] = {0x8B, 0x25};
static char Negative_Gamma_Curve_for_Blue_10[2] = {0x8C, 0x70};
static char Negative_Gamma_Curve_for_Blue_11[2] = {0x8D, 0x1B};
static char Negative_Gamma_Curve_for_Blue_12[2] = {0x8E, 0x42};
static char Negative_Gamma_Curve_for_Blue_13[2] = {0x8F, 0x5A};
static char Negative_Gamma_Curve_for_Blue_14[2] = {0x90, 0x61};
static char Negative_Gamma_Curve_for_Blue_15[2] = {0x91, 0x74};
static char Negative_Gamma_Curve_for_Blue_16[2] = {0x92, 0xAF};
static char Negative_Gamma_Curve_for_Blue_17[2] = {0x93, 0x35};

static char Unlock_CMD2_0[2] = {0xF3, 0xAA};
static char CMD2_P0_MTP[2] = {0xC9, 0x01};
static char Unlock_CMD2_P0[2] = {0xFF, 0xAA};

static struct dsi_cmd_desc novatek_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 20,
		sizeof(sw_reset), sw_reset},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Unlock_CMD2_0), Unlock_CMD2_0},

	/* Positive gamma red */	
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_00), Positive_Gamma_Curve_for_Red_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_01), Positive_Gamma_Curve_for_Red_01},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_02), Positive_Gamma_Curve_for_Red_02},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_03), Positive_Gamma_Curve_for_Red_03},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_04), Positive_Gamma_Curve_for_Red_04},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_05), Positive_Gamma_Curve_for_Red_05},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_06), Positive_Gamma_Curve_for_Red_06},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_07), Positive_Gamma_Curve_for_Red_07},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_08), Positive_Gamma_Curve_for_Red_08},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_09), Positive_Gamma_Curve_for_Red_09},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_10), Positive_Gamma_Curve_for_Red_10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_11), Positive_Gamma_Curve_for_Red_11},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_12), Positive_Gamma_Curve_for_Red_12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_13), Positive_Gamma_Curve_for_Red_13},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_14), Positive_Gamma_Curve_for_Red_14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_15), Positive_Gamma_Curve_for_Red_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_16), Positive_Gamma_Curve_for_Red_16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Red_17), Positive_Gamma_Curve_for_Red_17},	

	/* Negative gamma red */	
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_00), Negative_Gamma_Curve_for_Red_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_01), Negative_Gamma_Curve_for_Red_01},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_02), Negative_Gamma_Curve_for_Red_02},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_03), Negative_Gamma_Curve_for_Red_03},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_04), Negative_Gamma_Curve_for_Red_04},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_05), Negative_Gamma_Curve_for_Red_05},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_06), Negative_Gamma_Curve_for_Red_06},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_07), Negative_Gamma_Curve_for_Red_07},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_08), Negative_Gamma_Curve_for_Red_08},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_09), Negative_Gamma_Curve_for_Red_09},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_10), Negative_Gamma_Curve_for_Red_10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_11), Negative_Gamma_Curve_for_Red_11},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_12), Negative_Gamma_Curve_for_Red_12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_13), Negative_Gamma_Curve_for_Red_13},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_14), Negative_Gamma_Curve_for_Red_14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_15), Negative_Gamma_Curve_for_Red_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_16), Negative_Gamma_Curve_for_Red_16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Red_17), Negative_Gamma_Curve_for_Red_17},

	/* Positive gamma Green */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_00), Positive_Gamma_Curve_for_Green_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_01), Positive_Gamma_Curve_for_Green_01},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_02), Positive_Gamma_Curve_for_Green_02},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_03), Positive_Gamma_Curve_for_Green_03},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_04), Positive_Gamma_Curve_for_Green_04},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_05), Positive_Gamma_Curve_for_Green_05},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_06), Positive_Gamma_Curve_for_Green_06},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_07), Positive_Gamma_Curve_for_Green_07},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_08), Positive_Gamma_Curve_for_Green_08},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_09), Positive_Gamma_Curve_for_Green_09},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_10), Positive_Gamma_Curve_for_Green_10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_11), Positive_Gamma_Curve_for_Green_11},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_12), Positive_Gamma_Curve_for_Green_12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_13), Positive_Gamma_Curve_for_Green_13},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_14), Positive_Gamma_Curve_for_Green_14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_15), Positive_Gamma_Curve_for_Green_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_16), Positive_Gamma_Curve_for_Green_16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Green_17), Positive_Gamma_Curve_for_Green_17},

	/* Negative gamma Green */	
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_00), Negative_Gamma_Curve_for_Green_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_01), Negative_Gamma_Curve_for_Green_01},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_02), Negative_Gamma_Curve_for_Green_02},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_03), Negative_Gamma_Curve_for_Green_03},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_04), Negative_Gamma_Curve_for_Green_04},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_05), Negative_Gamma_Curve_for_Green_05},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_06), Negative_Gamma_Curve_for_Green_06},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_07), Negative_Gamma_Curve_for_Green_07},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_08), Negative_Gamma_Curve_for_Green_08},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_09), Negative_Gamma_Curve_for_Green_09},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_10), Negative_Gamma_Curve_for_Green_10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_11), Negative_Gamma_Curve_for_Green_11},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_12), Negative_Gamma_Curve_for_Green_12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_13), Negative_Gamma_Curve_for_Green_13},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_14), Negative_Gamma_Curve_for_Green_14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_15), Negative_Gamma_Curve_for_Green_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_16), Negative_Gamma_Curve_for_Green_16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Green_17), Negative_Gamma_Curve_for_Green_17},

	/* Positive gamma Blue */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_00), Positive_Gamma_Curve_for_Blue_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_01), Positive_Gamma_Curve_for_Blue_01},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_02), Positive_Gamma_Curve_for_Blue_02},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_03), Positive_Gamma_Curve_for_Blue_03},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_04), Positive_Gamma_Curve_for_Blue_04},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_05), Positive_Gamma_Curve_for_Blue_05},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_06), Positive_Gamma_Curve_for_Blue_06},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_07), Positive_Gamma_Curve_for_Blue_07},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_08), Positive_Gamma_Curve_for_Blue_08},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_09), Positive_Gamma_Curve_for_Blue_09},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_10), Positive_Gamma_Curve_for_Blue_10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_11), Positive_Gamma_Curve_for_Blue_11},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_12), Positive_Gamma_Curve_for_Blue_12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_13), Positive_Gamma_Curve_for_Blue_13},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_14), Positive_Gamma_Curve_for_Blue_14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_15), Positive_Gamma_Curve_for_Blue_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_16), Positive_Gamma_Curve_for_Blue_16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Positive_Gamma_Curve_for_Blue_17), Positive_Gamma_Curve_for_Blue_17},

	/* Negative gamma Blue */	
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_00), Negative_Gamma_Curve_for_Blue_00},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_01), Negative_Gamma_Curve_for_Blue_01},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_02), Negative_Gamma_Curve_for_Blue_02},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_03), Negative_Gamma_Curve_for_Blue_03},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_04), Negative_Gamma_Curve_for_Blue_04},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_05), Negative_Gamma_Curve_for_Blue_05},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_06), Negative_Gamma_Curve_for_Blue_06},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_07), Negative_Gamma_Curve_for_Blue_07},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_08), Negative_Gamma_Curve_for_Blue_08},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_09), Negative_Gamma_Curve_for_Blue_09},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_10), Negative_Gamma_Curve_for_Blue_10},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_11), Negative_Gamma_Curve_for_Blue_11},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_12), Negative_Gamma_Curve_for_Blue_12},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_13), Negative_Gamma_Curve_for_Blue_13},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_14), Negative_Gamma_Curve_for_Blue_14},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_15), Negative_Gamma_Curve_for_Blue_15},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_16), Negative_Gamma_Curve_for_Blue_16},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, 
		sizeof(Negative_Gamma_Curve_for_Blue_17), Negative_Gamma_Curve_for_Blue_17},

	/* Set of Gamma Tables (-) */
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(CMD2_P0_MTP), CMD2_P0_MTP},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 50,
		sizeof(Unlock_CMD2_P0), Unlock_CMD2_P0},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 100,
		sizeof(display_on), display_on},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
#ifndef CONFIG_MACH_KTTECH	
		sizeof(novatek_f4), novatek_f4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(novatek_8c), novatek_8c},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(novatek_ff), novatek_ff},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
#endif	
		sizeof(set_num_of_lanes), set_num_of_lanes},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_width), set_width},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(set_height), set_height},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(rgb_888), rgb_888},
#ifdef CONFIG_KTTECH_NOVATEK_BACKLIGHT
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(led_pwm3), led_pwm3},
#endif
};

