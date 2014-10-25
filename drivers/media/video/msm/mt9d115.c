
// htcheck_d115 [ driver ]

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
  
#include "mt9d115.h"
//#include <linux/jiffies.h> 

/* Micron MT9D115 Registers and their values */
/*
 *  Camera i2c data, clock
 */ 
#define  MT9D115_I2C_NAME         "mt9d115"

/* 
 * Camera image sensor slave address
 */
#define  MT9D115_I2C_WR_ID     0x7A  // 

#define  MT9D115_DEFAULT_CLOCK_RATE  24000000

/* Sensor Core Registers */
#define  REG_MT9D115_MODEL_ID 0x0000
#define  MT9D115_MODEL_ID     0x2580

#define SENSOR_DEBUG 1

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "driver.mt9d115: " fmt, ##args) 

struct mt9d115_work {
	struct work_struct work;
};


static struct  mt9d115_work *mt9d115_sensorw;
static struct  i2c_client *mt9d115_client;

struct mt9d115_ctrl_t {
	const struct msm_camera_sensor_info *sensordata;
};


static struct mt9d115_ctrl_t *mt9d115_ctrl = NULL;

static bool PREV_FRONT_CAMCORDER_SUPPORT ;

static DECLARE_WAIT_QUEUE_HEAD(mt9d115_wait_queue);
DEFINE_MUTEX(mt9d115_mut);

static int16_t mt9d115_effect = CAMERA_EFFECT_OFF;
static bool CSI_CONFIG;


//static unsigned long inactivity_time1;
//static unsigned long inactivity_time2;
//static unsigned int delayed_ms;
/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct mt9d115_reg mt9d115_regs;


