/* bma150_i2c.c
 *
 * Accelerometer device driver for I2C
 *
 * Copyright (C) 2011-2012 ALPS ELECTRIC CO., LTD. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#if 1 // KTFT
#include <linux/fs.h>
#include <linux/miscdevice.h>
#endif

#define I2C_RETRY_DELAY	5
#define I2C_RETRIES		5

/* Register Name for accsns */
#define ACC_XOUT		0x02
#define ACC_YOUT		0x04
#define ACC_ZOUT		0x06
#define ACC_TEMP		0x08
#define ACC_REG0B		0x0B
#define ACC_REG0A		0x0A
#define ACC_REG14		0x14

#define ACC_DRIVER_NAME "accsns_i2c"
#define I2C_ACC_ADDR (0x38)		/* 011 1000	*/
#define I2C_BUS_NUMBER	5

#if 1 // KTFT
#define BMA150_DEVICE_FILE_NAME "bma250_dev"
#define SMB380_RANGE_2G			0 /**< sets range to 2G mode \see smb380_set_range() */
#define SMB380_BW_25HZ		0	/**< sets bandwidth to 25HZ \see smb380_set_bandwidth() */
#define BMA150_CALIBRATION_MAX_TRIES 20
#define NO_ERR					0x00
#define NO_CALIB				0x100
#define CALIB_ERR_MOV 			0x80
#define CALIB_X_AXIS			1
#define CALIB_Y_AXIS			2
#define CALIB_Z_AXIS			4

#define SMB380_X_AXIS_LSB_REG		0x02
#define SMB380_X_AXIS_MSB_REG		0x03
#define SMB380_Y_AXIS_LSB_REG		0x04
#define SMB380_Y_AXIS_MSB_REG		0x05
#define SMB380_Z_AXIS_LSB_REG		0x06
#define SMB380_Z_AXIS_MSB_REG		0x07
#define SMB380_CTRL_REG		0x0a
#define SMB380_RANGE_BWIDTH_REG	0x14
#define SMB380_OFFS_GAIN_X_REG		0x16
#define SMB380_OFFSET_X_REG		0x1a
#define SMB380_EEP_OFFSET   0x20




/* BANDWIDTH dependend definitions */

#define SMB380_BANDWIDTH__POS				0
#define SMB380_BANDWIDTH__LEN			 	3
#define SMB380_BANDWIDTH__MSK			 	0x07
#define SMB380_BANDWIDTH__REG				SMB380_RANGE_BWIDTH_REG

/* RANGE */

#define SMB380_RANGE__POS				3
#define SMB380_RANGE__LEN				2
#define SMB380_RANGE__MSK				0x18	
#define SMB380_RANGE__REG				SMB380_RANGE_BWIDTH_REG

#define SMB380_EE_W__POS			4
#define SMB380_EE_W__LEN			1
#define SMB380_EE_W__MSK			0x10
#define SMB380_EE_W__REG			SMB380_CTRL_REG

#define SMB380_OFFSET_X_LSB__POS	6
#define SMB380_OFFSET_X_LSB__LEN	2
#define SMB380_OFFSET_X_LSB__MSK	0xC0
#define SMB380_OFFSET_X_LSB__REG	SMB380_OFFS_GAIN_X_REG

#define SMB380_OFFSET_X_MSB__POS	0
#define SMB380_OFFSET_X_MSB__LEN	8
#define SMB380_OFFSET_X_MSB__MSK	0xFF
#define SMB380_OFFSET_X_MSB__REG	SMB380_OFFSET_X_REG

#define SMB380_ACC_X_LSB__POS   	6
#define SMB380_ACC_X_LSB__LEN   	2
#define SMB380_ACC_X_LSB__MSK		0xC0
#define SMB380_ACC_X_LSB__REG		SMB380_X_AXIS_LSB_REG

#define SMB380_ACC_X_MSB__POS   	0
#define SMB380_ACC_X_MSB__LEN   	8
#define SMB380_ACC_X_MSB__MSK		0xFF
#define SMB380_ACC_X_MSB__REG		SMB380_X_AXIS_MSB_REG

#define SMB380_ACC_Y_LSB__POS   	6
#define SMB380_ACC_Y_LSB__LEN   	2
#define SMB380_ACC_Y_LSB__MSK   	0xC0
#define SMB380_ACC_Y_LSB__REG		SMB380_Y_AXIS_LSB_REG

#define SMB380_ACC_Y_MSB__POS   	0
#define SMB380_ACC_Y_MSB__LEN   	8
#define SMB380_ACC_Y_MSB__MSK   	0xFF
#define SMB380_ACC_Y_MSB__REG		SMB380_Y_AXIS_MSB_REG

