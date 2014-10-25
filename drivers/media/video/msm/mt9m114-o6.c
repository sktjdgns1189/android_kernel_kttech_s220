#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>

#include "mt9m114-o6.h"

/* Micron MT9M114 Registers and their values */
/*
 *  Camera i2c data, clock
 */ 
#define  MT9M114_I2C_NAME         "mt9m114"

/* 
 * Camera image sensor slave address
 */
#define  MT9M114_I2C_WR_ID     0x90  // Sensor 스펙 보고 맞출 것

#define  MT9M114_DEFAULT_CLOCK_RATE  24000000
 
/* Sensor Core Registers */
#define  REG_MT9M114_MODEL_ID 0x0000
#define  MT9M114_MODEL_ID     0x2481

#define SENSOR_DEBUG 0

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "driver.mt9m114: " fmt, ##args) 

struct mt9m114_work {
	struct work_struct work;
};


static struct  mt9m114_work *mt9m114_sensorw;
static struct  i2c_client *mt9m114_client;

struct mt9m114_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};


static struct mt9m114_ctrl *mt9m114_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(mt9m114_wait_queue);
DEFINE_MUTEX(mt9m114_mut);

static int16_t mt9m114_effect = CAMERA_EFFECT_OFF;
static bool CSI_CONFIG;
static int32_t mt9m114_seq_probe(void);
static int32_t mt9m114_seq_probe_(void); // boba.kim 0420


/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct mt9m114_reg mt9m114_regs;