/*=============================================================*/
static int32_t mt9d115_i2c_txdata(unsigned short saddr,
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
	if (i2c_transfer(mt9d115_client->adapter, msg, 1) < 0) {
		CDBG("mt9d115_i2c_txdata failed\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9d115_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum mt9d115_width width)
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
		// CDBG("mt9d115_i2c_write: waddr= 0x%04x wdata=0x%04x\n", waddr, wdata);
#endif
		rc = mt9d115_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = waddr;
		buf[1] = wdata;
		rc = mt9d115_i2c_txdata(saddr, buf, 2);
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG( "i2c_write failed, addr = 0x%x, val = 0x%x!\n", waddr, wdata);

	return rc;
}

static int32_t mt9d115_i2c_write_table(
                                      	struct mt9d115_i2c_reg_conf const *reg_conf_tbl,
                                      	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9d115_i2c_write(mt9d115_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata,
			reg_conf_tbl->width);
		if (rc < 0)
			break;
		if (reg_conf_tbl->mdelay_time != 0)
			mdelay(reg_conf_tbl->mdelay_time);
		reg_conf_tbl++;
	}

	return rc;
}

static int mt9d115_i2c_rxdata(unsigned short saddr,
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
		CDBG("msm_io_i2c_r: 0x%04x 0x%04x\n", *(u16 *) rxdata, *(u16 *) (rxdata + 2));
	else if (length == 4)
		CDBG("msm_io_i2c_r: 0x%04x\n", *(u16 *) rxdata);
	else
		CDBG("msm_io_i2c_r: length = %d\n", length);
#endif

	if (i2c_transfer(mt9d115_client->adapter, msgs, 2) < 0) {
		CDBG("mt9d115_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9d115_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum mt9d115_width width)
	
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

		rc = mt9d115_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0] << 8 | buf[1];
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG("mt9d115_i2c_read addr = 0x%x failed!\n", saddr);

	return rc;
}

static int32_t mt9d115_seq_probe(void)
{
	int32_t rc = -EINVAL;
	int32_t i = 0;
	unsigned short readData;

	do {
		msleep(40);
		mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN);    
		mt9d115_i2c_read(mt9d115_client->addr, 0x0990, &readData, WORD_LEN);
		i++;
		if (readData==0)
			rc = 0;
	} while (rc != 0 && i < 20);
	CDBG("%s: readData(0x%x), i(%d)\n", __func__, readData, i);
	// rc = mt9d115_i2c_write(mt9d115_client->addr, 0x001A, 0x0010, WORD_LEN); //{0x098C, 0xA103, WORD_LEN, 0  },  // MCU_ADDRESS [SEQ_CMD] 
	return rc;
}

static int32_t mt9d115_stop_stream(void)
{
	int32_t rc = 0;

	CDBG("%s\n", __func__);
	return rc;
}

static int32_t mt9d115_start_stream(void)
{
	int32_t rc = 0;

#if 1 // jykim CTS: reduce delay
	int32_t i = 0;
	unsigned short readData;

	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA005, WORD_LEN);    //{0x098C, 0xA005, WORD_LEN, 0 }, 	// MCU_ADDRESS [MON_CMD]
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0001, WORD_LEN);    //{0x0990, 0x0001, WORD_LEN, 300}, 	// MCU_DATA_0

	do {
		msleep(20);
		mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA024, WORD_LEN);    
		mt9d115_i2c_read(mt9d115_client->addr, 0x0990, &readData, WORD_LEN);
		i++;
		if (readData!=0)
			rc = 0;
	} while (rc != 0 && i < 20);
	
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x326C, 0x1404, WORD_LEN);    //{0x326C, 0x1404, WORD_LEN, 0 }, 	// APERTURE_PARAMETERS
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA207, WORD_LEN);    //{0x098C, 0xA207, WORD_LEN, 0 }, 	// MCU_ADDRESS
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0008, WORD_LEN);    //{0x0990, 0x0008, WORD_LEN, 0 }, 	// MCU_DATA_0
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA208, WORD_LEN);    //{0x098C, 0xA208, WORD_LEN, 0 }, 	// MCU_ADDRESS
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0000, WORD_LEN);    //{0x0990, 0x0000, WORD_LEN, 0 }, 	// MCU_DATA_0
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24C, WORD_LEN);    //{0x098C, 0xA24C, WORD_LEN, 0 }, 	// MCU_ADDRESS
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0020, WORD_LEN);    //{0x0990, 0x0020, WORD_LEN, 0 }, 	// MCU_DATA_0
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xAB06, WORD_LEN);    //{0x098C, 0xAB06, WORD_LEN, 0 }, 	// MCU_ADDRESS
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x001E, WORD_LEN);    //{0x0990, 0x001E, WORD_LEN, 0 }, 	// MCU_DATA_0
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA102, WORD_LEN);    //{0x098C, 0xA102, WORD_LEN, 0 }, 	// MCU_ADDRESS
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x000B, WORD_LEN);    //{0x0990, 0x000B, WORD_LEN, 0 }, 	// MCU_DATA_0
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0018, 0x0028, WORD_LEN);    //{0x0018, 0x0028, WORD_LEN, 100}, // STANDBY_CONTROL
	msleep(10);
	
#endif

	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN);    
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0006, WORD_LEN);    
	rc = mt9d115_seq_probe();
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN);    
	rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN);    
	rc = mt9d115_seq_probe();

	CDBG("%s\n", __func__);
	return rc;
}