#define SMB380_ACC_Z_LSB__POS   	6
#define SMB380_ACC_Z_LSB__LEN   	2
#define SMB380_ACC_Z_LSB__MSK		0xC0
#define SMB380_ACC_Z_LSB__REG		SMB380_Z_AXIS_LSB_REG

#define SMB380_ACC_Z_MSB__POS   	0
#define SMB380_ACC_Z_MSB__LEN   	8
#define SMB380_ACC_Z_MSB__MSK		0xFF
#define SMB380_ACC_Z_MSB__REG		SMB380_Z_AXIS_MSB_REG


#define SMB380_GET_BITSLICE(regvar, bitname)\
			(regvar & bitname##__MSK) >> bitname##__POS


#define SMB380_SET_BITSLICE(regvar, bitname, val)\
		  (regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK)  


/** SMB380 acceleration data 
	\brief Structure containing acceleration values for x,y and z-axis in signed short

*/

typedef struct  {
		short x, /**< holds x-axis acceleration data sign extended. Range -512 to 511. */
			  y, /**< holds y-axis acceleration data sign extended. Range -512 to 511. */
			  z; /**< holds z-axis acceleration data sign extended. Range -512 to 511. */
} smb380acc_t;
#endif

static struct i2c_driver accsns_driver;
static struct i2c_client *client_accsns = NULL;
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend accsns_early_suspend_handler;
#endif

static atomic_t flgEna;
static atomic_t flgSuspend;

static int retry_count=0;//20120807/kyuhak.choi/calibration fail

static int accsns_i2c_readm(u8 *rxData, int length)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = client_accsns->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = rxData,
		 },
		{
		 .addr = client_accsns->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	do {
		err = i2c_transfer(client_accsns->adapter, msgs, 2);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&client_accsns->adapter->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int accsns_i2c_writem(u8 *txData, int length)
{
	int err;
	int tries = 0;
#ifdef ALPS_DEBUG
	int i;
#endif

	struct i2c_msg msg[] = {
		{
		 .addr = client_accsns->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

#ifdef ALPS_DEBUG
	printk("[ACC] i2c_writem : ");
	for (i=0; i<length;i++) printk("0X%02X, ", txData[i]);
	printk("\n");
#endif

	do {
		err = i2c_transfer(client_accsns->adapter, msg, 1);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&client_accsns->adapter->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

int accsns_get_acceleration_data(int *xyz)
{
	int err = -1;
	int i;
	u8 sx[6];

    if (atomic_read(&flgSuspend) == 1) return err;
	sx[0] = ACC_XOUT;
	err = accsns_i2c_readm(sx, 6);
	if (err < 0) return err;
	for (i=0; i<3; i++) {
		xyz[i] = (sx[2 * i] >> 6) | (sx[2 * i + 1] << 2);
		if (xyz[i] & 0x200) xyz[i] = (xyz[i] | 0xFFFFFC00);
	}

#if 1//kyuhak.choi/2012/04/06/temp acc sensor.	
	xyz[0]=(-1*xyz[0]);
	//xyz[1]=(-1*xyz[1]);
	xyz[2]=(-1*xyz[2]);
#endif

#ifdef ALPS_DEBUG
	/*** DEBUG OUTPUT - REMOVE ***/
	printk("Acc_I2C, x:%d, y:%d, z:%d\n", xyz[0], xyz[1], xyz[2]);
	/*** <end> DEBUG OUTPUT - REMOVE ***/
#endif

	return err;
}

void accsns_activate(int flgatm, int flg)
{
	u8 buf[2];

	if (flg != 0) flg = 1;

	buf[0] = ACC_REG14    ; buf[1] = 0;
	accsns_i2c_writem(buf, 2);
	buf[0] = ACC_REG0B    ; buf[1] = 0;
	accsns_i2c_writem(buf, 2);
	buf[0] = ACC_REG0A;
	if (flg == 0) buf[1] = 0x01;
	else          buf[1] = 0x00;
	accsns_i2c_writem(buf, 2);
	mdelay(2);
	if (flgatm) atomic_set(&flgEna, flg);
}

static void accsns_register_init(void)
{
    int d[3];
    u8  buf[2];

#ifdef ALPS_DEBUG
	printk("[ACC] register_init\n");
#endif

    buf[0] = ACC_REG0A;
    buf[1] = 0x02;
    accsns_i2c_writem(buf, 2);
    mdelay(4);

    accsns_activate(0, 1);
    accsns_get_acceleration_data(d);
    printk("[ACC] x:%d y:%d z:%d\n",d[0],d[1],d[2]);
    accsns_activate(0, 0);
}

#if 1 // KTFT
/**	set smb380s range 
 \param range 
 \return  result of bus communication function
 
 \see SMB380_RANGE_2G		
 \see SMB380_RANGE_4G			
 \see SMB380_RANGE_8G			
*/
int smb380_set_range(char range) 
{			
	int comres = 0;
	unsigned char data[2];

	if (range<3) {
		data[0] = SMB380_RANGE__REG;
		comres = accsns_i2c_readm(&data[0], 1);
		data[1] = SMB380_SET_BITSLICE(data[0], SMB380_RANGE, range);
		data[0] = SMB380_RANGE__REG;
		comres += accsns_i2c_writem(&data[0], 2);
	}
	return comres;
}

/** set SMB380 internal filter bandwidth
   \param bw bandwidth (see bandwidth constants)
   \return result of bus communication function
   \see #define SMB380_BW_25HZ, SMB380_BW_50HZ, SMB380_BW_100HZ, SMB380_BW_190HZ, SMB380_BW_375HZ, SMB380_BW_750HZ, SMB380_BW_1500HZ
   \see smb380_get_bandwidth()
*/
int smb380_set_bandwidth(char bw) 
{
	int comres = 0;
	unsigned char data[2];

	if (bw<8) {
		data[0] = SMB380_BANDWIDTH__REG;
		comres = accsns_i2c_readm(&data[0], 1);
		data[1] = SMB380_SET_BITSLICE(data[0], SMB380_BANDWIDTH, bw);
		data[0] = SMB380_BANDWIDTH__REG;
		comres += accsns_i2c_writem(&data[0], 2);
	}
	return comres;
}

/** write offset data to SMB380 image
   \param eew 0 = lock EEPROM 1 = unlock EEPROM 
   \return result of bus communication function
*/
int smb380_set_ee_w(unsigned char eew)
{
	unsigned char data[2];
	int comres;

	data[0] = SMB380_EE_W__REG;
	comres = accsns_i2c_readm(&data[0], 1);
	data[1] = SMB380_SET_BITSLICE(data[0], SMB380_EE_W, eew);
	data[0] = SMB380_EE_W__REG;
	comres = accsns_i2c_writem(&data[0], 2);
	return comres;
}

/** read out offset data from 
   \param xyz select axis x=0, y=1, z=2
   \param *offset pointer to offset value (offset is in offset binary representation
   \return result of bus communication function
   \note use smb380_set_ee_w() function to enable access to offset registers 
*/
int smb380_get_offset(unsigned char xyz, unsigned short *offset) 
{
	int comres;
	unsigned char data;

	data = (SMB380_OFFSET_X_LSB__REG+xyz);
	comres = accsns_i2c_readm(&data, 1);
	data = SMB380_GET_BITSLICE(data, SMB380_OFFSET_X_LSB);
	*offset = data;
	data = (SMB380_OFFSET_X_MSB__REG+xyz);
	comres += accsns_i2c_readm(&data, 1);
	*offset |= (data<<2);
	return comres;
}

/** read function for raw register access

		\param addr register address
		\param *data pointer to data array for register read back
		\param len number of bytes to be read starting from addr
	
*/

int smb380_read_reg(unsigned char addr, unsigned char *data, unsigned char len)
{
	int comres;

	data[0] = addr;
	comres = accsns_i2c_readm(&data[0], len);
	return comres;
}

/** calls the linked wait function

		\param msec amount of mili seconds to pause
		\return number of mili seconds waited
*/

int smb380_pause(int msec) 
{
	mdelay(msec);	
	return msec;
}

/** write offset data to SMB380 image
   \param xyz select axis x=0, y=1, z=2
   \param offset value to write (offset is in offset binary representation
   \return result of bus communication function
   \note use smb380_set_ee_w() function to enable access to offset registers 
*/
int smb380_set_offset(unsigned char xyz, unsigned short offset) 
{
	int comres;
	unsigned char data[2];

	data[0] = (SMB380_OFFSET_X_LSB__REG+xyz);
	comres = accsns_i2c_readm(&data[0], 1);
	data[1] = SMB380_SET_BITSLICE(data[0], SMB380_OFFSET_X_LSB, offset);
	data[0] = (SMB380_OFFSET_X_LSB__REG+xyz);
	comres += accsns_i2c_writem(&data[0], 2);
	data[1] = (offset&0x3ff)>>2;
	data[0] = (SMB380_OFFSET_X_MSB__REG+xyz);
	comres += accsns_i2c_writem(&data[0], 2);
	return comres;
}

/** write offset data to SMB380 image
   \param xyz select axis x=0, y=1, z=2
   \param offset value to write to eeprom(offset is in offset binary representation
   \return result of bus communication function
   \note use smb380_set_ee_w() function to enable access to offset registers in EEPROM space
*/
int smb380_set_offset_eeprom(unsigned char xyz, unsigned short offset) 
{
	int comres;
	unsigned char data[2];

	data[0] = (SMB380_OFFSET_X_LSB__REG+xyz);
	comres = accsns_i2c_readm(&data[0], 1);
	data[1] = SMB380_SET_BITSLICE(data[0], SMB380_OFFSET_X_LSB, offset);
	data[0] = (SMB380_EEP_OFFSET+SMB380_OFFSET_X_LSB__REG + xyz);
	comres += accsns_i2c_writem(&data[0], 2);   
	mdelay(34);
	data[1] = (offset&0x3ff)>>2;
	data[0] = (SMB380_EEP_OFFSET+ SMB380_OFFSET_X_MSB__REG+xyz);
	comres += accsns_i2c_writem(&data[0], 2);
	mdelay(34);
	return comres;
}

/** Write the given calibration register in the sensor either in the image registers or the EEPROM
  \param *offset takes the sensor calibration values
  \param EEPROM when EEPROM is 0, hte Image register will be updated. otherwise EEPROM is being written
  \return 0x00 Offset words successful updated
*/
int bma150_writeCalib(smb380acc_t *offset,int EEPROM)
{
	int comres;

	comres = smb380_set_ee_w(1);
	if (EEPROM)
	{
		comres |= smb380_set_offset_eeprom(0, offset->x);
		comres |= smb380_set_offset_eeprom(1, offset->y);
		comres |= smb380_set_offset_eeprom(2, offset->z);
	}
	else
	{
		comres = smb380_set_offset(0, offset->x);
		comres |= smb380_set_offset(1, offset->y);
		comres |= smb380_set_offset(2, offset->z);
	}
	comres |= smb380_set_ee_w(0);
	return comres;
}

/** X,Y and Z-axis acceleration data readout 
	\param *acc pointer to \ref smb380acc_t structure for x,y,z data readout
	\note data will be read by multi-byte protocol into a 6 byte structure 
*/
int smb380_read_accel_xyz(smb380acc_t * acc)
{
	int comres;
	unsigned char data[6];

	data[0] = SMB380_ACC_X_LSB__REG;
	comres = accsns_i2c_readm(&data[0], 6);
	
	acc->x = SMB380_GET_BITSLICE(data[0],SMB380_ACC_X_LSB) | (SMB380_GET_BITSLICE(data[1],SMB380_ACC_X_MSB)<<SMB380_ACC_X_LSB__LEN);
	acc->x = acc->x << (sizeof(short)*8-(SMB380_ACC_X_LSB__LEN+SMB380_ACC_X_MSB__LEN));
	acc->x = acc->x >> (sizeof(short)*8-(SMB380_ACC_X_LSB__LEN+SMB380_ACC_X_MSB__LEN));

	acc->y = SMB380_GET_BITSLICE(data[2],SMB380_ACC_Y_LSB) | (SMB380_GET_BITSLICE(data[3],SMB380_ACC_Y_MSB)<<SMB380_ACC_Y_LSB__LEN);
	acc->y = acc->y << (sizeof(short)*8-(SMB380_ACC_Y_LSB__LEN + SMB380_ACC_Y_MSB__LEN));
	acc->y = acc->y >> (sizeof(short)*8-(SMB380_ACC_Y_LSB__LEN + SMB380_ACC_Y_MSB__LEN));
	
	
	acc->z = SMB380_GET_BITSLICE(data[4],SMB380_ACC_Z_LSB); 
	acc->z |= (SMB380_GET_BITSLICE(data[5],SMB380_ACC_Z_MSB)<<SMB380_ACC_Z_LSB__LEN);
	acc->z = acc->z << (sizeof(short)*8-(SMB380_ACC_Z_LSB__LEN+SMB380_ACC_Z_MSB__LEN));
	acc->z = acc->z >> (sizeof(short)*8-(SMB380_ACC_Z_LSB__LEN+SMB380_ACC_Z_MSB__LEN));
	
	return comres;
	
}

/** reads out acceleration data and averages them, measures min and max
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param num_avg numer of samples for averaging
  \param *min returns the minimum measured value
  \param *max returns the maximum measured value
  \param *average returns the average value
 */
int bma150_read_accel_avg(int num_avg, smb380acc_t *min, smb380acc_t *max, smb380acc_t *avg )
{
   	long x_avg=0, y_avg=0, z_avg=0;   
   	int comres=0;
   	int i;
   	smb380acc_t accel;		                /* read accel data */

   	x_avg = 0; y_avg=0; z_avg=0;                  
   	max->x = -512; max->y =-512; max->z = -512;
   	min->x = 512;  min->y = 512; min->z = 512;  

	for (i=0; i<num_avg; i++) {
		comres += smb380_read_accel_xyz(&accel);      /* read 10 acceleration data triples */
		if (accel.x > max->x)
			max->x = accel.x;
		if (accel.x < min->x) 
			min->x = accel.x;

		if (accel.y > max->y)
			max->y = accel.y;
		if (accel.y < min->y) 
			min->y = accel.y;

		if (accel.z > max->z)
			max->z = accel.z;
		if (accel.z < min->z) 
			min->z = accel.z;
		
		x_avg += accel.x;
		y_avg += accel.y;
		z_avg += accel.z;

		smb380_pause(10);
	}
	avg->x = (x_avg / num_avg);                             /* calculate averages, min and max values */
	avg->y = (y_avg / num_avg);
	avg->z = (z_avg / num_avg);
	return comres;
}

/** verifies the accerleration values to be good enough for calibration calculations
  \param min takes the minimum measured value
  \param max takes the maximum measured value
  \param takes returns the average value
  \return 1: min,max values are in range, 0: not in range
*/
int bma150_verify_min_max(smb380acc_t min, smb380acc_t max, smb380acc_t avg) 
{
	short dx, dy, dz;
	int ver_ok=1;

	dx =  ((max.x) - (min.x));    /* calc delta max-min */
	dy =  ((max.y) - (min.y));
	dz =  ((max.z) - (min.z));


	if ((dx> 10) || (dx<-10)) 
	  ver_ok = 0;
    if ((dy> 10) || (dy<-10)) 
	  ver_ok = 0;
	if ((dz> 10) || (dz<-10)) 
	  ver_ok = 0;

	return ver_ok;
}

/** calculates new offset in respect to acceleration data and old offset register values
  \param orientation pass orientation one axis needs to be absolute 1 the others need to be 0
  \param accel holds the measured acceleration value of one axis
  \param *offset takes the old offset value and modifies it to the new calculated one
  \param stepsize holds the value of the calculated stepsize for one axis
  \param *min_accel_adapt stores the minimal accel_adapt to see if calculations make offset worse
  \param *min_offset stores old offset bevor actual calibration to make resetting possible in next function call
  \param *min_reached stores value to prevent further calculations if minimum for an axis reached 
  \return	0x00: Axis was already calibrated
			0x01: Axis needs calibration
			0x11: Axis needs calibration and the calculated offset was outside boundaries
 */
int bma150_calc_new_offset(short orientation, short accel, short *offset, short *stepsize, short *min_accel_adapt, short *min_offset, short *min_reached, short *prev_steps, int iteration_counter)
{
	short old_offset;
	short new_offset;
	short accel_adapt;
	short steps = 0;

   	unsigned char  calibrated = 0;

   	old_offset = *offset;
   
   	accel_adapt = accel - (orientation * 256);
	if(accel_adapt < -512)
	{
	 	accel_adapt = -512;
	}
	else if(accel_adapt > 511)
	{
	 	accel_adapt = 511;
	}
		                                
   	if (((accel_adapt > 7) || (accel_adapt < -7)) && !(*min_reached))	/* does the axis need calibration? minimum for this axis not yet reached? */
   	{
		if (abs(accel_adapt) <= abs(*min_accel_adapt))				/* accel_adapt smaller than minimal accel_adapt ?
															   			means: previous offset calculation lead to better result */
		{
			if(((3*(*prev_steps) * (*stepsize)) <= 2*(abs((*min_accel_adapt) - (accel_adapt)))) && (iteration_counter >= 1))/* if calculated stepsize is too small compared to real stepsize*/
			{
				(*stepsize) = (abs((*min_accel_adapt) - (accel_adapt))) / (*prev_steps);	/* adapt stepsize */
			}
						
			if ((accel_adapt < (*stepsize)) && (accel_adapt > 0))	/* check for values less than quantisation of offset register */
				new_offset = old_offset -1;          
		 	else if ((accel_adapt > -(*stepsize)) && (accel_adapt < 0))    
		   		new_offset = old_offset +1;
	     	else
			{
				steps = (accel_adapt/(*stepsize));					/* calculate offset LSB */
				if((2*(abs(steps * (*stepsize) - accel_adapt))) > (*stepsize))	/* for better performance (example: accel_adapt = -25, stepsize = 13 --> two steps better than one) */
				{
					if(accel_adapt < 0)
						steps-=1;
					else
						steps+=1;	
				} 
	       		new_offset = old_offset - steps;					/* calculate new offset */
			}
	     	
			if (new_offset < 0)										/* check for register boundary */
			{
				new_offset = 0;										/* <0 ? */
				calibrated = 0x10;
			}
	     	else if (new_offset > 1023)
		 	{
				new_offset = 1023;									/* >1023 ? */
				calibrated = 0x10;
		 	}

			*prev_steps = abs(steps);								/* store number of steps */
			if(*prev_steps==0)										/* at least 1 step is done */
				*prev_steps = 1;
						
			*min_accel_adapt = accel_adapt; 						/* set current accel_adapt value as minimum */
			*min_offset = old_offset;								/* store old offset (value before calculations above) */

			*offset = new_offset;									/* set offset as calculated */
		}
		else
		{
			*offset = *min_offset;									/* restore old offset */
			if((2*(*prev_steps) * (*stepsize)) <= (abs((*min_accel_adapt) - (accel_adapt))))/* if calculated stepsize is too small compared to real stepsize*/
			{
				(*stepsize) = (abs((*min_accel_adapt) - (accel_adapt))) / (*prev_steps);	/* adapt stepsize */
			}
			else
			{
				*min_reached = 0x01;								/* prevent further calibrations */
			}	
		}
					    
	 	calibrated |= 0x01;
   	}
  	return calibrated;
}

static ssize_t bma150_xyz_cal_enable_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
    if (cnt != 1)
        return -EINVAL;

    if (ubuf[0] == 1) {
        short offset_x, offset_y, offset_z;
        int need_calibration=0, min_max_ok=0;
        int error_flag=0;
        int iteration_counter =0;
        int dummy;
        int tries = BMA150_CALIBRATION_MAX_TRIES;

//****************************************************************************	
        unsigned char dummy2;
        short min_accel_adapt_x = 0xFE00, min_accel_adapt_y = 0xFE00, min_accel_adapt_z = 0xFE00;
        short min_offset_x = 0, min_offset_y = 0, min_offset_z = 0; 
        short min_reached_x	= 0, min_reached_y	= 0, min_reached_z	= 0;
        short prev_steps_x = 0x01FF, prev_steps_y = 0x01FF, prev_steps_z = 0x01FF;
//****************************************************************************

        smb380acc_t min,max,avg,gain,step;
        smb380acc_t orientation;
        smb380acc_t offset;

        orientation.x = 0;
        orientation.y = 0;
        orientation.z = 1; //MO2 daehyok.ryu 2012.07.22 Modify Screen rotation

        smb380_set_range(SMB380_RANGE_2G);
        smb380_set_bandwidth(SMB380_BW_25HZ); 
        smb380_set_ee_w(1);
        smb380_get_offset(0, &offset_x);
        smb380_get_offset(1, &offset_y);
        smb380_get_offset(2, &offset_z);
	

//****************************************************************************
        smb380_read_reg(0x16, &dummy2, 1);			/* read gain registers from image */
        gain.x = dummy2 & 0x3F;
        smb380_read_reg(0x17, &dummy2, 1);
        gain.y = dummy2 & 0x3F;
        smb380_read_reg(0x18, &dummy2, 1);
        gain.z = dummy2 & 0x3F;
        smb380_set_ee_w(0);

        step.x = gain.x * 15/64 + 7;				/* calculate stepsizes for all 3 axes */
        step.y = gain.y * 15/64 + 7;
        step.z = gain.z * 15/64 + 7;

        smb380_pause(50);							/* needed to prevent CALIB_ERR_MOV */
//***************************************************************************

        do {

            bma150_read_accel_avg(10, &min, &max, &avg);  		/* read acceleration data min, max, avg */

            min_max_ok = bma150_verify_min_max(min, max, avg);
#if 1//20120807/kyuhak.choi/calibration fail
            if (!min_max_ok)	{/* check if calibration is possible */	
		if(retry_count>2)
                    return CALIB_ERR_MOV;
		retry_count++;
	 }
#else
            if (!min_max_ok)									/* check if calibration is possible */
                return CALIB_ERR_MOV;
#endif
            need_calibration = 0;

            /* x-axis */
            dummy = bma150_calc_new_offset(orientation.x, avg.x, &offset_x, &step.x, &min_accel_adapt_x, &min_offset_x, &min_reached_x, &prev_steps_x, iteration_counter);
            smb380_set_ee_w(1);
            if (dummy & 0x01)									/* x-axis calibrated ? */
            {					
                smb380_set_offset(0, offset_x);
                need_calibration = CALIB_X_AXIS;
            }
            if (dummy & 0x10)									/* x-axis offset register boundary reached */
                error_flag |= (CALIB_X_AXIS << 4);

			
            /* y-axis */
            dummy = bma150_calc_new_offset(orientation.y, avg.y, &offset_y, &step.y, &min_accel_adapt_y, &min_offset_y, &min_reached_y, &prev_steps_y, iteration_counter);
            if (dummy & 0x01)									/* y-axis calibrated ? */
            {
                smb380_set_offset(1, offset_y);
                need_calibration |= CALIB_Y_AXIS;
            }
            if (dummy & 0x10)									/* y-axis offset register boundary reached ? */
                error_flag |= (CALIB_Y_AXIS << 4);

			
            /* z-axis */
            dummy = bma150_calc_new_offset(orientation.z, avg.z, &offset_z, &step.z, &min_accel_adapt_z, &min_offset_z, &min_reached_z, &prev_steps_z, iteration_counter);
            if (dummy & 0x01)									/* z-axis calibrated ? */
            {
                smb380_set_offset(2, offset_z);
                need_calibration |= CALIB_Z_AXIS;
            }
            if (dummy & 0x10)									/* z-axis offset register boundary reached */
                error_flag |= (CALIB_Z_AXIS << 4);
			

            smb380_set_ee_w(0);
            iteration_counter++;

            if (need_calibration)	/* if one of the offset got changed wait for the filter to fill with the new acceleration */
            {
                smb380_pause(50);
            }
        } while (need_calibration && (iteration_counter != tries));

        if(iteration_counter == 1)					/* no calibration needed at all */
        {
            error_flag |= NO_CALIB;
            return error_flag;
        }
        else if (iteration_counter == tries)		/* further calibration needed */
        {
            error_flag |= need_calibration;
            return error_flag;
        }

        offset.x = offset_x;
        offset.y = offset_y;
        offset.z = offset_z;
		
        bma150_writeCalib(&offset, 1);

        return NO_ERR;
    }	
    return 1;
}

static int bma150_open(struct inode *inode, struct file *file)
{
    retry_count=0;//20120807/kyuhak.choi/calibration fail
    return 0;
}

static int bma150_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long bma150_ioctl(struct file *file, unsigned int cmd,  unsigned long arg)
{
    return 0;
}

static ssize_t bma150_xyz_cal_data_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
    unsigned short offset_x = 0;
    unsigned short offset_y = 0;
    unsigned short offset_z = 0;

    if (cnt < 12)
        return -EINVAL;

    smb380_set_ee_w(1);

    if (smb380_get_offset(0, &(offset_x)) < 0)
        printk("bma150_get_offset_filt_x read error\n");

    ubuf[0] = offset_x & 0xFF;
    ubuf[1] = (offset_x >> 8) & 0xFF;
    ubuf[2] = (offset_x >> 16) & 0xFF;
    ubuf[3] = (offset_x >> 24) & 0xFF;

    if (smb380_get_offset(1, &(offset_y)) < 0)
        printk("bma150_get_offset_filt_y read error\n");

    ubuf[4] = offset_y & 0xFF;
    ubuf[5] = (offset_y >> 8) & 0xFF;
    ubuf[6] = (offset_y >> 16) & 0xFF;
    ubuf[7] = (offset_y >> 24) & 0xFF;

    if (smb380_get_offset(2, &(offset_z)) < 0)
        printk("bma150_get_offset_filt_z read error\n");

    ubuf[8] = offset_z & 0xFF;
    ubuf[9] = (offset_z >> 8) & 0xFF;
    ubuf[10] = (offset_z >> 16) & 0xFF;
    ubuf[11] = (offset_z >> 24) & 0xFF;

    smb380_set_ee_w(0);

    return 12;
}

static struct file_operations bma150_fops = {
    .owner = THIS_MODULE,
    .open = bma150_open,
    .release = bma150_release,
    .unlocked_ioctl = bma150_ioctl,
    .read = bma150_xyz_cal_data_read,
    .write = bma150_xyz_cal_enable_write,
};


static struct miscdevice bma150_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = BMA150_DEVICE_FILE_NAME,
    .fops = &bma150_fops,
};
#endif

static int accsns_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	printk("[ACC] probe\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->adapter->dev, "client not i2c capable\n");
		return -ENOMEM;
	}

	client_accsns = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!client_accsns) {
		dev_err(&client->adapter->dev, "failed to allocate memory for module data\n");
		return -ENOMEM;
	}

#if 1 // KTFT	
    if (misc_register(&bma150_device)) {
        printk(KERN_ERR "!!! bma150_probe: misc failed\n");
    }
#endif

	dev_info(&client->adapter->dev, "detected accelerometer\n");

	return 0;
}

static int __devexit accsns_remove(struct i2c_client *client)
{
#ifdef ALPS_DEBUG
    printk("[ACC] remove\n");
#endif
	accsns_activate(0, 0);

#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&accsns_early_suspend_handler);
#endif
    kfree(client_accsns);
    return 0;
}

static int accsns_suspend(struct i2c_client *client,pm_message_t mesg)
{
#ifdef ALPS_DEBUG
	printk("[ACC] suspend\n");
#endif
    atomic_set(&flgSuspend, 1);
	accsns_activate(0, 0);

	return 0;
}

static int accsns_resume(struct i2c_client *client)
{
#ifdef ALPS_DEBUG
	printk("[ACC] resume\n");
#endif
    atomic_set(&flgSuspend, 0);
	accsns_activate(0, atomic_read(&flgEna));

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void accsns_early_suspend(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
    printk("[ACC] early_suspend\n");
#endif
    accsns_suspend(client_accsns, PMSG_SUSPEND);
}

static void accsns_early_resume(struct early_suspend *handler)
{
#ifdef ALPS_DEBUG
    printk("[ACC] early_resume\n");
#endif
    accsns_resume(client_accsns);
}
#endif

static const struct i2c_device_id accsns_id[] = {
	{ ACC_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver accsns_driver = {
	.probe     = accsns_probe,
    .remove    = accsns_remove,
	.id_table  = accsns_id,
	.driver    = {
  		.name	= ACC_DRIVER_NAME,
	},
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= accsns_suspend,
	.resume		= accsns_resume,
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend accsns_early_suspend_handler = {
    .suspend = accsns_early_suspend,
    .resume  = accsns_early_resume,
};
#endif

static int __init accsns_init(void)
{
	struct i2c_board_info i2c_info;
	struct i2c_adapter *adapter;
	int rc;

#ifdef ALPS_DEBUG
	printk("[ACC] init\n");
#endif
	atomic_set(&flgEna, 0);
    atomic_set(&flgSuspend, 0);
	rc = i2c_add_driver(&accsns_driver);
	if (rc != 0) {
		printk("can't add i2c driver\n");
		rc = -ENOTSUPP;
		return rc;
	}

	memset(&i2c_info, 0, sizeof(struct i2c_board_info));
	i2c_info.addr = I2C_ACC_ADDR;
	strlcpy(i2c_info.type, ACC_DRIVER_NAME , I2C_NAME_SIZE);
	
	adapter = i2c_get_adapter(I2C_BUS_NUMBER);
	if (!adapter) {
		printk("can't get i2c adapter %d\n", I2C_BUS_NUMBER);
		rc = -ENOTSUPP;
		goto probe_done;
	}

	client_accsns = i2c_new_device(adapter, &i2c_info);
	client_accsns->adapter->timeout = 0;
	client_accsns->adapter->retries = 0;
	  
	i2c_put_adapter(adapter);
	if (!client_accsns) {
		printk("can't add i2c device at 0x%x\n",(unsigned int)i2c_info.addr);
		rc = -ENOTSUPP;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&accsns_early_suspend_handler);
#endif

    accsns_register_init();

#ifdef ALPS_DEBUG
	printk("accsns_open end !!!!\n");
#endif
	
	probe_done: 

	return rc;
}

static void __exit accsns_exit(void)
{
#ifdef ALPS_DEBUG
	printk("[ACC] exit\n");
#endif
	i2c_del_driver(&accsns_driver);
}

module_init(accsns_init);
module_exit(accsns_exit);

EXPORT_SYMBOL(accsns_get_acceleration_data);
EXPORT_SYMBOL(accsns_activate);

MODULE_DESCRIPTION("Alps Accelerometer Device");
MODULE_AUTHOR("ALPS ELECTRIC CO., LTD.");
MODULE_LICENSE("GPL v2");