/*=============================================================*/
static int32_t mt9m114_i2c_txdata(unsigned short saddr,
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

#if 0
//#if SENSOR_DEBUG
	if (length == 2)
		CDBG("msm_io_i2c_w: 0x%04x 0x%04x\n",
			*(u16 *) txdata, *(u16 *) (txdata + 2));
	else if (length == 4)
		CDBG("msm_io_i2c_w: 0x%04x\n", *(u16 *) txdata);
	else
		CDBG("msm_io_i2c_w: length = %d\n", length);
#endif
	if (i2c_transfer(mt9m114_client->adapter, msg, 1) < 0) {
		CDBG("mt9m114_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9m114_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum mt9m114_width width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF00)>>8;
		buf[3] = (wdata & 0x00FF);

#if SENSOR_DEBUG
		CDBG("mt9m114_i2c_write: waddr= 0x%04x wdata=0x%04x\n", waddr, wdata);
#endif
		rc = mt9m114_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = waddr;
		buf[1] = wdata;
		rc = mt9m114_i2c_txdata(saddr, buf, 2);
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t mt9m114_i2c_write_table(
	struct mt9m114_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9m114_i2c_write(mt9m114_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata,
			reg_conf_tbl->width);
		if (rc < 0)
			break;

		if (reg_conf_tbl->mdelay_time != 0xff) {
			mt9m114_seq_probe();
		}
		else if (reg_conf_tbl->mdelay_time != 0)
			mdelay(reg_conf_tbl->mdelay_time);
		reg_conf_tbl++;
	}

	return rc;
}

static int mt9m114_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

#if SENSOR_DEBUG
	if (length == 2)
		CDBG("msm_io_i2c_r: 0x%04x 0x%04x\n",
			*(u16 *) rxdata, *(u16 *) (rxdata + 2));
	else if (length == 4)
		CDBG("msm_io_i2c_r: 0x%04x\n", *(u16 *) rxdata);
	else
		CDBG("msm_io_i2c_r: length = %d\n", length);
#endif

	if (i2c_transfer(mt9m114_client->adapter, msgs, 2) < 0) {
		CDBG("mt9m114_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9m114_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum mt9m114_width width)
	
{
	int32_t rc = 0;
	unsigned char buf[4];

  //CDBG("%s: line=%d\n", __func__, __LINE__);

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	switch (width) {
	case WORD_LEN: {
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);

		rc = mt9m114_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0] << 8 | buf[1];
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG("mt9m114_i2c_read addr = 0x%x failed!\n", saddr);

	return rc;
}

static int32_t mt9m114_seq_probe(void)
{
	int32_t rc = -EINVAL;
/*
	int32_t cnt = 0;
	unsigned  short readData = 1;
	
	while ( ((readData & 0x0001) == 1) && cnt < 10) {
		mdelay(10);
		rc = mt9m114_i2c_read(mt9m114_client->addr, 0x0080, &readData, WORD_LEN);
		CDBG("%s: cnt=%d,  readData=%d\n", __func__, cnt, readData);
		cnt++;
	}
*/

	return rc;
}

static int32_t mt9m114_seq_probe_(void) // boba.kim 0420
{
	int32_t rc = -EINVAL;
	int32_t i = 0;
	unsigned  short readData = 1;

//	msleep(50);
	do {  
		mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xDC01, WORD_LEN); 
		mt9m114_i2c_read(mt9m114_client->addr, 0x0990, &readData, WORD_LEN);
		i++;
		if (readData==0x5200){
 			printk("%s: OK readData(0x%x), i(%d)\n", __func__, readData, i);
			rc = 0;
		}	
		else
			msleep(50);
		} while (rc != 0 && i < 10);
		printk("%s: readData(0x%x), i(%d)\n", __func__, readData, i);

	return rc;
}

static int32_t mt9m114_stop_stream(void)
{
	int32_t rc = 0;

	CDBG("%s\n", __func__);
	return rc;
}

static int32_t mt9m114_start_stream(void)
{
	int32_t rc = 0;
	CDBG("%s\n", __func__);
	return rc;
}


static int32_t mt9m114_sensor_setting(int update_type, int rt)
{
	int32_t rc = 0;
	unsigned short readData = 1;	
	struct msm_camera_csi_params mt9m114_csi_params;

	switch (update_type) {
	case REG_INIT:
		CDBG("%s: REG_INIT \n", __func__);
		CSI_CONFIG = 0;
		break;

	case UPDATE_PERIODIC:
		CDBG("%s: UPDATE_PERIODIC \n", __func__);

		if (rt == RES_PREVIEW) {
			if(!CSI_CONFIG) {
					rc = mt9m114_stop_stream();
					msleep(50);

					mt9m114_csi_params.data_format = CSI_8BIT;
					mt9m114_csi_params.lane_cnt = 1;
					mt9m114_csi_params.lane_assign = 0xe4;
					mt9m114_csi_params.dpcm_scheme = 0;
					mt9m114_csi_params.settle_cnt = 20;

					rc = msm_camio_csi_config(&mt9m114_csi_params);		

					mt9m114_seq_probe_(); //boba.kim 0420
					CDBG("%s: write reg_table \n", __func__);
					rc = mt9m114_i2c_write_table(&mt9m114_regs.regtbl[0],
										mt9m114_regs.regtbl_size);

					rc = mt9m114_start_stream();
					
					mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xDC01, WORD_LEN); 
					mt9m114_i2c_read(mt9m114_client->addr, 0x0990, &readData, WORD_LEN);
					printk("%s: readData(0x%x) \n", __func__, readData);
					
					CSI_CONFIG = 1;
			}			
		}
		else if(rt == RES_CAPTURE) {
			return rc;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t mt9m114_power_down(void)
{
	CDBG("%s: e\n", __func__);
	if (mt9m114_ctrl->sensordata->cam_shutdown != NULL)
	  mt9m114_ctrl->sensordata->cam_shutdown(&mt9m114_client->dev, 1); // Front camera

	//CDBG("%s: x\n", __func__);
	return 0;
}

static int mt9m114_probe_init_done(const struct msm_camera_sensor_info *data)
{
	CDBG("%s: e\n", __func__);

	gpio_set_value_cansleep(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);

	//CDBG("%s: x\n", __func__);
	return 0;
}


static int mt9m114_set_brightness(int brightness)
{
	long rc = 0;

	//CDBG("%s: brightness=%d == e\n", __func__, brightness);

	switch (brightness) {

		case 0: { // Brightness -5
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x1600, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x1400, WORD_LEN);
		}
		break;

		case 1: { // Brightness -3
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x2600, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x2000, WORD_LEN); 			
		}
		break;

		case 2: { // Brightness -2
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x3800, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x2E00, WORD_LEN); 
		}
		break;

		case 3: { // Brightness 0
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x4A00, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x3E00, WORD_LEN); 
		}
		break;

		case 4: { // Brightness +2
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x5E00, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x5200, WORD_LEN); 
		}
		break;

		case 5: { // Brightness +3
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x7200, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x6600, WORD_LEN); 
		}
		break;

		case 6: { // Brightness +5
//  {0x098E, 0xC87A, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AET_TARGET_AVERAGE_LUMA]
//  {0x0990, 0x8800, WORD_LEN, 0  }, 	// CAM_AET_TARGET_AVERAGE_LUMA
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC87A, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x8000, WORD_LEN); 
		}
		break;

		default: {
		}
	}


	//CDBG("%s: x\n", __func__);
	return rc;

}