static int32_t mt9d115_sensor_setting(int update_type, int rt)
{
	int32_t rc = 0;
	unsigned short readData = 0;	
	struct msm_camera_csi_params mt9d115_csi_params;

	switch (update_type) {
	case REG_INIT:
		CDBG("%s: REG_INIT \n", __func__);
		CSI_CONFIG = 0;
		break;

	case UPDATE_PERIODIC:
		CDBG("%s: UPDATE_PERIODIC \n", __func__);

		if (rt == RES_PREVIEW) {
			CDBG("%s:PREV_FRONT_CAMCORDER_SUPPORT = %d\n", __func__, PREV_FRONT_CAMCORDER_SUPPORT);

			// if(PREV_FRONT_CAMCORDER_SUPPORT) { //bobafett video mode
            			if(!CSI_CONFIG) {
            				rc = mt9d115_stop_stream();
            
            				mt9d115_csi_params.data_format = CSI_8BIT;
            				mt9d115_csi_params.lane_cnt = 1;
            				mt9d115_csi_params.lane_assign = 0xe4;
            				mt9d115_csi_params.dpcm_scheme = 0;
            				mt9d115_csi_params.settle_cnt = 20;
            
            				rc = msm_camio_csi_config(&mt9d115_csi_params);
            				msleep(50);			
            
            				CDBG("%s: write reg_table \n", __func__);
            				rc = mt9d115_i2c_write_table(&mt9d115_regs.regtbl[0],
            									mt9d115_regs.regtbl_size);
            
            				rc = mt9d115_start_stream();
            				CSI_CONFIG = 1;
            			}	
            
            			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA104, WORD_LEN); 	 // MCU_ADDRESS [SEQ_STATE] 
            			rc = mt9d115_i2c_read(mt9d115_client->addr, 0x0990, &readData, WORD_LEN);
            			if (readData != 3) {
            				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA115, WORD_LEN); // MCU_ADDRESS [SEQ_CMD] 
            				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0000, WORD_LEN); // MCU_DATA_0
            				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD] 
            				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0001, WORD_LEN); // MCU_DATA_0
            				rc = mt9d115_seq_probe();
            			}
            
				if(PREV_FRONT_CAMCORDER_SUPPORT) { //bobafett video mode
					// fixed 27.5 fps
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA20C, WORD_LEN); 	// MCU_ADDRESS [AE_MAX_INDEX]
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0004, WORD_LEN); 	// MCU_DATA_0
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2B28, WORD_LEN); 	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTART]
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0800, WORD_LEN); 	// MCU_DATA_0
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2B2A, WORD_LEN); 	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTOP]
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0C80, WORD_LEN); 	// MCU_DATA_0
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); 	// MCU_ADDRESS
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); 	// MCU_DATA_0
					rc = mt9d115_seq_probe();
				} else {
					rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA20C, WORD_LEN); 	 // MCU_ADDRESS [SEQ_STATE] 
					rc = mt9d115_i2c_read(mt9d115_client->addr, 0x0990, &readData, WORD_LEN);
					// if (readData != 0x10) {
					if (readData != 0x08) {	// 2011.09.02 min.15fps
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA20C, WORD_LEN); 	// MCU_ADDRESS [AE_MAX_INDEX]
						// rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0010, WORD_LEN); 	// MCU_DATA_0
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0008, WORD_LEN); 	// MCU_DATA_0 // 2011.09.02 min.15fps
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2B28, WORD_LEN); 	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTART]
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0A30, WORD_LEN); 	// MCU_DATA_0
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2B2A, WORD_LEN); 	// MCU_ADDRESS [HG_LL_BRIGHTNESSSTOP]
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x1200, WORD_LEN); 	// MCU_DATA_0
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x221F, WORD_LEN); 		// MCU_ADDRESS [AE_DGAIN_AE1]			
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0080, WORD_LEN); 	   // MCU_DATA_0
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); 	// MCU_ADDRESS
						rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); 	// MCU_DATA_0
						rc = mt9d115_seq_probe();
					}
				}
				//inactivity_time2 = jiffies;
				//delayed_ms = jiffies_to_msecs(inactivity_time2-inactivity_time1);
				//CDBG("====================================================== from probe_init_sensor to sensor_setting   = %d ms\n", delayed_ms);
				return rc;

		}
		else if(rt == RES_CAPTURE) {
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA115, WORD_LEN); // MCU_ADDRESS [SEQ_CMD] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0002, WORD_LEN); // MCU_DATA_0
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0002, WORD_LEN); // MCU_DATA_0
			rc = mt9d115_seq_probe();
			return rc;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int32_t mt9d115_power_down(void)
{
	CDBG("%s: e\n", __func__);

#ifdef CONFIG_KTTECH_CAMERA_MT9D115 // jykim
	if (mt9d115_ctrl->sensordata->cam_shutdown != NULL)
	  mt9d115_ctrl->sensordata->cam_shutdown(&mt9d115_client->dev, 1); // Front camera
#endif

	CDBG("%s: x\n", __func__);
	return 0;
}