static int mt9m114_set_wb(int wb)
{
	long rc = 0;

//	CDBG("%s: wb=%d === e\n", __func__, wb);

	switch (wb) {

		case 1: { // CAMERA_WB_AUTO
//  {0x098E, 0xC909, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_AWB_AWBMODE]
//  {0x0990, 0x0300, WORD_LEN, 0  }, 	// CAM_AWB_AWBMODE
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC909, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0300, WORD_LEN); 
		}
		break;

		case 5: { // CAMERA_WB_DAYLIGHT
//  {0x098E, 0x0000, WORD_LEN, 0  },
//  {0x0990, 0x0100, WORD_LEN, 0  }, 	// CAM_AWB_AWBMODE
//  {0xC8F0, 0x1964, WORD_LEN, 0  }, 	// CAM_AWB_COLOR_TEMPERATURE
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC909, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0100, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xC8F0, 0x1964, WORD_LEN); 
		}
		break;

		case 6: { // CAMERA_WB_CLOUDY_DAYLIGHT
//  {0x098E, 0x0000, WORD_LEN, 0  },
//  {0x0990, 0x0100, WORD_LEN, 0  }, 	// CAM_AWB_AWBMODE
//  {0xC8F0, 0x13EC, WORD_LEN, 0  }, 	// CAM_AWB_COLOR_TEMPERATURE		
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC909, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0100, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xC8F0, 0x13EC, WORD_LEN); 	
		}
		break;

		case 4: { // CAMERA_WB_FLUORESCENT
//  {0x098E, 0x0000, WORD_LEN, 0  },
//  {0x0990, 0x0100, WORD_LEN, 0  }, 	// CAM_AWB_AWBMODE
//  {0xC8F0, 0x1130, WORD_LEN, 0  }, 	// CAM_AWB_COLOR_TEMPERATURE		}
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC909, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0100, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xC8F0, 0x1130, WORD_LEN); 
		}
		break;

		case 3: { // CAMERA_WB_INCANDESCENT
//  {0x098E, 0x0000, WORD_LEN, 0  },
//  {0x0990, 0x0100, WORD_LEN, 0  }, 	// CAM_AWB_AWBMODE
//  {0xC8F0, 0x0AF0, WORD_LEN, 0  }, 	// CAM_AWB_COLOR_TEMPERATURE
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC909, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0100, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xC8F0, 0x0AF0, WORD_LEN); 						
		}
		break;

	}

	//CDBG("%s: x\n", __func__);
	return rc;

}