static int mt9d115_probe_init_done(const struct msm_camera_sensor_info *data)
{
	CDBG("%s: e\n", __func__);

	gpio_set_value_cansleep(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);

	CDBG("%s: x\n", __func__);
	return 0;
}


static int mt9d115_set_brightness(int brightness)
{
	long rc = 0;

	//CDBG("%s: brightness=%d == e\n", __func__, brightness);

	switch (brightness) {

		case 0: { // Brightness -5
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0014, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 1: { // Brightness -3
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0024, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 2: { // Brightness -2
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0036, WORD_LEN); // MCU_DATA_0
		}

		case 3: { // Brightness 0
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0048, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 4: { // Brightness +2
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x005C, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 5: { // Brightness +3
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0070, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 6: { // Brightness +5
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA24F, WORD_LEN); // MCU_ADDRESS 			// [PRI_A_CONFIG_AE_TRACK_BASE_TARGET]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0086, WORD_LEN); // MCU_DATA_0
		}
		break;

		default: {
		}
	}


	//CDBG("%s: x\n", __func__);
	return rc;

}

static int mt9d115_set_wb(int wb)
{

	long rc = 0;
	//CDBG("%s: wb=%d === e\n", __func__, wb);

	switch (wb) {

		case 1: { // CAMERA_WB_AUTO
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34A, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MIN] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0059, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34B, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MAX] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00B6, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34C, WORD_LEN); // MCU_ADDRESS [AWB_GAINMIN_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0059, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34D, WORD_LEN); // MCU_ADDRESS [AWB_GAINMAX_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00B6, WORD_LEN); // MCU_DATA_0                 
			//rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34E, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_R]  
			//rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A4, WORD_LEN); // MCU_DATA_0                 
			//rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA350, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_B]  
			//rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0079, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 5: { // CAMERA_WB_DAYLIGHT
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34A, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MIN] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x009A, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34B, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MAX] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A4, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34C, WORD_LEN); // MCU_ADDRESS [AWB_GAINMIN_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0079, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34D, WORD_LEN); // MCU_ADDRESS [AWB_GAINMAX_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0083, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34E, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_R]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A0, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA350, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_B]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0080, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 6: { // CAMERA_WB_CLOUDY_DAYLIGHT
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34A, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MIN] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x009D, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34B, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MAX] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A7, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34C, WORD_LEN); // MCU_ADDRESS [AWB_GAINMIN_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0077, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34D, WORD_LEN); // MCU_ADDRESS [AWB_GAINMAX_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0081, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34E, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_R]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A0, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA350, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_B]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0080, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 4: { // CAMERA_WB_FLUORESCENT
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34A, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MIN] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0090, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34B, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MAX] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x009A, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34C, WORD_LEN); // MCU_ADDRESS [AWB_GAINMIN_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A3, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34D, WORD_LEN); // MCU_ADDRESS [AWB_GAINMAX_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00AD, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34E, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_R]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0095, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA350, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_B]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00A8, WORD_LEN); // MCU_DATA_0
		}
		break;

		case 3: { // CAMERA_WB_INCANDESCENT
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34A, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MIN] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x006B, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34B, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_MAX] 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0075, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34C, WORD_LEN); // MCU_ADDRESS [AWB_GAINMIN_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00B1, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34D, WORD_LEN); // MCU_ADDRESS [AWB_GAINMAX_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00BB, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA34E, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_R]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0070, WORD_LEN); // MCU_DATA_0                 
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA350, WORD_LEN); // MCU_ADDRESS [AWB_GAIN_B]  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x00B8, WORD_LEN); // MCU_DATA_0
		}
		break;

		rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]								  
		rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); // MCU_DATA_0
		rc = mt9d115_seq_probe();

		default: {
		}
	}


	//CDBG("%s: x\n", __func__);
	return rc;

}

static long mt9d115_set_effect(int mode, int effect)
{

	long rc = 0;
	unsigned short readData = 0;

	//CDBG("%s: mode=%d, effect=%d  == e\n", __func__, mode, effect);

	switch (effect) {
		case CAMERA_EFFECT_OFF: { // 0
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2759, WORD_LEN); 	 // MCU_ADDRESS [SEQ_STATE] 
			rc = mt9d115_i2c_read(mt9d115_client->addr, 0x0990, &readData, WORD_LEN);
			if ((readData&0x000F)) {
				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2759, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6440, WORD_LEN); // MCU_DATA_0                       
				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x275B, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6440, WORD_LEN); // MCU_DATA_0                       
				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]            
				rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); // MCU_DATA_0
				rc = mt9d115_seq_probe();
			}
		}
		break;

		case CAMERA_EFFECT_MONO: { // 1
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2759, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6441, WORD_LEN); // MCU_DATA_0                       
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x275B, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6441, WORD_LEN); // MCU_DATA_0                       
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]            
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); // MCU_DATA_0	
			rc = mt9d115_seq_probe();
		}
		break;

		case CAMERA_EFFECT_NEGATIVE: { // 2
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2759, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6443, WORD_LEN); // MCU_DATA_0                       
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x275B, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6443, WORD_LEN); // MCU_DATA_0                       
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]            
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); // MCU_DATA_0
			rc = mt9d115_seq_probe();
		}
		break;

		case CAMERA_EFFECT_SOLARIZE: { // 3
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2759, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6445, WORD_LEN); // MCU_DATA_0                       
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x275B, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6445, WORD_LEN); // MCU_DATA_0                       
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]            
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); // MCU_DATA_0
			rc = mt9d115_seq_probe();
		}
		break;

		case CAMERA_EFFECT_SEPIA: { // 7
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2763, WORD_LEN); // MCU_ADDRESS [MODE_COMMONMODESETTINGS_FX_SEPIA_SETTINGS]
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0xDD13, WORD_LEN); // MCU_DATA_0                                            
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x2759, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_A]                      
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6442, WORD_LEN); // MCU_DATA_0                                            
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0x275B, WORD_LEN); // MCU_ADDRESS [MODE_SPEC_EFFECTS_B]                      
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x6442, WORD_LEN); // MCU_DATA_0                                            
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x098C, 0xA103, WORD_LEN); // MCU_ADDRESS [SEQ_CMD]                                  
			rc = mt9d115_i2c_write(mt9d115_client->addr, 0x0990, 0x0005, WORD_LEN); // MCU_DATA_0
			rc = mt9d115_seq_probe();
		}
		break;

		default: {
		}
	}
	mt9d115_effect = effect;

	//CDBG("%s: x\n", __func__);
	return rc;

}

static int32_t mt9d115_video_config(int mode)
{
	int32_t rc = 0;
	int rt;

	/* change sensor resolution if needed */
	rt = RES_PREVIEW;

	if (mt9d115_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;

	return rc;
}

static int32_t mt9d115_snapshot_config(int mode)
{
	int32_t rc = 0;	
	int rt;
	rt = RES_CAPTURE;

	if (mt9d115_sensor_setting(UPDATE_PERIODIC, rt) < 0)
		return rc;

	return rc;
}

static long mt9d115_set_sensor_mode(int mode, int res)
{
	int rc = 0;

	CDBG("%s: e ================   mode=%d\n", __func__, mode);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		CDBG("%s: SENSOR_PREVIEW_MODE res:%d \n", __func__, res);
		if(res == SENSOR_FRONT_CAMCORDER_SIZE) {
			PREV_FRONT_CAMCORDER_SUPPORT = 1;
		} 
		else {
			PREV_FRONT_CAMCORDER_SUPPORT = 0;
		}

		rc = mt9d115_video_config(mode);
		break;

	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		CDBG("%s: SENSOR_SNAPSHOT_MODE or SENSOR_RAW_SNAPSHOT_MODE\n", __func__);
		rc = mt9d115_snapshot_config(mode);
		break;

	default:
		return -EINVAL;
	}

	CDBG("%s: =================== x\n", __func__);
	return 0;
}