static long mt9m114_set_effect(int mode, int effect)
{
	long rc = 0;
//	unsigned short readData = 0;

	//CDBG("%s: mode=%d, effect=%d  == e\n", __func__, mode, effect);

	switch (effect) {
		case CAMERA_EFFECT_OFF: { // 0
//  {0x098E, 0xC874, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
//  {0x0990, 0x0000, WORD_LEN, 0  }, 	// CAM_SFX_CONTROL
//  {0xDC00, 0x2800, WORD_LEN, 0  }, 	// SYSMGR_NEXT_STATE
//  {0x0080, 0x8004, WORD_LEN, 50}, 	// COMMAND_REGISTER
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC874, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0000, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xDC00, 0x2800, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0080, 0x8004, WORD_LEN); 
				msleep(50);
		}
		break;

		case CAMERA_EFFECT_MONO: { // 1
//  {0x098E, 0xC874, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
//  {0x0990, 0x0100, WORD_LEN, 0  }, 	// CAM_SFX_CONTROL
//  {0xDC00, 0x2800, WORD_LEN, 0  }, 	// SYSMGR_NEXT_STATE
//  {0x0080, 0x8004, WORD_LEN, 50}, 	// COMMAND_REGISTER
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC874, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0100, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xDC00, 0x2800, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0080, 0x8004, WORD_LEN); 
				msleep(50);
		}
		break;

		case CAMERA_EFFECT_NEGATIVE: { // 2
//  {0x098E, 0xC874, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
//  {0x0990, 0x0300, WORD_LEN, 0  }, 	// CAM_SFX_CONTROL
//  {0xDC00, 0x2800, WORD_LEN, 0  }, 	// SYSMGR_NEXT_STATE
//  {0x0080, 0x8004, WORD_LEN, 50}, 	// COMMAND_REGISTER
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC874, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0300, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xDC00, 0x2800, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0080, 0x8004, WORD_LEN); 
				msleep(50);
		}
		break;

		case CAMERA_EFFECT_SOLARIZE: { // 3
//  {0x098E, 0xC874, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
//  {0x0990, 0x0500, WORD_LEN, 0  }, 	// CAM_SFX_CONTROL
//  {0xDC00, 0x2800, WORD_LEN, 0  }, 	// SYSMGR_NEXT_STATE
//  {0x0080, 0x8004, WORD_LEN, 50}, 	// COMMAND_REGISTER
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC874, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0500, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xDC00, 0x2800, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0080, 0x8004, WORD_LEN); 
				msleep(50);
		}
		break;

		case CAMERA_EFFECT_SEPIA: { // 7
//  {0x098E, 0xC874, WORD_LEN, 0  }, 	// LOGICAL_ADDRESS_ACCESS [CAM_SFX_CONTROL]
//  {0x0990, 0x0200, WORD_LEN, 0  }, 	// CAM_SFX_CONTROL
//  {0xDC00, 0x2800, WORD_LEN, 0  }, 	// SYSMGR_NEXT_STATE
//  {0x0080, 0x8004, WORD_LEN, 50}, 	// COMMAND_REGISTER
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x098E, 0xC874, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0990, 0x0200, WORD_LEN); 			
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0xDC00, 0x2800, WORD_LEN); 
				rc = mt9m114_i2c_write(mt9m114_client->addr, 0x0080, 0x8004, WORD_LEN); 
				msleep(50);
		}
		break;

	}
	mt9m114_effect = effect;

	//CDBG("%s: x\n", __func__);
	return rc;
}

static int32_t mt9m114_video_config(int mode)
{
	int32_t rc = 0;
	int rt;

	/* change sensor resolution if needed */
	rt = RES_PREVIEW;

	if (mt9m114_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;

	return rc;
}

static int32_t mt9m114_snapshot_config(int mode)
{
	int32_t rc = 0;	
	int rt;
	rt = RES_CAPTURE;

	if (mt9m114_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;

	return rc;
}

static long mt9m114_set_sensor_mode(int mode)
{
	int rc = 0;

	CDBG("%s: e ================   mode=%d\n", __func__, mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		CDBG("%s: SENSOR_PREVIEW_MODE \n", __func__);
		rc = mt9m114_video_config(mode);
		break;

	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		CDBG("%s: SENSOR_SNAPSHOT_MODE or SENSOR_RAW_SNAPSHOT_MODE\n", __func__);
		rc = mt9m114_snapshot_config(mode);
		break;

	default:
		return -EINVAL;
	}

	CDBG("%s: =================== x\n", __func__);
	return 0;
}

static int mt9m114_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id = 0;
	int rc = 0;

	CDBG("%s: e line=%d\n", __func__, __LINE__);
	data->cam_setup(&mt9m114_client->dev, 1);

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

#if 0 // PP에서 #if 0 으로 rollback
	if(mt9m114_ctrl == NULL) {
		mt9m114_ctrl = kzalloc(sizeof(struct mt9m114_ctrl), GFP_KERNEL);
		if (!mt9m114_ctrl) {
			CDBG("mt9m114_init failed!\n");
			rc = -ENOMEM;
	  	goto init_probe_fail;
		}
	}

	if (data)
		mt9m114_ctrl->sensordata = data;
#endif

	/* Micron suggested Power up block End */
	/* Read the Model ID of the sensor */
	rc = mt9m114_i2c_read(mt9m114_client->addr,
		REG_MT9M114_MODEL_ID, &model_id, WORD_LEN);
	if (rc < 0)
		goto init_probe_fail;

	CDBG("mt9m114 model_id = 0x%x\n", model_id);

	/* Check if it matches it with the value in Datasheet */
	if (model_id != MT9M114_MODEL_ID) {
		rc = -EINVAL;
		goto init_probe_fail;
	}

	CDBG("%s: x \n", __func__);
	goto init_probe_done;

init_probe_fail:
	CDBG(" mt9m114_probe_init_sensor fails\n");
	gpio_direction_output(data->sensor_reset, 0);
	mt9m114_probe_init_done(data);

init_probe_done:
	if(data->sensor_other_reset_supported)	 {
		gpio_set_value_cansleep(data->sensor_other_reset, 0);	
	}

	CDBG(" mt9m114_probe_init_sensor finishes\n");
	return rc;
}

int mt9m114_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

  CDBG("%s: e\n", __func__);

	mt9m114_ctrl = kzalloc(sizeof(struct mt9m114_ctrl), GFP_KERNEL);
	if (!mt9m114_ctrl) {
		CDBG("mt9m114_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		mt9m114_ctrl->sensordata = data;

	msm_camio_clk_rate_set(MT9M114_DEFAULT_CLOCK_RATE);
	rc = mt9m114_probe_init_sensor(data);
	if (rc < 0) {
		CDBG("mt9m114_sensor_init failed!\n");
		goto init_fail;
	}

	rc = mt9m114_sensor_setting(REG_INIT, RES_PREVIEW);
  
	if (rc < 0) {
		goto init_fail;
	} else
		goto init_done;


init_done:
	CDBG("%s: x init_done\n", __func__);
	return rc;

init_fail:
	CDBG("%s: init_fail\n", __func__);
	gpio_direction_output(data->sensor_reset, 0);
	mt9m114_probe_init_done(data);
	mt9m114_power_down(); //boba
	return rc;
}

static int mt9m114_init_client(struct i2c_client *client)
{
  CDBG("%s: line=%d\n", __func__, __LINE__);

	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9m114_wait_queue);
	return 0;
}

int mt9m114_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

  //CDBG("%s: line=%d\n", __func__, __LINE__);

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

  /* down(&mt9m114_sem); */

  //CDBG("mt9m114_ioctl, cfgtype = %d, mode = %d\n", cfg_data.cfgtype, cfg_data.mode);

		switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = mt9m114_set_sensor_mode(
						cfg_data.mode);
			break;

		case CFG_SET_EFFECT:
			rc = mt9m114_set_effect(cfg_data.mode, cfg_data.cfg.effect);
			break;
			
		case CFG_SET_BRIGHTNESS:
			rc = mt9m114_set_brightness(cfg_data.cfg.brightness);
			break;
			
		case CFG_SET_WB:
			rc = mt9m114_set_wb(cfg_data.cfg.wb_val);
			break;

		case CFG_GET_AF_MAX_STEPS:
		default:
			rc = -EINVAL;
			break;
		}

	/* up(&mt9m114_sem); */

	return rc;
}