static int mt9d115_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id = 0;
	int rc = 0;

#ifdef CONFIG_KTTECH_CAMERA_MT9D115 // jykim
	CDBG("%s: e line=%d\n", __func__, __LINE__);
	data->cam_setup(&mt9d115_client->dev, 1);
#endif

	//inactivity_time1 = jiffies;

	CDBG("%s: %d sensor_reset=%d\n", __func__, __LINE__, data->sensor_reset);
	gpio_direction_output(data->sensor_reset, 0);
  if(gpio_get_value(98))
  {
    CDBG(" 11111111111111111 gpio get value \n" );
  }
  else
  {
    CDBG(" 00000000000000000 gpio get value \n" );
  }
	gpio_set_value_cansleep(data->sensor_reset, 0);
	msleep(10);
  if(gpio_get_value(98))
  {
    CDBG(" 11111111111111111 gpio get value \n" );
  }
  else
  {
    CDBG(" 00000000000000000 gpio get value \n" );
  }
	gpio_set_value_cansleep(data->sensor_reset, 1);
	msleep(10);
  if(gpio_get_value(98))
  {
    CDBG(" 11111111111111111 gpio get value \n" );
  }
  else
  {
    CDBG(" 00000000000000000 gpio get value \n" );
  }

	if(mt9d115_ctrl == NULL) {
		mt9d115_ctrl = kzalloc(sizeof(struct mt9d115_ctrl_t), GFP_KERNEL);
		if (!mt9d115_ctrl) {
			CDBG("mt9d115_init failed!\n");
			rc = -ENOMEM;
			goto init_probe_fail;
		}
	}

	if (data)
		mt9d115_ctrl->sensordata = data;

	/* Micron suggested Power up block End */
	/* Read the Model ID of the sensor */
	rc = mt9d115_i2c_read(mt9d115_client->addr,
		REG_MT9D115_MODEL_ID, &model_id, WORD_LEN);
	if (rc < 0)
		goto init_probe_fail;

	CDBG("mt9d115 model_id = 0x%x\n", model_id);

	/* Check if it matches it with the value in Datasheet */
	if (model_id != MT9D115_MODEL_ID) {
		rc = -EINVAL;
		goto init_probe_fail;
	}

	CDBG("%s: x \n", __func__);
	goto init_probe_done;

init_probe_fail:
	CDBG(" mt9d115_probe_init_sensor fails\n");
	gpio_direction_output(data->sensor_reset, 0);
	mt9d115_probe_init_done(data);
init_probe_done:
	CDBG(" mt9d115_probe_init_sensor finishes\n");
	return rc;
}

int mt9d115_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

  CDBG("%s: e\n", __func__);

	mt9d115_ctrl = kzalloc(sizeof(struct mt9d115_ctrl_t), GFP_KERNEL);
	if (!mt9d115_ctrl) {
		CDBG("mt9d115_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		mt9d115_ctrl->sensordata = data;

	msm_camio_clk_rate_set(MT9D115_DEFAULT_CLOCK_RATE);
	rc = mt9d115_probe_init_sensor(data);
	if (rc < 0) {
		CDBG("mt9d115_sensor_init failed!\n");
		goto init_fail;
	}

	rc = mt9d115_sensor_setting(REG_INIT, RES_PREVIEW);
  
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
	mt9d115_probe_init_done(data);
	
	return rc;
}

static int mt9d115_init_client(struct i2c_client *client)
{
  CDBG("%s: line=%d\n", __func__, __LINE__);

	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9d115_wait_queue);
	return 0;
}

int mt9d115_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

  //CDBG("%s: line=%d\n", __func__, __LINE__);

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

  /* down(&mt9d115_sem); */

  CDBG("mt9d115_ioctl, cfgtype = %d, mode = %d\n", cfg_data.cfgtype, cfg_data.mode);

		switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = mt9d115_set_sensor_mode(cfg_data.mode, cfg_data.rs);
			break;

		case CFG_SET_EFFECT:
			rc = mt9d115_set_effect(cfg_data.mode, cfg_data.cfg.effect);
			break;
			
		case CFG_SET_BRIGHTNESS:
			rc = mt9d115_set_brightness(cfg_data.cfg.brightness);
			break;
			
		case CFG_SET_WB:
			rc = mt9d115_set_wb(cfg_data.cfg.wb_val);
			break;

		case CFG_GET_AF_MAX_STEPS:
		default:
			rc = -EINVAL;
			break;
		}

	/* up(&mt9d115_sem); */

	return rc;
}

int mt9d115_sensor_release(void)
{
	int rc = 0;
	CDBG("%s: line=%d\n", __func__, __LINE__);

	mutex_lock(&mt9d115_mut);

	mt9d115_power_down();
#ifndef CONFIG_KTTECH_CAMERA_MT9D115 // jykim remove warning
	gpio_set_value_cansleep(mt9d115_ctrl->sensordata->sensor_reset, 0);
	msleep(5);
	gpio_free(mt9d115_ctrl->sensordata->sensor_reset);
#endif

	kfree(mt9d115_ctrl);
	mt9d115_ctrl = NULL;

	mutex_unlock(&mt9d115_mut);

	return rc;
}

static int mt9d115_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

  CDBG("%s: line=%d\n", __func__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9d115_sensorw =
		kzalloc(sizeof(struct mt9d115_work), GFP_KERNEL);

	if (!mt9d115_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9d115_sensorw);
	mt9d115_init_client(client);
	mt9d115_client = client;
	mt9d115_client->addr = mt9d115_client->addr >> 1;

	CDBG("mt9d115_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(mt9d115_sensorw);
	mt9d115_sensorw = NULL;
	CDBG("mt9d115_probe failed!\n");
	return rc;
}

static const struct i2c_device_id mt9d115_i2c_id[] = {
	{ "mt9d115", 0},
	{ },
};

static struct i2c_driver mt9d115_i2c_driver = {
	.id_table = mt9d115_i2c_id,
	.probe  = mt9d115_i2c_probe,
	.remove = __exit_p(mt9d115_i2c_remove),
	.driver = {
		.name = "mt9d115",
	},
};

static int mt9d115_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&mt9d115_i2c_driver);
	CDBG("%s : rc=%d    mt9d115_client=%p\n", __func__, rc, mt9d115_client);
	if (rc < 0 || mt9d115_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}

#if 0 // PP에서 #if 0 으로 rollback
	msm_camio_clk_rate_set(MT9D115_DEFAULT_CLOCK_RATE);
	rc = mt9d115_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
#endif

	s->s_init = mt9d115_sensor_open_init;
	s->s_release = mt9d115_sensor_release;
	s->s_config  = mt9d115_sensor_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = 180; // jykim x-y flip
 
	gpio_set_value_cansleep(info->sensor_reset, 0);

	goto probe_done;
 
probe_fail:
	CDBG("mt9d115_sensor_probe: SENSOR PROBE FAILS!\n");
probe_done:
#if 0 // PP에서 #if 0 으로 rollback
	if (mt9d115_ctrl->sensordata->cam_shutdown != NULL)
	  mt9d115_ctrl->sensordata->cam_shutdown(&mt9d115_client->dev, 1); // Front camera
#endif
	return rc;
}

static int __mt9d115_probe(struct platform_device *pdev)
{
  //CDBG("%s: line=%d\n", __func__, __LINE__);
	return msm_camera_drv_start(pdev, mt9d115_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9d115_probe,
	.driver = {
		.name = "msm_camera_mt9d115",
		.owner = THIS_MODULE,
	},
};

static int __init mt9d115_init(void)
{
  //CDBG("%s: line=%d\n", __func__, __LINE__);
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9d115_init);