int mt9m114_sensor_release(void)
{
	int rc = 0;
	CDBG("%s: line=%d\n", __func__, __LINE__);

	mutex_lock(&mt9m114_mut);

	mt9m114_power_down();
#if 0
	gpio_set_value_cansleep(mt9m114_ctrl->sensordata->sensor_reset, 0);
	msleep(5);
	gpio_free(mt9m114_ctrl->sensordata->sensor_reset);
#endif

	kfree(mt9m114_ctrl);
	mt9m114_ctrl = NULL;

	mutex_unlock(&mt9m114_mut);

	return rc;
}

static int mt9m114_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

  CDBG("%s: line=%d\n", __func__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9m114_sensorw =
		kzalloc(sizeof(struct mt9m114_work), GFP_KERNEL);

	if (!mt9m114_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9m114_sensorw);
	mt9m114_init_client(client);
	mt9m114_client = client;
	//mt9m114_client->addr = mt9m114_client->addr >> 1;

	CDBG("mt9m114_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(mt9m114_sensorw);
	mt9m114_sensorw = NULL;
	CDBG("mt9m114_probe failed!\n");
	return rc;
}

static const struct i2c_device_id mt9m114_i2c_id[] = {
	{ "mt9m114", 0},
	{ },
};

static struct i2c_driver mt9m114_i2c_driver = {
	.id_table = mt9m114_i2c_id,
	.probe  = mt9m114_i2c_probe,
	.remove = __exit_p(mt9m114_i2c_remove),
	.driver = {
		.name = "mt9m114",
	},
};

static int mt9m114_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&mt9m114_i2c_driver);
	printk("%s : rc=%d    mt9m114_client=%p\n", __func__, rc, mt9m114_client);

	if (rc < 0 || mt9m114_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}

#if 0 // PP에서 #if 0 으로 rollback
	msm_camio_clk_rate_set(MT9M114_DEFAULT_CLOCK_RATE);
	rc = mt9m114_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
#endif
	s->s_init = mt9m114_sensor_open_init;
	s->s_release = mt9m114_sensor_release;
	s->s_config  = mt9m114_sensor_config;
	s->s_camera_type = FRONT_CAMERA_2D;
//	s->s_mount_angle = 180; 
	s->s_mount_angle = info->sensor_platform_info->mount_angle;

  printk(KERN_INFO "MT9M114__ kuzuri:  mount_angle= %d", s->s_mount_angle);

  
 
	gpio_set_value_cansleep(info->sensor_reset, 0);

	goto probe_done;
 
probe_fail:
	CDBG("mt9m114_sensor_probe: SENSOR PROBE FAILS!\n");
probe_done:
#if 0 // PP에서 #if 0 으로 rollback
 if (mt9m114_ctrl->sensordata->cam_shutdown != NULL)
	  mt9m114_ctrl->sensordata->cam_shutdown(&mt9m114_client->dev, 0); 
#endif
	return rc;
}

static int __mt9m114_probe(struct platform_device *pdev)
{
  //CDBG("%s: line=%d\n", __func__, __LINE__);
	return msm_camera_drv_start(pdev, mt9m114_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9m114_probe,
	.driver = {
		.name = "msm_camera_mt9m114",
		.owner = THIS_MODULE,
	},
};

static int __init mt9m114_init(void)
{
  //CDBG("%s: line=%d\n", __func__, __LINE__);
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9m114_init);
