/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "hynix_hi1337_iii_Sensor.h"

#define PFX "hi1337_camera_sensor"
#define LOG_INF(format, args...)    \
	pr_devel(PFX "[%s] " format, __func__, ##args)

//PDAF
#define ENABLE_PDAF 1
#define e2prom 0

#define per_frame 1

extern bool read_hi1337_eeprom( kal_uint16 addr, BYTE *data, kal_uint32 size); 
extern bool read_eeprom( kal_uint16 addr, BYTE * data, kal_uint32 size);
extern unsigned char fusion_id_main[48];
#define HI1337_VENDOR_ID  0x41 //0x41 for AAC
 

#define MULTI_WRITE 1
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = HYNIX_HI1337_III_SENSOR_ID,

	.checksum_value = 0xb7c53a42,       //0x6d01485c // Auto Test Mode ����..

	.pre = {
		.pclk = 576000000,	 //VT CLK : 72MHz * 8 = =	576000000				//record different mode's pclk
		.linelength =  5760, //720*8		//record different mode's linelength
		.framelength = 3322, //1666, 			//record different mode's framelength
		.startx = 0,				    //record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2104, 		//record different mode's width of grabwindow
		.grabwindow_height = 1560,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000, //(720M*4/10)
	},
	.cap = {
		.pclk =576000000,
		.linelength = 5760,
		.framelength = 3333,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000, //(1440M * 4 / 10 )
	},
	// need to setting
    .cap1 = {
		.pclk = 600000000,
		.linelength = 5760,
		.framelength = 6662,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
		.mipi_pixel_rate = 285600000,//(714M*4/10)
    },
	.normal_video = {
		.pclk =576000000,
		.linelength = 5760, 	
		.framelength = 3333, 
		.startx = 0,	
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000, //(1440M * 4 / 10 )
	},
	.hs_video = {
        .pclk = 576000000,
        .linelength = 5760,				
		.framelength = 833,			
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280 ,		
		.grabwindow_height = 720 ,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 1200,
		.mipi_pixel_rate = 192000000, //( 480*4/10)
	},
    .slim_video = {
		.pclk = 576000000,
		.linelength = 5760,
		.framelength = 1666,
		.startx = 0,
		.starty = 0,
    	.grabwindow_width = 1920,
    	.grabwindow_height = 1080,
    	.mipi_data_lp2hs_settle_dc = 85,//unit , ns
    	.max_framerate = 600,
    	.mipi_pixel_rate = 288000000, //(720M*4/10)
    },
	.custom1 = {
		.pclk = 576000000,
		.linelength = 5760,
		.framelength = 4149,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,
	},
	.margin = 7,
	.min_shutter = 7,
	.max_frame_length = 0x325AA0,
#if per_frame
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
#else
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
#endif

	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 6,	  //support sensor mode num

	.cap_delay_frame = 1,
	.pre_delay_frame = 1,
	.video_delay_frame = 0,
	.hs_video_delay_frame = 1,
	.slim_video_delay_frame = 1,
	.custom1_delay_frame = 1,
	.frame_time_delay_frame = 1,//TODO_xieyue
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x40,0xff},
	.i2c_speed = 1000,
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x0100,
    .gain = 0xe0,
    .dummy_pixel = 0,
    .dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x40,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[6] = {
    { 4208, 3120,     0,    0, 4208, 3120, 2104, 1560,   0, 0, 2104, 1560,   0, 0, 2104, 1560}, // Preview
    { 4208, 3120,     0,    0, 4208, 3120, 4208, 3120,   0, 0, 4208, 3120,   0, 0, 4208, 3120}, // capture
    { 4208, 3120,     0,    0, 4208, 3120, 4208, 3120,   0, 0, 4208, 3120,   0, 0, 4208, 3120}, // video
    { 4208, 3120,   824,  600, 2560, 1920,  640,  480,   0, 0,  640,  480,   0, 0,  640,  480}, //hight speed video
    { 4208, 3120,   184,  480, 3840, 2160, 1920, 1080,   0, 0, 1920, 1080,   0, 0, 1920, 1080},// slim video
    { 4208, 3120,     0,    0, 4208, 3120, 4208, 3120,   0, 0, 3264, 2448,   0, 0, 3264, 2448}, //  custom1 24fps (3264 x 2448)
};


#if ENABLE_PDAF
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX = 56,
    .i4OffsetY = 24,
    .i4PitchX = 32,
    .i4PitchY = 32,
    .i4PairNum =8,
    .i4SubBlkW =16,
    .i4SubBlkH =8,
    .i4PosL = {{60,29},{76,29},{68,33},{84,33},{60,45},{76,45},{68,49},{84,49}},
    .i4PosR = {{60,25},{76,25},{68,37},{84,37},{60,41},{76,41},{68,53},{84,53}},
    .i4BlockNumX = 128,
    .i4BlockNumY = 96,
    /* 0:IMAGE_NORMAL,1:IMAGE_H_MIRROR,2:IMAGE_V_MIRROR,3:IMAGE_HV_MIRROR */
    .iMirrorFlip = 0,
};
#endif


#if MULTI_WRITE
#define I2C_BUFFER_LEN 1020

static kal_uint16 hi1337_table_write_cmos_sensor(
					kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data >> 8);
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;
		}

		if ((I2C_BUFFER_LEN - tosend) < 4 ||
			len == IDX ||
			addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend,
				imgsensor.i2c_write_id,
				4, imgsensor_info.i2c_speed);

			tosend = 0;
		}
	}
	return 0;
}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n",
		imgsensor.dummy_line, imgsensor.dummy_pixel);
	write_cmos_sensor(0x020e, imgsensor.frame_length & 0xFFFF); 
	write_cmos_sensor(0x0206, imgsensor.line_length/8);

}	/*	set_dummy  */

static kal_uint32 return_sensor_id(void)
{
	return (((read_cmos_sensor(0x0716) << 8) | read_cmos_sensor(0x0717))+2);

}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
			frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length -
		imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length -
			imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;

	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
    kal_uint32 realtime_fps = 0;

    // Test
    kal_uint32 line_le = 0;
    kal_uint32 frame_le = 0;


    spin_lock(&imgsensor_drv_lock);

    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);

    LOG_INF("shutter = %d, imgsensor.frame_length = %d, imgsensor.min_frame_length = %d\n",
        shutter, imgsensor.frame_length, imgsensor.min_frame_length);


    shutter = (shutter < imgsensor_info.min_shutter) ?
        imgsensor_info.min_shutter : shutter;
    shutter = (shutter >
        (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
        (imgsensor_info.max_frame_length - imgsensor_info.margin) :
        shutter;
    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk /
            (imgsensor.line_length * imgsensor.frame_length) * 10;
        if (realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if (realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
        else
            write_cmos_sensor(0x020e, imgsensor.frame_length);
    } else{
            write_cmos_sensor(0x020e, imgsensor.frame_length);
    }

    write_cmos_sensor_8(0x020D, (shutter & 0xFF0000) >> 16 );
    write_cmos_sensor(0x020A, shutter);

    frame_le = (read_cmos_sensor(0x020E) << 8) | read_cmos_sensor(0x020F);
    line_le = (read_cmos_sensor(0x0206) << 8) | read_cmos_sensor(0x0207);

    LOG_INF("frame_length = %d , shutter = %d \n", imgsensor.frame_length, shutter);

    LOG_INF("frame = %d, line = %d\n", frame_le, line_le);


}	/*	write_shutter  */

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	LOG_INF("set_shutter");
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

static void set_shutter_frame_length(
				kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
//	pr_err("hi1337 %s %d\n", __func__, __LINE__);

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
	? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk
			/ imgsensor.line_length * 10 / imgsensor.frame_length;

		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x020e, imgsensor.frame_length);
		}
	} else {
			write_cmos_sensor(0x020e, imgsensor.frame_length);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x020D, (shutter & 0xFF0000) >> 16 );
	write_cmos_sensor(0x020A, shutter);

	LOG_INF("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
		shutter, imgsensor.frame_length,
		frame_length, dummy_line, read_cmos_sensor(0x0350));

}

/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 gain2reg(kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0000;
    reg_gain = gain / 4 - 16;

    return (kal_uint16)reg_gain;

}


static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X    */
    /* [4:9] = M meams M X         */
    /* Total gain = M + N /16 X   */

    if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
        LOG_INF("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 16 * BASEGAIN)
            gain = 16 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
	
    write_cmos_sensor_8(0x0213,reg_gain);
	return gain;

}

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le,
				kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	if (imgsensor.ihdr_en) {
		spin_lock(&imgsensor_drv_lock);
		if (le > imgsensor.min_frame_length - imgsensor_info.margin)
			imgsensor.frame_length = le + imgsensor_info.margin;
		else
			imgsensor.frame_length = imgsensor.min_frame_length;
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			imgsensor.frame_length =
				imgsensor_info.max_frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (le < imgsensor_info.min_shutter)
			le = imgsensor_info.min_shutter;
		if (se < imgsensor_info.min_shutter)
			se = imgsensor_info.min_shutter;
		// Extend frame length first
		write_cmos_sensor(0x0006, imgsensor.frame_length);
		write_cmos_sensor(0x3502, (le << 4) & 0xFF);
		write_cmos_sensor(0x3501, (le >> 4) & 0xFF);
		write_cmos_sensor(0x3500, (le >> 12) & 0x0F);
		write_cmos_sensor(0x3508, (se << 4) & 0xFF);
		write_cmos_sensor(0x3507, (se >> 4) & 0xFF);
		write_cmos_sensor(0x3506, (se >> 12) & 0x0F);
		set_gain(gain);
	}
}
#endif


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0000, 0x0000);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0000, 0x0100);

		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0000, 0x0200);

		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0000, 0x0300);

		break;
	default:
		LOG_INF("Error image_mirror setting");
		break;
	}

}
#endif
/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/

#if MULTI_WRITE
static kal_uint16 addr_data_pair_init_hi1337[] = {

/*
DISP_DATE ="2020-01-09 15:01:41"
DISP_FORMAT = BAYER10_PACKED
DISP_DATAORDER = GR
MIPI_LANECNT = 4
*/
0x0790, 0x0100,
0x2000, 0x0000,
0x2002, 0x0058,
0x2006, 0x40B2,
0x2008, 0xB05C,
0x200A, 0x8446,
0x200C, 0x40B2,
0x200E, 0xB082,
0x2010, 0x8450,
0x2012, 0x40B2,
0x2014, 0xB0A0,
0x2016, 0x84C6,
0x2018, 0x40B2,
0x201A, 0xB0EC,
0x201C, 0x8470,
0x201E, 0x40B2,
0x2020, 0xB112,
0x2022, 0x84B4,
0x2024, 0x40B2,
0x2026, 0xB14E,
0x2028, 0x84B0,
0x202A, 0x40B2,
0x202C, 0xB17C,
0x202E, 0x84B8,
0x2030, 0x40B2,
0x2032, 0xB1B2,
0x2034, 0x847C,
0x2036, 0x40B2,
0x2038, 0xB420,
0x203A, 0x8478,
0x203C, 0x40B2,
0x203E, 0xB4B4,
0x2040, 0x8476,
0x2042, 0x40B2,
0x2044, 0xB530,
0x2046, 0x847E,
0x2048, 0x40B2,
0x204A, 0xB640,
0x204C, 0x843A,
0x204E, 0x40B2,
0x2050, 0xB822,
0x2052, 0x845C,
0x2054, 0x40B2,
0x2056, 0xB852,
0x2058, 0x845E,
0x205A, 0x4130,
0x205C, 0x1292,
0x205E, 0xD016,
0x2060, 0xB3D2,
0x2062, 0x0B00,
0x2064, 0x2002,
0x2066, 0xD2E2,
0x2068, 0x0381,
0x206A, 0x93C2,
0x206C, 0x0263,
0x206E, 0x2001,
0x2070, 0x4130,
0x2072, 0x422D,
0x2074, 0x403E,
0x2076, 0x879E,
0x2078, 0x403F,
0x207A, 0x192A,
0x207C, 0x1292,
0x207E, 0x843E,
0x2080, 0x3FF7,
0x2082, 0xB3D2,
0x2084, 0x0267,
0x2086, 0x2403,
0x2088, 0xD0F2,
0x208A, 0x0040,
0x208C, 0x0381,
0x208E, 0x90F2,
0x2090, 0x0010,
0x2092, 0x0260,
0x2094, 0x2002,
0x2096, 0x1292,
0x2098, 0x84BC,
0x209A, 0x1292,
0x209C, 0xD020,
0x209E, 0x4130,
0x20A0, 0x1292,
0x20A2, 0x8470,
0x20A4, 0x1292,
0x20A6, 0x8452,
0x20A8, 0x0900,
0x20AA, 0x7118,
0x20AC, 0x1292,
0x20AE, 0x848E,
0x20B0, 0x0900,
0x20B2, 0x7112,
0x20B4, 0x0800,
0x20B6, 0x7A20,
0x20B8, 0x4292,
0x20BA, 0x86EE,
0x20BC, 0x7334,
0x20BE, 0x0F00,
0x20C0, 0x7304,
0x20C2, 0x421F,
0x20C4, 0x8620,
0x20C6, 0x1292,
0x20C8, 0x846E,
0x20CA, 0x1292,
0x20CC, 0x8488,
0x20CE, 0x0B00,
0x20D0, 0x7114,
0x20D2, 0x0002,
0x20D4, 0x1292,
0x20D6, 0x848C,
0x20D8, 0x1292,
0x20DA, 0x8454,
0x20DC, 0x43C2,
0x20DE, 0x85F6,
0x20E0, 0x4292,
0x20E2, 0x0C34,
0x20E4, 0x0202,
0x20E6, 0x1292,
0x20E8, 0x8444,
0x20EA, 0x4130,
0x20EC, 0x4392,
0x20EE, 0x7360,
0x20F0, 0xB3D2,
0x20F2, 0x0B00,
0x20F4, 0x2402,
0x20F6, 0xC2E2,
0x20F8, 0x0381,
0x20FA, 0x0900,
0x20FC, 0x732C,
0x20FE, 0x4382,
0x2100, 0x7360,
0x2102, 0x422D,
0x2104, 0x403E,
0x2106, 0x8700,
0x2108, 0x403F,
0x210A, 0x86F8,
0x210C, 0x1292,
0x210E, 0x843E,
0x2110, 0x4130,
0x2112, 0x4F0C,
0x2114, 0x403F,
0x2116, 0x0267,
0x2118, 0xF0FF,
0x211A, 0xFFDF,
0x211C, 0x0000,
0x211E, 0xF0FF,
0x2120, 0xFFEF,
0x2122, 0x0000,
0x2124, 0x421D,
0x2126, 0x84B0,
0x2128, 0x403E,
0x212A, 0x06F9,
0x212C, 0x4C0F,
0x212E, 0x1292,
0x2130, 0x84AC,
0x2132, 0x4F4E,
0x2134, 0xB31E,
0x2136, 0x2403,
0x2138, 0xD0F2,
0x213A, 0x0020,
0x213C, 0x0267,
0x213E, 0xB32E,
0x2140, 0x2403,
0x2142, 0xD0F2,
0x2144, 0x0010,
0x2146, 0x0267,
0x2148, 0xC3E2,
0x214A, 0x0267,
0x214C, 0x4130,
0x214E, 0x120B,
0x2150, 0x120A,
0x2152, 0x403A,
0x2154, 0x1140,
0x2156, 0x1292,
0x2158, 0xD080,
0x215A, 0x430B,
0x215C, 0x4A0F,
0x215E, 0x532A,
0x2160, 0x1292,
0x2162, 0x84A4,
0x2164, 0x4F0E,
0x2166, 0x430F,
0x2168, 0x5E82,
0x216A, 0x870C,
0x216C, 0x6F82,
0x216E, 0x870E,
0x2170, 0x531B,
0x2172, 0x923B,
0x2174, 0x2BF3,
0x2176, 0x413A,
0x2178, 0x413B,
0x217A, 0x4130,
0x217C, 0xF0F2,
0x217E, 0x007F,
0x2180, 0x0267,
0x2182, 0x421D,
0x2184, 0x84B6,
0x2186, 0x403E,
0x2188, 0x01F9,
0x218A, 0x1292,
0x218C, 0x84AC,
0x218E, 0x4F4E,
0x2190, 0xF35F,
0x2192, 0x2403,
0x2194, 0xD0F2,
0x2196, 0xFF80,
0x2198, 0x0267,
0x219A, 0xB36E,
0x219C, 0x2404,
0x219E, 0xD0F2,
0x21A0, 0x0040,
0x21A2, 0x0267,
0x21A4, 0x3C03,
0x21A6, 0xF0F2,
0x21A8, 0xFFBF,
0x21AA, 0x0267,
0x21AC, 0xC2E2,
0x21AE, 0x0267,
0x21B0, 0x4130,
0x21B2, 0x120B,
0x21B4, 0x120A,
0x21B6, 0x8231,
0x21B8, 0x430B,
0x21BA, 0x93C2,
0x21BC, 0x0C0A,
0x21BE, 0x2404,
0x21C0, 0xB3D2,
0x21C2, 0x0B05,
0x21C4, 0x2401,
0x21C6, 0x431B,
0x21C8, 0x422D,
0x21CA, 0x403E,
0x21CC, 0x192A,
0x21CE, 0x403F,
0x21D0, 0x879E,
0x21D2, 0x1292,
0x21D4, 0x843E,
0x21D6, 0x930B,
0x21D8, 0x20F4,
0x21DA, 0x93E2,
0x21DC, 0x0241,
0x21DE, 0x24EB,
0x21E0, 0x403A,
0x21E2, 0x0292,
0x21E4, 0x4AA2,
0x21E6, 0x0A00,
0x21E8, 0xB2E2,
0x21EA, 0x0361,
0x21EC, 0x2405,
0x21EE, 0x4A2F,
0x21F0, 0x1292,
0x21F2, 0x8474,
0x21F4, 0x4F82,
0x21F6, 0x0A1C,
0x21F8, 0x93C2,
0x21FA, 0x0360,
0x21FC, 0x34CD,
0x21FE, 0x430C,
0x2200, 0x4C0F,
0x2202, 0x5F0F,
0x2204, 0x4F0D,
0x2206, 0x510D,
0x2208, 0x4F0E,
0x220A, 0x5A0E,
0x220C, 0x4E1E,
0x220E, 0x0002,
0x2210, 0x4F1F,
0x2212, 0x192A,
0x2214, 0x1202,
0x2216, 0xC232,
0x2218, 0x4303,
0x221A, 0x4E82,
0x221C, 0x0130,
0x221E, 0x4F82,
0x2220, 0x0138,
0x2222, 0x421E,
0x2224, 0x013A,
0x2226, 0x421F,
0x2228, 0x013C,
0x222A, 0x4132,
0x222C, 0x108E,
0x222E, 0x108F,
0x2230, 0xEF4E,
0x2232, 0xEF0E,
0x2234, 0xF37F,
0x2236, 0xC312,
0x2238, 0x100F,
0x223A, 0x100E,
0x223C, 0x4E8D,
0x223E, 0x0000,
0x2240, 0x531C,
0x2242, 0x922C,
0x2244, 0x2BDD,
0x2246, 0xB3D2,
0x2248, 0x1921,
0x224A, 0x2403,
0x224C, 0x410F,
0x224E, 0x1292,
0x2250, 0x847E,
0x2252, 0x403B,
0x2254, 0x843E,
0x2256, 0x422D,
0x2258, 0x410E,
0x225A, 0x403F,
0x225C, 0x1908,
0x225E, 0x12AB,
0x2260, 0x403D,
0x2262, 0x0005,
0x2264, 0x403E,
0x2266, 0x0292,
0x2268, 0x403F,
0x226A, 0x85EC,
0x226C, 0x12AB,
0x226E, 0x421F,
0x2270, 0x060E,
0x2272, 0x9F82,
0x2274, 0x8628,
0x2276, 0x288D,
0x2278, 0x9382,
0x227A, 0x060E,
0x227C, 0x248A,
0x227E, 0x90BA,
0x2280, 0x0010,
0x2282, 0x0000,
0x2284, 0x2C0B,
0x2286, 0x93C2,
0x2288, 0x85F6,
0x228A, 0x2008,
0x228C, 0x403F,
0x228E, 0x06A7,
0x2290, 0xD0FF,
0x2292, 0x0007,
0x2294, 0x0000,
0x2296, 0xF0FF,
0x2298, 0xFFF8,
0x229A, 0x0000,
0x229C, 0x4392,
0x229E, 0x8628,
0x22A0, 0x403F,
0x22A2, 0x06A7,
0x22A4, 0xD2EF,
0x22A6, 0x0000,
0x22A8, 0xC2EF,
0x22AA, 0x0000,
0x22AC, 0x93C2,
0x22AE, 0x86E3,
0x22B0, 0x2068,
0x22B2, 0xB0F2,
0x22B4, 0x0040,
0x22B6, 0x0B05,
0x22B8, 0x2461,
0x22BA, 0xD3D2,
0x22BC, 0x0410,
0x22BE, 0xB3E2,
0x22C0, 0x0381,
0x22C2, 0x2089,
0x22C4, 0x90B2,
0x22C6, 0x0030,
0x22C8, 0x0A00,
0x22CA, 0x2C52,
0x22CC, 0x93C2,
0x22CE, 0x85F6,
0x22D0, 0x204F,
0x22D2, 0x430E,
0x22D4, 0x430C,
0x22D6, 0x4C0F,
0x22D8, 0x5F0F,
0x22DA, 0x5F0F,
0x22DC, 0x5F0F,
0x22DE, 0x4F1F,
0x22E0, 0x8570,
0x22E2, 0xF03F,
0x22E4, 0x07FF,
0x22E6, 0x903F,
0x22E8, 0x0400,
0x22EA, 0x343E,
0x22EC, 0x5F0E,
0x22EE, 0x531C,
0x22F0, 0x923C,
0x22F2, 0x2BF1,
0x22F4, 0x4E0F,
0x22F6, 0x930E,
0x22F8, 0x3834,
0x22FA, 0x110F,
0x22FC, 0x110F,
0x22FE, 0x110F,
0x2300, 0x9382,
0x2302, 0x85F6,
0x2304, 0x2023,
0x2306, 0x5F82,
0x2308, 0x86E6,
0x230A, 0x403B,
0x230C, 0x86E6,
0x230E, 0x4B2F,
0x2310, 0x12B0,
0x2312, 0xB3EC,
0x2314, 0x4F8B,
0x2316, 0x0000,
0x2318, 0x430C,
0x231A, 0x4C0D,
0x231C, 0x5D0D,
0x231E, 0x5D0D,
0x2320, 0x5D0D,
0x2322, 0x403A,
0x2324, 0x86E8,
0x2326, 0x421B,
0x2328, 0x86E6,
0x232A, 0x4B0F,
0x232C, 0x8A2F,
0x232E, 0x4F0E,
0x2330, 0x4E0F,
0x2332, 0x5F0F,
0x2334, 0x7F0F,
0x2336, 0xE33F,
0x2338, 0x8E8D,
0x233A, 0x8570,
0x233C, 0x7F8D,
0x233E, 0x8572,
0x2340, 0x531C,
0x2342, 0x923C,
0x2344, 0x2BEA,
0x2346, 0x4B8A,
0x2348, 0x0000,
0x234A, 0x3C45,
0x234C, 0x9382,
0x234E, 0x85F8,
0x2350, 0x2005,
0x2352, 0x4382,
0x2354, 0x86E6,
0x2356, 0x4382,
0x2358, 0x86E8,
0x235A, 0x3FD7,
0x235C, 0x4F82,
0x235E, 0x86E6,
0x2360, 0x3FD4,
0x2362, 0x503F,
0x2364, 0x0007,
0x2366, 0x3FC9,
0x2368, 0x5F0E,
0x236A, 0x503E,
0x236C, 0xF800,
0x236E, 0x3FBF,
0x2370, 0x430F,
0x2372, 0x12B0,
0x2374, 0xB3EC,
0x2376, 0x4382,
0x2378, 0x86E6,
0x237A, 0x3C2D,
0x237C, 0xC3D2,
0x237E, 0x0410,
0x2380, 0x3F9E,
0x2382, 0x430D,
0x2384, 0x403E,
0x2386, 0x0050,
0x2388, 0x403F,
0x238A, 0x84D0,
0x238C, 0x1292,
0x238E, 0x844E,
0x2390, 0x3F90,
0x2392, 0x5392,
0x2394, 0x8628,
0x2396, 0x3F84,
0x2398, 0x403B,
0x239A, 0x843E,
0x239C, 0x4A0F,
0x239E, 0x532F,
0x23A0, 0x422D,
0x23A2, 0x4F0E,
0x23A4, 0x403F,
0x23A6, 0x0E08,
0x23A8, 0x12AB,
0x23AA, 0x422D,
0x23AC, 0x403E,
0x23AE, 0x192A,
0x23B0, 0x410F,
0x23B2, 0x12AB,
0x23B4, 0x3F48,
0x23B6, 0x93C2,
0x23B8, 0x85F6,
0x23BA, 0x2312,
0x23BC, 0x403A,
0x23BE, 0x85EC,
0x23C0, 0x3F11,
0x23C2, 0x403D,
0x23C4, 0x0200,
0x23C6, 0x422E,
0x23C8, 0x403F,
0x23CA, 0x192A,
0x23CC, 0x1292,
0x23CE, 0x844E,
0x23D0, 0xC3D2,
0x23D2, 0x1921,
0x23D4, 0x3F02,
0x23D6, 0x422D,
0x23D8, 0x403E,
0x23DA, 0x879E,
0x23DC, 0x403F,
0x23DE, 0x192A,
0x23E0, 0x1292,
0x23E2, 0x843E,
0x23E4, 0x5231,
0x23E6, 0x413A,
0x23E8, 0x413B,
0x23EA, 0x4130,
0x23EC, 0x4382,
0x23EE, 0x052C,
0x23F0, 0x4F0D,
0x23F2, 0x930D,
0x23F4, 0x3402,
0x23F6, 0xE33D,
0x23F8, 0x531D,
0x23FA, 0xF03D,
0x23FC, 0x07F0,
0x23FE, 0x4D0E,
0x2400, 0xC312,
0x2402, 0x100E,
0x2404, 0x110E,
0x2406, 0x110E,
0x2408, 0x110E,
0x240A, 0x930F,
0x240C, 0x3803,
0x240E, 0x4EC2,
0x2410, 0x052C,
0x2412, 0x3C04,
0x2414, 0x4EC2,
0x2416, 0x052D,
0x2418, 0xE33D,
0x241A, 0x531D,
0x241C, 0x4D0F,
0x241E, 0x4130,
0x2420, 0x120B,
0x2422, 0x120A,
0x2424, 0x93C2,
0x2426, 0x85F6,
0x2428, 0x2003,
0x242A, 0xB3D2,
0x242C, 0x0360,
0x242E, 0x2402,
0x2430, 0x1292,
0x2432, 0x847A,
0x2434, 0x1292,
0x2436, 0x847C,
0x2438, 0x93C2,
0x243A, 0x0600,
0x243C, 0x3803,
0x243E, 0x93C2,
0x2440, 0x0604,
0x2442, 0x3832,
0x2444, 0xD2F2,
0x2446, 0x0F01,
0x2448, 0xB3D2,
0x244A, 0x0363,
0x244C, 0x2418,
0x244E, 0x421F,
0x2450, 0x1246,
0x2452, 0x4F0E,
0x2454, 0x430F,
0x2456, 0x421B,
0x2458, 0x1244,
0x245A, 0x430A,
0x245C, 0xDA0E,
0x245E, 0xDB0F,
0x2460, 0x821E,
0x2462, 0x86F4,
0x2464, 0x721F,
0x2466, 0x86F6,
0x2468, 0x2C1B,
0x246A, 0x421F,
0x246C, 0x1240,
0x246E, 0xF03F,
0x2470, 0x01FF,
0x2472, 0x9F82,
0x2474, 0x0A00,
0x2476, 0x2814,
0x2478, 0xD0F2,
0x247A, 0xFF80,
0x247C, 0x1240,
0x247E, 0x93C2,
0x2480, 0x85F6,
0x2482, 0x2015,
0x2484, 0xB0F2,
0x2486, 0x0020,
0x2488, 0x0381,
0x248A, 0x2407,
0x248C, 0x9292,
0x248E, 0x862A,
0x2490, 0x0384,
0x2492, 0x2C03,
0x2494, 0xD3D2,
0x2496, 0x0649,
0x2498, 0x3C0A,
0x249A, 0xC3D2,
0x249C, 0x0649,
0x249E, 0x3C07,
0x24A0, 0xF0F2,
0x24A2, 0x007F,
0x24A4, 0x1240,
0x24A6, 0x3FEB,
0x24A8, 0xC2F2,
0x24AA, 0x0F01,
0x24AC, 0x3FCD,
0x24AE, 0x413A,
0x24B0, 0x413B,
0x24B2, 0x4130,
0x24B4, 0x425F,
0x24B6, 0x86E2,
0x24B8, 0xD25F,
0x24BA, 0x86E1,
0x24BC, 0x4F4E,
0x24BE, 0x5E0E,
0x24C0, 0x425F,
0x24C2, 0x0204,
0x24C4, 0xF07F,
0x24C6, 0x0003,
0x24C8, 0xF37F,
0x24CA, 0xDF0E,
0x24CC, 0x40B2,
0x24CE, 0x8030,
0x24D0, 0x7A00,
0x24D2, 0x40B2,
0x24D4, 0x0100,
0x24D6, 0x7A02,
0x24D8, 0x40B2,
0x24DA, 0x0D04,
0x24DC, 0x7A0C,
0x24DE, 0x40B2,
0x24E0, 0xFFF0,
0x24E2, 0x7A04,
0x24E4, 0x93C2,
0x24E6, 0x86E0,
0x24E8, 0x240A,
0x24EA, 0x40B2,
0x24EC, 0xFFF1,
0x24EE, 0x7A06,
0x24F0, 0x40B2,
0x24F2, 0xFFF4,
0x24F4, 0x7A08,
0x24F6, 0x40B2,
0x24F8, 0xFFF5,
0x24FA, 0x7A0A,
0x24FC, 0x3C09,
0x24FE, 0x40B2,
0x2500, 0xFFF2,
0x2502, 0x7A06,
0x2504, 0x40B2,
0x2506, 0xFFF4,
0x2508, 0x7A08,
0x250A, 0x40B2,
0x250C, 0xFFF6,
0x250E, 0x7A0A,
0x2510, 0xF03E,
0x2512, 0x0003,
0x2514, 0x5E0E,
0x2516, 0x425F,
0x2518, 0x86E2,
0x251A, 0xD25F,
0x251C, 0x86E1,
0x251E, 0xF31F,
0x2520, 0x5F0F,
0x2522, 0x5F0F,
0x2524, 0x5F0F,
0x2526, 0xD31E,
0x2528, 0xDF0E,
0x252A, 0x4E82,
0x252C, 0x7A12,
0x252E, 0x4130,
0x2530, 0x120B,
0x2532, 0x120A,
0x2534, 0x1209,
0x2536, 0x1208,
0x2538, 0x1207,
0x253A, 0x1206,
0x253C, 0x1205,
0x253E, 0x1204,
0x2540, 0x8231,
0x2542, 0x4F81,
0x2544, 0x0000,
0x2546, 0x4381,
0x2548, 0x0002,
0x254A, 0x4304,
0x254C, 0x411C,
0x254E, 0x0002,
0x2550, 0x5C0C,
0x2552, 0x4C0F,
0x2554, 0x5F0F,
0x2556, 0x5F0F,
0x2558, 0x5F0F,
0x255A, 0x5F0F,
0x255C, 0x5F0F,
0x255E, 0x503F,
0x2560, 0x1980,
0x2562, 0x440D,
0x2564, 0x5D0D,
0x2566, 0x4D0E,
0x2568, 0x5F0E,
0x256A, 0x4E2E,
0x256C, 0x4D05,
0x256E, 0x5505,
0x2570, 0x5F05,
0x2572, 0x4516,
0x2574, 0x0008,
0x2576, 0x4517,
0x2578, 0x000A,
0x257A, 0x460A,
0x257C, 0x470B,
0x257E, 0xF30A,
0x2580, 0xF32B,
0x2582, 0x4A81,
0x2584, 0x0004,
0x2586, 0x4B81,
0x2588, 0x0006,
0x258A, 0xB03E,
0x258C, 0x2000,
0x258E, 0x2404,
0x2590, 0xF03E,
0x2592, 0x1FFF,
0x2594, 0xE33E,
0x2596, 0x531E,
0x2598, 0xF317,
0x259A, 0x503E,
0x259C, 0x2000,
0x259E, 0x4E0F,
0x25A0, 0x5F0F,
0x25A2, 0x7F0F,
0x25A4, 0xE33F,
0x25A6, 0x512C,
0x25A8, 0x4C28,
0x25AA, 0x4309,
0x25AC, 0x4E0A,
0x25AE, 0x4F0B,
0x25B0, 0x480C,
0x25B2, 0x490D,
0x25B4, 0x1202,
0x25B6, 0xC232,
0x25B8, 0x12B0,
0x25BA, 0xFFC0,
0x25BC, 0x4132,
0x25BE, 0x108E,
0x25C0, 0x108F,
0x25C2, 0xEF4E,
0x25C4, 0xEF0E,
0x25C6, 0xF37F,
0x25C8, 0xC312,
0x25CA, 0x100F,
0x25CC, 0x100E,
0x25CE, 0x4E85,
0x25D0, 0x0018,
0x25D2, 0x4F85,
0x25D4, 0x001A,
0x25D6, 0x480A,
0x25D8, 0x490B,
0x25DA, 0x460C,
0x25DC, 0x470D,
0x25DE, 0x1202,
0x25E0, 0xC232,
0x25E2, 0x12B0,
0x25E4, 0xFFC0,
0x25E6, 0x4132,
0x25E8, 0x4E0C,
0x25EA, 0x4F0D,
0x25EC, 0x108C,
0x25EE, 0x108D,
0x25F0, 0xED4C,
0x25F2, 0xED0C,
0x25F4, 0xF37D,
0x25F6, 0xC312,
0x25F8, 0x100D,
0x25FA, 0x100C,
0x25FC, 0x411E,
0x25FE, 0x0004,
0x2600, 0x411F,
0x2602, 0x0006,
0x2604, 0x5E0E,
0x2606, 0x6F0F,
0x2608, 0x5E0E,
0x260A, 0x6F0F,
0x260C, 0x5E0E,
0x260E, 0x6F0F,
0x2610, 0xDE0C,
0x2612, 0xDF0D,
0x2614, 0x4C85,
0x2616, 0x002C,
0x2618, 0x4D85,
0x261A, 0x002E,
0x261C, 0x5314,
0x261E, 0x9224,
0x2620, 0x2B95,
0x2622, 0x5391,
0x2624, 0x0002,
0x2626, 0x92A1,
0x2628, 0x0002,
0x262A, 0x2B8F,
0x262C, 0x5231,
0x262E, 0x4134,
0x2630, 0x4135,
0x2632, 0x4136,
0x2634, 0x4137,
0x2636, 0x4138,
0x2638, 0x4139,
0x263A, 0x413A,
0x263C, 0x413B,
0x263E, 0x4130,
0x2640, 0x120B,
0x2642, 0x120A,
0x2644, 0x1209,
0x2646, 0x8031,
0x2648, 0x000C,
0x264A, 0x425F,
0x264C, 0x0205,
0x264E, 0xC312,
0x2650, 0x104F,
0x2652, 0x114F,
0x2654, 0x114F,
0x2656, 0x114F,
0x2658, 0x114F,
0x265A, 0x114F,
0x265C, 0xF37F,
0x265E, 0x4F0B,
0x2660, 0xF31B,
0x2662, 0x5B0B,
0x2664, 0x5B0B,
0x2666, 0x5B0B,
0x2668, 0x503B,
0x266A, 0xD196,
0x266C, 0x4219,
0x266E, 0x0508,
0x2670, 0xF039,
0x2672, 0x2000,
0x2674, 0x4F0A,
0x2676, 0xC312,
0x2678, 0x100A,
0x267A, 0xE31A,
0x267C, 0x421F,
0x267E, 0x86EE,
0x2680, 0x503F,
0x2682, 0xFF60,
0x2684, 0x903F,
0x2686, 0x00C8,
0x2688, 0x2C02,
0x268A, 0x403F,
0x268C, 0x00C8,
0x268E, 0x4F82,
0x2690, 0x7322,
0x2692, 0xB3D2,
0x2694, 0x0381,
0x2696, 0x2009,
0x2698, 0x421F,
0x269A, 0x85F8,
0x269C, 0xD21F,
0x269E, 0x85F6,
0x26A0, 0x930F,
0x26A2, 0x24B1,
0x26A4, 0x40F2,
0x26A6, 0xFF80,
0x26A8, 0x0619,
0x26AA, 0x1292,
0x26AC, 0xD00A,
0x26AE, 0x430D,
0x26B0, 0x93C2,
0x26B2, 0x86E0,
0x26B4, 0x2003,
0x26B6, 0xB2F2,
0x26B8, 0x0360,
0x26BA, 0x2001,
0x26BC, 0x431D,
0x26BE, 0x425F,
0x26C0, 0x86E3,
0x26C2, 0xD25F,
0x26C4, 0x86E2,
0x26C6, 0xF37F,
0x26C8, 0x5F0F,
0x26CA, 0x425E,
0x26CC, 0x86DD,
0x26CE, 0xDE0F,
0x26D0, 0x5F0F,
0x26D2, 0x5B0F,
0x26D4, 0x4FA2,
0x26D6, 0x0402,
0x26D8, 0x930D,
0x26DA, 0x2007,
0x26DC, 0x930A,
0x26DE, 0x248E,
0x26E0, 0x4F5F,
0x26E2, 0x0001,
0x26E4, 0xF37F,
0x26E6, 0x4FC2,
0x26E8, 0x0403,
0x26EA, 0x93C2,
0x26EC, 0x86DD,
0x26EE, 0x2483,
0x26F0, 0xC2F2,
0x26F2, 0x0400,
0x26F4, 0xB2E2,
0x26F6, 0x0265,
0x26F8, 0x2407,
0x26FA, 0x421F,
0x26FC, 0x0508,
0x26FE, 0xF03F,
0x2700, 0xFFDF,
0x2702, 0xD90F,
0x2704, 0x4F82,
0x2706, 0x0508,
0x2708, 0xB3D2,
0x270A, 0x0383,
0x270C, 0x2484,
0x270E, 0x403F,
0x2710, 0x0508,
0x2712, 0x4FB1,
0x2714, 0x0000,
0x2716, 0x4FB1,
0x2718, 0x0002,
0x271A, 0x4FB1,
0x271C, 0x0004,
0x271E, 0x403F,
0x2720, 0x0500,
0x2722, 0x4FB1,
0x2724, 0x0006,
0x2726, 0x4FB1,
0x2728, 0x0008,
0x272A, 0x4FB1,
0x272C, 0x000A,
0x272E, 0xB3E2,
0x2730, 0x0383,
0x2732, 0x2412,
0x2734, 0xC2E1,
0x2736, 0x0002,
0x2738, 0xB2E2,
0x273A, 0x0383,
0x273C, 0x434F,
0x273E, 0x634F,
0x2740, 0xF37F,
0x2742, 0x4F4E,
0x2744, 0x114E,
0x2746, 0x434E,
0x2748, 0x104E,
0x274A, 0x415F,
0x274C, 0x0007,
0x274E, 0xF07F,
0x2750, 0x007F,
0x2752, 0xDE4F,
0x2754, 0x4FC1,
0x2756, 0x0007,
0x2758, 0xB2F2,
0x275A, 0x0383,
0x275C, 0x2415,
0x275E, 0xF0F1,
0x2760, 0xFFBF,
0x2762, 0x0000,
0x2764, 0xB0F2,
0x2766, 0x0010,
0x2768, 0x0383,
0x276A, 0x434E,
0x276C, 0x634E,
0x276E, 0x5E4E,
0x2770, 0x5E4E,
0x2772, 0x5E4E,
0x2774, 0x5E4E,
0x2776, 0x5E4E,
0x2778, 0x5E4E,
0x277A, 0x415F,
0x277C, 0x0006,
0x277E, 0xF07F,
0x2780, 0xFFBF,
0x2782, 0xDE4F,
0x2784, 0x4FC1,
0x2786, 0x0006,
0x2788, 0xB0F2,
0x278A, 0x0020,
0x278C, 0x0383,
0x278E, 0x2410,
0x2790, 0xF0F1,
0x2792, 0xFFDF,
0x2794, 0x0002,
0x2796, 0xB0F2,
0x2798, 0x0040,
0x279A, 0x0383,
0x279C, 0x434E,
0x279E, 0x634E,
0x27A0, 0x5E4E,
0x27A2, 0x5E4E,
0x27A4, 0x415F,
0x27A6, 0x0008,
0x27A8, 0xC26F,
0x27AA, 0xDE4F,
0x27AC, 0x4FC1,
0x27AE, 0x0008,
0x27B0, 0x93C2,
0x27B2, 0x0383,
0x27B4, 0x3412,
0x27B6, 0xF0F1,
0x27B8, 0xFFDF,
0x27BA, 0x0000,
0x27BC, 0x425E,
0x27BE, 0x0382,
0x27C0, 0xF35E,
0x27C2, 0x5E4E,
0x27C4, 0x5E4E,
0x27C6, 0x5E4E,
0x27C8, 0x5E4E,
0x27CA, 0x5E4E,
0x27CC, 0x415F,
0x27CE, 0x0006,
0x27D0, 0xF07F,
0x27D2, 0xFFDF,
0x27D4, 0xDE4F,
0x27D6, 0x4FC1,
0x27D8, 0x0006,
0x27DA, 0x410F,
0x27DC, 0x4FB2,
0x27DE, 0x0508,
0x27E0, 0x4FB2,
0x27E2, 0x050A,
0x27E4, 0x4FB2,
0x27E6, 0x050C,
0x27E8, 0x4FB2,
0x27EA, 0x0500,
0x27EC, 0x4FB2,
0x27EE, 0x0502,
0x27F0, 0x4FB2,
0x27F2, 0x0504,
0x27F4, 0x3C10,
0x27F6, 0xD2F2,
0x27F8, 0x0400,
0x27FA, 0x3F7C,
0x27FC, 0x4F6F,
0x27FE, 0xF37F,
0x2800, 0x4FC2,
0x2802, 0x0402,
0x2804, 0x3F72,
0x2806, 0x90F2,
0x2808, 0x0011,
0x280A, 0x0619,
0x280C, 0x2B4E,
0x280E, 0x50F2,
0x2810, 0xFFF0,
0x2812, 0x0619,
0x2814, 0x3F4A,
0x2816, 0x5031,
0x2818, 0x000C,
0x281A, 0x4139,
0x281C, 0x413A,
0x281E, 0x413B,
0x2820, 0x4130,
0x2822, 0x0900,
0x2824, 0x7312,
0x2826, 0x421F,
0x2828, 0x0A08,
0x282A, 0xF03F,
0x282C, 0xF7FF,
0x282E, 0x4F82,
0x2830, 0x0A88,
0x2832, 0x0900,
0x2834, 0x7312,
0x2836, 0x421F,
0x2838, 0x0A0E,
0x283A, 0xF03F,
0x283C, 0x7FFF,
0x283E, 0x4F82,
0x2840, 0x0A8E,
0x2842, 0x0900,
0x2844, 0x7312,
0x2846, 0x421F,
0x2848, 0x0A1E,
0x284A, 0xC31F,
0x284C, 0x4F82,
0x284E, 0x0A9E,
0x2850, 0x4130,
0x2852, 0x4292,
0x2854, 0x0A08,
0x2856, 0x0A88,
0x2858, 0x0900,
0x285A, 0x7312,
0x285C, 0x4292,
0x285E, 0x0A0E,
0x2860, 0x0A8E,
0x2862, 0x0900,
0x2864, 0x7312,
0x2866, 0x4292,
0x2868, 0x0A1E,
0x286A, 0x0A9E,
0x286C, 0x4130,
0x286E, 0x7400,
0x2870, 0x8058,
0x2872, 0x1807,
0x2874, 0x00E0,
0x2876, 0x7002,
0x2878, 0x17C7,
0x287A, 0x0045,
0x287C, 0x0006,
0x287E, 0x17CC,
0x2880, 0x0015,
0x2882, 0x1512,
0x2884, 0x216F,
0x2886, 0x005B,
0x2888, 0x005D,
0x288A, 0x00DE,
0x288C, 0x00DD,
0x288E, 0x5023,
0x2890, 0x00DE,
0x2892, 0x005B,
0x2894, 0x0410,
0x2896, 0x0091,
0x2898, 0x0015,
0x289A, 0x0040,
0x289C, 0x7023,
0x289E, 0x1653,
0x28A0, 0x0156,
0x28A2, 0x0001,
0x28A4, 0x2081,
0x28A6, 0x700E,
0x28A8, 0x2F99,
0x28AA, 0x005C,
0x28AC, 0x0000,
0x28AE, 0x5040,
0x28B0, 0x0045,
0x28B2, 0x213A,
0x28B4, 0x0303,
0x28B6, 0x0148,
0x28B8, 0x0049,
0x28BA, 0x0045,
0x28BC, 0x0046,
0x28BE, 0x081D,
0x28C0, 0x00DE,
0x28C2, 0x00DD,
0x28C4, 0x00DC,
0x28C6, 0x00DE,
0x28C8, 0x04D6,
0x28CA, 0x2014,
0x28CC, 0x2081,
0x28CE, 0x704E,
0x28D0, 0x2F99,
0x28D2, 0x005C,
0x28D4, 0x0002,
0x28D6, 0x5060,
0x28D8, 0x31C0,
0x28DA, 0x2122,
0x28DC, 0x7800,
0x28DE, 0xC08C,
0x28E0, 0x0001,
0x28E2, 0x9038,
0x28E4, 0x59F7,
0x28E6, 0x907A,
0x28E8, 0x03D8,
0x28EA, 0x8D90,
0x28EC, 0x01C0,
0x28EE, 0x7400,
0x28F0, 0x2002,
0x28F2, 0x70DF,
0x28F4, 0x3F40,
0x28F6, 0x0240,
0x28F8, 0x7800,
0x28FA, 0x0021,
0x28FC, 0x7400,
0x28FE, 0x0001,
0x2900, 0x70DF,
0x2902, 0x3F5F,
0x2904, 0x7012,
0x2906, 0x2F01,
0x2908, 0x7800,
0x290A, 0x7400,
0x290C, 0x2004,
0x290E, 0x70DF,
0x2910, 0x3F20,
0x2912, 0x0240,
0x2914, 0x7800,
0x2916, 0x0041,
0x2918, 0x7400,
0x291A, 0x2008,
0x291C, 0x70DF,
0x291E, 0x3F20,
0x2920, 0x0240,
0x2922, 0x7800,
0x2924, 0x0041,
0x2926, 0x7400,
0x2928, 0x0004,
0x292A, 0x70DF,
0x292C, 0x3F5F,
0x292E, 0x7012,
0x2930, 0x2F01,
0x2932, 0x7800,
0x2934, 0x7400,
0x2936, 0x2010,
0x2938, 0x70DF,
0x293A, 0x3F40,
0x293C, 0x0240,
0x293E, 0x7800,
0x2940, 0x0000,
0x2942, 0xB86E,
0x2944, 0x0000,
0x2946, 0xB86E,
0x2948, 0xB8DE,
0x294A, 0x0002,
0x294C, 0x0063,
0x294E, 0xB90A,
0x2950, 0x0063,
0x2952, 0xB8EE,
0x2954, 0x0063,
0x2956, 0xB918,
0x2958, 0x0063,
0x295A, 0xB926,
0x295C, 0xB8FA,
0x295E, 0x0004,
0x2960, 0x0063,
0x2962, 0xB918,
0x2964, 0x0063,
0x2966, 0xB934,
0x2968, 0x0063,
0x296A, 0xB90A,
0x296C, 0x0063,
0x296E, 0xB8FC,
0x2970, 0xB8FA,
0x2972, 0x0004,
0x2974, 0x0066,
0x2976, 0x0067,
0x2978, 0x00AF,
0x297A, 0x01CF,
0x297C, 0x0087,
0x297E, 0x0083,
0x2980, 0x011B,
0x2982, 0x035A,
0x2984, 0x00FA,
0x2986, 0x00F2,
0x2988, 0x00A6,
0x298A, 0x00A4,
0x298C, 0xFFFF,
0x298E, 0x002D,
0x2990, 0x005A,
0x2992, 0x0000,
0x2994, 0x0000,
0x2996, 0xB974,
0x2998, 0xB940,
0x299A, 0xB98E,
0x299C, 0xB94C,
0x299E, 0xB960,
0x29A0, 0xB94C,
0x29A2, 0xB960,
0x29A4, 0xB94C,
0x29A6, 0xB960,
0x29A8, 0xB94C,
0x29AA, 0xB960,
0x29AC, 0xB94C,
0x29AE, 0xB960,
0x29B0, 0xB94C,
0x29B2, 0xB960,
0x29B4, 0xB94C,
0x29B6, 0xB960,
0x29B8, 0xB94C,
0x29BA, 0xB960,
0x29BC, 0xB94C,
0x29BE, 0xB960,
0x29C0, 0xB94C,
0x29C2, 0xB960,
0x29C4, 0xB94C,
0x29C6, 0xB960,
0x29C8, 0xB94C,
0x29CA, 0xB960,
0x29CC, 0xB94C,
0x29CE, 0xB960,
0x29D0, 0xB94C,
0x29D2, 0xB960,
0x29D4, 0xB94C,
0x29D6, 0xB960,
0x29D8, 0xB94C,
0x29DA, 0xB960,
0x3710, 0x871E,
0x3712, 0xB9BC,
0x3714, 0xB99A,
0x3716, 0xD140,
0x3718, 0xB99C,
0x371A, 0xB998,
0x371C, 0x0000,
0x371E, 0x0040,
0x3720, 0x003D,
0x3722, 0x003E,
0x3724, 0x0040,
0x3726, 0x0044,
0x3728, 0x0049,
0x372A, 0x004D,
0x372C, 0x0052,
0x372E, 0x0057,
0x3730, 0x005C,
0x3732, 0x0062,
0x3734, 0x0068,
0x3736, 0x006E,
0x3738, 0x0074,
0x373A, 0x007A,
0x373C, 0x0080,
0x373E, 0x0087,
0x3740, 0x008E,
0x3742, 0x0095,
0x3744, 0x009C,
0x3746, 0x00A4,
0x3748, 0x00AB,
0x374A, 0x00B2,
0x374C, 0x00BA,
0x374E, 0x00C1,
0x3750, 0x00C7,
0x3752, 0x00CD,
0x3754, 0x00D4,
0x3756, 0x00DA,
0x3758, 0x00E0,
0x375A, 0x00E6,
0x375C, 0x00E6,
0x375E, 0x0000,
0x3760, 0x0000,
0x3762, 0x0000,
0x3764, 0x0000,
0x3766, 0x0000,
0x3768, 0x0000,
0x376A, 0x0000,
0x376C, 0x0000,
0x376E, 0x0000,
0x3770, 0x0000,
0x3772, 0x0000,
0x3774, 0x0000,
0x3776, 0x0000,
0x3778, 0x0000,
0x377A, 0x0000,
0x377C, 0x0000,
0x377E, 0x0000,
0x3780, 0x0000,
0x3782, 0x0000,
0x3784, 0x0000,
0x3786, 0x0000,
0x3788, 0x0000,
0x378A, 0x0000,
0x378C, 0x0000,
0x378E, 0x0000,
0x3790, 0x0000,
0x3792, 0x0000,
0x3794, 0x0000,
0x3796, 0x0000,
0x3798, 0x0000,
0x379A, 0x0000,
0x379C, 0x0000,
0x0268, 0x00EB,
0x026A, 0xFFFF,
0x026C, 0x00FF,
0x026E, 0x0000,
0x0360, 0x1E8E,
0x040E, 0x01EB,
0x0600, 0x1130,
0x0602, 0x3112,
0x0604, 0x8048,
0x0606, 0x00E9,
0x0676, 0x07FF,
0x067A, 0x0505,
0x067C, 0x0505,
0x06A8, 0x0240,
0x06AA, 0x00CA,
0x06AC, 0x0041,
0x06B4, 0x3FFF,
0x06DE, 0x0505,
0x06E0, 0x0505,
0x06E2, 0xFF00,
0x06E4, 0x8369,
0x06E6, 0x8369,
0x06E8, 0x8369,
0x06EA, 0x8369,
0x052A, 0x0000,
0x052C, 0x0000,
0x0F06, 0x0002,
0x1102, 0x0008,
0x0A04, 0xB4C5,
0x0A06, 0xC400,
0x0A08, 0x988A,
0x0A0A, 0xF386,
0x0A0E, 0xEEC0,
0x0A12, 0x0000,
0x0A18, 0x0010,
0x0A1E, 0x000F,
0x0A20, 0x0015,
0x0C00, 0x0021,
0x0C16, 0x0002,
0x0708, 0x6FC0,
0x070C, 0x0000,
0x0780, 0x010F,
0x1244, 0x0000,
0x1246, 0x012C,
0x105C, 0x0F0B,
0x1958, 0x003F,
0x195A, 0x004C,
0x195C, 0x0097,
0x195E, 0x0221,
0x1960, 0x03FF,
0x1980, 0x007D,
0x1982, 0x0028,
0x1984, 0x2018,
0x1986, 0x0010,
0x1988, 0x0000,
0x198A, 0x0000,
0x198C, 0x0428,
0x198E, 0x0000,
0x1990, 0x1B33,
0x1992, 0x0000,
0x1994, 0x3000,
0x1996, 0x0002,
0x1962, 0x003F,
0x1964, 0x004C,
0x1966, 0x0097,
0x1968, 0x0221,
0x196A, 0x03FF,
0x19C0, 0x007D,
0x19C2, 0x0028,
0x19C4, 0x2018,
0x19C6, 0x0010,
0x19C8, 0x0000,
0x19CA, 0x0000,
0x19CC, 0x0428,
0x19CE, 0x0000,
0x19D0, 0x1B33,
0x19D2, 0x0000,
0x19D4, 0x3000,
0x19D6, 0x0002,
0x196C, 0x003F,
0x196E, 0x004C,
0x1970, 0x0097,
0x1972, 0x0221,
0x1974, 0x03FF,
0x1A00, 0x007D,
0x1A02, 0x0028,
0x1A04, 0x2018,
0x1A06, 0x0010,
0x1A08, 0x0000,
0x1A0A, 0x0000,
0x1A0C, 0x0428,
0x1A0E, 0x0000,
0x1A10, 0x1B33,
0x1A12, 0x0000,
0x1A14, 0x3000,
0x1A16, 0x0002,
0x1976, 0x003F,
0x1978, 0x004C,
0x197A, 0x0097,
0x197C, 0x0221,
0x197E, 0x03FF,
0x1A40, 0x007D,
0x1A42, 0x0028,
0x1A44, 0x2018,
0x1A46, 0x0010,
0x1A48, 0x0000,
0x1A4A, 0x0000,
0x1A4C, 0x0428,
0x1A4E, 0x0000,
0x1A50, 0x1B33,
0x1A52, 0x0000,
0x1A54, 0x3000,
0x1A56, 0x0002,
0x027E, 0x0100,



};
#endif


static void sensor_init(void)
{
#if MULTI_WRITE
	hi1337_table_write_cmos_sensor(
		addr_data_pair_init_hi1337,
		sizeof(addr_data_pair_init_hi1337) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_preview_hi1337[] = {

/*
DISP_WIDTH = 2104
DISP_HEIGHT = 1560
DISP_NOTE = "BIN2"
MIPI_SPEED = 720.00
MIPI_LANE = 4
DISP_DATAORDER = GR
*/
//Sensor Information////////////////////////////
//Sensor            : Hi-1337
//Date              : 2020-03-30
//Customer          : Longcheer M505
//Image size        : 2104x1560
//MCLK              : 24MHz
//MIPI speed(Mbps)  : 720Mbps x 4Lane
//Pixel order       : GR
//Frame rate        : 30.10fps
//PDAF              : 3 / PD-DPC on, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0200,
0x0206, 0x02D0,
0x020A, 0x0CF6,
0x020E, 0x0CFA,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002C,
0x022A, 0x0015,
0x022C, 0x0E2D,
0x022E, 0x0C61,
0x0234, 0x3311,
0x0236, 0x3311,
0x0238, 0x3311,
0x023A, 0x2222,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0400,
0x0F04, 0x0004,
0x0B02, 0x0100,
0x0B04, 0x00FC,
0x0B12, 0x0838,
0x0B14, 0x0618,
0x0B20, 0x0200,
0x1100, 0x1100,
0x1108, 0x0402,
0x1118, 0x0000,
0x0A10, 0xB070,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0618,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0100,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0690,
0x1012, 0x00B4,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC106,
0x1022, 0x0618,
0x1024, 0x0306,
0x1026, 0x0A0A,
0x1028, 0x1008,
0x102A, 0x0A05,
0x102C, 0x1200,
0x1038, 0x0000,
0x103E, 0x0101,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x06AE,
0x1600, 0x0400,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,

};
#endif

static void preview_setting(void)
{
#if MULTI_WRITE
	hi1337_table_write_cmos_sensor(
		addr_data_pair_preview_hi1337,
		sizeof(addr_data_pair_preview_hi1337) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_capture_30fps_hi1337[] = {

/*
DISP_WIDTH = 4208
DISP_HEIGHT = 3120
DISP_NOTE = "NORMAL_PD2_VC"
MIPI_SPEED = 1440.00
MIPI_LANE = 4
DISP_DATAORDER = GR

*/
//Sensor Information////////////////////////////
//Sensor			: Hi-1337
//Date				: 2020-03-30
//Customer			: Longcheer M505
//Image size		: 4208x3120
//MCLK				: 24MHz
//MIPI speed(Mbps)	: 1440Mbps x 4Lane
//Pixel order		: GR
//Frame rate		: 30.00fps
//PDAF				: 3 / PD-DPC off, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0000,
0x0206, 0x02D0,
0x020A, 0x0D01,
0x020E, 0x0D05,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002E,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0C61,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x0008,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x1070,
0x0B14, 0x0C30,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0202,
0x1118, 0x0000,
0x0A10, 0xB040,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0C30,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0B46,
0x1202, 0x1E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0D39,
0x1012, 0x0181,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC10B,
0x1022, 0x0C31,
0x1024, 0x030C,
0x1026, 0x1410,
0x1028, 0x1C0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x0000,
0x103E, 0x0001,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x0D75,
0x1600, 0x0000,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,


};

static kal_uint16 addr_data_pair_capture_15fps_hi1337[] = {

//Sensor Information////////////////////////////
//Sensor			: Hi-1337
//Date				: 2020-03-30
//Customer			: Longcheer M505
//Image size		: 4208x3120
//MCLK				: 24MHz
//MIPI speed(Mbps)	: 1440Mbps x 4Lane
//Pixel order		: GR
//Frame rate		: 15.10fps
//PDAF				: 3 / PD-DPC on, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0000,
0x0206, 0x02D0,
0x020A, 0x19DA,
0x020E, 0x19DE,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002E,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0C61,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x0008,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x1070,
0x0B14, 0x0C30,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0202,
0x1118, 0x0000,
0x0A10, 0xB040,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0C30,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0D39,
0x1012, 0x0181,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC10B,
0x1022, 0x0C31,
0x1024, 0x030C,
0x1026, 0x1410,
0x1028, 0x1C0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x0000,
0x103E, 0x0001,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x0D75,
0x1600, 0x0000,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,

};
#endif


static void capture_setting(kal_uint16 currefps)
{
#if MULTI_WRITE
	if (currefps == 300) {
	hi1337_table_write_cmos_sensor(
		addr_data_pair_capture_30fps_hi1337,
		sizeof(addr_data_pair_capture_30fps_hi1337) /
		sizeof(kal_uint16));

	} else {
	hi1337_table_write_cmos_sensor(
		addr_data_pair_capture_15fps_hi1337,
		sizeof(addr_data_pair_capture_15fps_hi1337) /
		sizeof(kal_uint16));
	}
#else
  if( currefps == 300) {


// PIP
  } else	{

	}
#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_video_hi1337[] = {

/*
DISP_WIDTH = 4208
DISP_HEIGHT = 3120
DISP_NOTE = "NORMAL_PD2_VC"
MIPI_SPEED = 1440.00
MIPI_LANE = 4
DISP_DATAORDER = GR
*/
//Sensor Information////////////////////////////
//Sensor			: Hi-1337
//Date				: 2020-03-30
//Customer			: Longcheer M505
//Image size		: 4208x3120
//MCLK				: 24MHz
//MIPI speed(Mbps)	: 1440Mbps x 4Lane
//Pixel order		: GR
//Frame rate		: 30.00fps
//PDAF				: 3 / PD-DPC off, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0000,
0x0206, 0x02D0,
0x020A, 0x0D01,
0x020E, 0x0D05,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x002E,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0C61,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x0008,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x1070,
0x0B14, 0x0C30,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0202,
0x1118, 0x0000,
0x0A10, 0xB040,
0x0C14, 0x0008,
0x0C18, 0x1070,
0x0C1A, 0x0C30,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0B46,
0x1202, 0x1E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0D39,
0x1012, 0x0181,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC10B,
0x1022, 0x0C31,
0x1024, 0x030C,
0x1026, 0x1410,
0x1028, 0x1C0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x0000,
0x103E, 0x0001,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x0D75,
0x1600, 0x0000,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,


};
#endif

static void normal_video_setting(void)
{
#if MULTI_WRITE
	hi1337_table_write_cmos_sensor(
		addr_data_pair_video_hi1337,
		sizeof(addr_data_pair_video_hi1337) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_hs_video_hi1337[] = {

/*
DISP_WIDTH = 1280
DISP_HEIGHT = 720
DISP_NOTE = "HD"
MIPI_SPEED = 480.00
MIPI_LANE = 4
DISP_DATAORDER = GR
*/
//Sensor Information////////////////////////////
//Sensor			: Hi-1337
//Date				: 2020-03-30
//Customer			: Longcheer M505
//Image size		: 1280x720
//MCLK				: 24MHz
//MIPI speed(Mbps)	: 480Mbps x 4Lane
//Pixel order		: GR
//Frame rate		: 120.03fps
//PDAF				: 3 / PD-DPC on, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0000,
0x0206, 0x02D0,
0x020A, 0x033D,
0x020E, 0x0341,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x020A,
0x022A, 0x0017,
0x022C, 0x0E3D,
0x022E, 0x0A83,
0x0234, 0x1111,
0x0236, 0x3333,
0x0238, 0x3333,
0x023A, 0x1133,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0800,
0x0F04, 0x0040,
0x0B02, 0x0100,
0x0B04, 0x00FC,
0x0B12, 0x0500,
0x0B14, 0x02D0,
0x0B20, 0x0300,
0x1100, 0x1100,
0x1108, 0x0002,
0x1118, 0x02A2,
0x0A10, 0xB040,
0x0C14, 0x00C0,
0x0C18, 0x0F00,
0x0C1A, 0x02D0,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0200,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0456,
0x1012, 0x0097,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC104,
0x1022, 0x0512,
0x1024, 0x0305,
0x1026, 0x0808,
0x1028, 0x0D06,
0x102A, 0x0705,
0x102C, 0x0D00,
0x1038, 0x0000,
0x103E, 0x0201,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x046A,
0x1600, 0x0000,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,



};
#endif

static void hs_video_setting(void)
{

#if MULTI_WRITE
	hi1337_table_write_cmos_sensor(
		addr_data_pair_hs_video_hi1337,
		sizeof(addr_data_pair_hs_video_hi1337) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_slim_video_hi1337[] = {

/*
[SENSOR_RES_MOD]
DISP_WIDTH = 1920
DISP_HEIGHT = 1080
DISP_NOTE = "FHD"
MIPI_SPEED = 720.00
MIPI_LANE = 4
DISP_DATAORDER = GR
*/
//Sensor Information////////////////////////////
//Sensor			: Hi-1337
//Date				: 2020-03-30
//Customer			: Longcheer M505
//Image size		: 1920x1080
//MCLK				: 24MHz
//MIPI speed(Mbps)	: 720Mbps x 4Lane
//Pixel order		: GR
//Frame rate		: 60.02fps
//PDAF				: 3 / PD-DPC on, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0200,
0x0206, 0x02D0,
0x020A, 0x067E,
0x020E, 0x0682,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x020C,
0x022A, 0x0015,
0x022C, 0x0E2D,
0x022E, 0x0A81,
0x0234, 0x3311,
0x0236, 0x3311,
0x0238, 0x3311,
0x023A, 0x2222,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0400,
0x0F04, 0x0060,
0x0B02, 0x0100,
0x0B04, 0x00FC,
0x0B12, 0x0780,
0x0B14, 0x0438,
0x0B20, 0x0200,
0x1100, 0x1100,
0x1108, 0x0002,
0x1118, 0x02A4,
0x0A10, 0xB070,
0x0C14, 0x00C0,
0x0C18, 0x0F00,
0x0C1A, 0x0438,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0100,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0690,
0x1012, 0x00EE,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC106,
0x1022, 0x0618,
0x1024, 0x0306,
0x1026, 0x0A0A,
0x1028, 0x1008,
0x102A, 0x0A05,
0x102C, 0x1200,
0x1038, 0x0000,
0x103E, 0x0101,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x06AE,
0x1600, 0x0400,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,


};
#endif


static void slim_video_setting(void)
{

#if MULTI_WRITE
	hi1337_table_write_cmos_sensor(
		addr_data_pair_slim_video_hi1337,
		sizeof(addr_data_pair_slim_video_hi1337) /
		sizeof(kal_uint16));
#else

#endif
}

#if MULTI_WRITE
static kal_uint16 addr_data_pair_custom1_24fps_hi1337[] = {
/*
DISP_WIDTH = 3264
DISP_HEIGHT = 2448
DISP_NOTE = "BIN2"
MIPI_SPEED = 1440.00
MIPI_LANE = 4
DISP_DATAORDER = GR
*/
//Sensor Information////////////////////////////
//Sensor			: Hi-1337
//Date				: 2020-03-30
//Customer			: Longcheer M505
//Image size		: 3264x2448
//MCLK				: 24MHz
//MIPI speed(Mbps)	: 1440Mbps x 4Lane
//Pixel order		: GR
//Frame rate		: 24.10fps
//PDAF				: 3 / PD-DPC on, Dyn-DPC on
////////////////////////////////////////////////
0x0B00, 0x0000,
0x0204, 0x0000,
0x0206, 0x02D0,
0x020A, 0x1031,
0x020E, 0x1035,
0x0214, 0x0200,
0x0216, 0x0200,
0x0218, 0x0200,
0x021A, 0x0200,
0x0224, 0x017E,
0x022A, 0x0017,
0x022C, 0x0E1F,
0x022E, 0x0B11,
0x0234, 0x1111,
0x0236, 0x1111,
0x0238, 0x1111,
0x023A, 0x1111,
0x0248, 0x0100,
0x0250, 0x0000,
0x0252, 0x0006,
0x0254, 0x0000,
0x0256, 0x0000,
0x0258, 0x0000,
0x025A, 0x0000,
0x025C, 0x0000,
0x025E, 0x0202,
0x0440, 0x0032,
0x0F00, 0x0000,
0x0F04, 0x01E0,
0x0B02, 0x0100,
0x0B04, 0x00DC,
0x0B12, 0x0CC0,
0x0B14, 0x0990,
0x0B20, 0x0100,
0x1100, 0x1100,
0x1108, 0x0002,
0x1118, 0x0216,
0x0A10, 0xB040,
0x0C14, 0x01E0,
0x0C18, 0x0CC0,
0x0C1A, 0x0990,
0x0730, 0x0001,
0x0732, 0x0000,
0x0734, 0x0300,
0x0736, 0x0060,
0x0738, 0x0003,
0x073C, 0x0700,
0x0740, 0x0000,
0x0742, 0x0000,
0x0744, 0x0300,
0x0746, 0x00B4,
0x0748, 0x0002,
0x074A, 0x0900,
0x074C, 0x0000,
0x074E, 0x0100,
0x0750, 0x0000,
0x1200, 0x0926,
0x1202, 0x0E00,
0x120E, 0x6027,
0x1210, 0x8027,
0x1000, 0x0300,
0x1002, 0xC311,
0x1004, 0x2BB0,
0x1010, 0x0D39,
0x1012, 0x02A8,
0x1014, 0x0020,
0x1016, 0x0020,
0x101A, 0x0020,
0x1020, 0xC10B,
0x1022, 0x0C31,
0x1024, 0x030C,
0x1026, 0x1410,
0x1028, 0x1C0E,
0x102A, 0x140A,
0x102C, 0x2200,
0x1038, 0x0000,
0x103E, 0x0001,
0x1042, 0x0008,
0x1044, 0x0120,
0x1046, 0x01B0,
0x1048, 0x0090,
0x1066, 0x0D75,
0x1600, 0x0000,
0x1608, 0x0020,
0x160A, 0x1200,
0x160C, 0x001A,
0x160E, 0x0D80,


};
#endif

static void custom1_setting(void)
{
#if MULTI_WRITE
	hi1337_table_write_cmos_sensor(
		addr_data_pair_custom1_24fps_hi1337,
		sizeof(addr_data_pair_custom1_24fps_hi1337) /
		sizeof(kal_uint16));
#endif
}
static kal_uint16 read_cmos_sensor_hi1337(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, 0xA8);

	return get_byte;
}

static void hi1337_fusion_id_read(void)
{
	int i;
	for (i=0; i<9; i++) {
		fusion_id_main[i] = read_cmos_sensor_hi1337(0x24+i);
		pr_devel("%s %d fusion_id_front[%d]=0x%2x\n",__func__, __LINE__, i, fusion_id_main[i]);
	}
}
static int hi1337_vendor_id_read(int addr)
{
	int  flag = 0;
	flag = read_cmos_sensor_hi1337(0x10);
    pr_info("hynix_hi1337_III  read vendor id , form 0x10 is: 0x%x\n", flag);
	return flag;
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	int  flag = 0;
	flag = hi1337_vendor_id_read(0x10); //0x10 for AAC
    if( flag != HI1337_VENDOR_ID) {
        pr_info("hynix_hi1337_I match vendor id fail, reead vendor id is: 0x%x,expect vendor id is 0x41 \n", flag);
        return ERROR_SENSOR_CONNECT_FAIL;
    }else{
        hi1337_fusion_id_read();
    }
    pr_info("hynix_hi1337_II match vendor id successed, reead vendor id is: 0x%x,expect vendor id is 0x41 \n", flag);

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
			LOG_INF("i2c write id : 0x%x, sensor id: 0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);

			}

			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}

	if (*sensor_id != imgsensor_info.sensor_id) {
		LOG_INF("Read id fail,sensor id: 0x%x\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	LOG_INF("[open]: PLATFORM:MT6737,MIPI 24LANE\n");
	LOG_INF("preview 1296*972@30fps,360Mbps/lane;"
		"capture 2592*1944@30fps,880Mbps/lane\n");
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		LOG_INF("open sensor id fail: 0x%x\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	//imgsensor.pdaf_mode = 1;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}	/*	open  */
static kal_uint32 close(void)
{
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("[hi1337] preview mode start\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("[hi1337] capture mode start\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    if (imgsensor.current_fps == imgsensor_info.cap.max_framerate)	{
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
	 //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n", imgsensor.current_fps);
	capture_setting(imgsensor.current_fps);

	return ERROR_NONE;

}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("[hi1337] normal video mode start\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("[hi1337] hs video mode start\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("[hi1337] slim video mode start\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();

    return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    pr_info("[hi1337] custom1 mode start\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.current_fps = imgsensor_info.custom1.max_framerate;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();
    return ERROR_NONE;
}


static kal_uint32 get_resolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	sensor_resolution->SensorCustom1Width =
		imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height =
		imgsensor_info.custom1.grabwindow_height;

	return ERROR_NONE;
}    /*    get_resolution    */


static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *sensor_info,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType =
	imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;
	sensor_info->Custom1DelayFrame =
		imgsensor_info.custom1_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;    // 0 is default 1x
	sensor_info->SensorPacketECCOrder = 1;

#if ENABLE_PDAF
	sensor_info->PDAF_Support = PDAF_SUPPORT_RAW;
#endif

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	    sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
	break;

	default:
	    sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
	    sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

	    sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
	break;
	}

	return ERROR_NONE;
}    /*    get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		LOG_INF("[odin]preview\n");
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		LOG_INF("[odin]capture\n");
	//case MSDK_SCENARIO_ID_CAMERA_ZSD:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		LOG_INF("[odin]video preview\n");
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;

	default:
		LOG_INF("[odin]default mode\n");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);

	if ((framerate == 30) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 15) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = 10 * framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);
	set_dummy();
	return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable,
			UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n",
				scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
		(frame_length - imgsensor_info.normal_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
				imgsensor_info.cap1.max_framerate) {
		frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.cap1.framelength) ?
			(frame_length - imgsensor_info.cap1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
			LOG_INF("fps %d fps not support,use cap: %d fps!\n",
			framerate, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
	    imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ?
			(frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;

	default:  //coding with  preview scenario by default
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
				imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
	    LOG_INF("error scenario_id = %d, we use preview scenario\n",
				scenario_id);
	break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
				enum MSDK_SCENARIO_ID_ENUM scenario_id,
				MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	    *framerate = imgsensor_info.pre.max_framerate;
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	    *framerate = imgsensor_info.normal_video.max_framerate;
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    *framerate = imgsensor_info.cap.max_framerate;
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    *framerate = imgsensor_info.hs_video.max_framerate;
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    *framerate = imgsensor_info.slim_video.max_framerate;
	break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;

	default:
	break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("set_test_pattern_mode enable: %d", enable);
	if (enable) {
		write_cmos_sensor(0x1038, 0x0000); //mipi_virtual_channel_ctrl
		write_cmos_sensor(0x1042, 0x0008); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		write_cmos_sensor(0x0b04, 0x0141);
		write_cmos_sensor(0x0C0A, 0x0200);

	} else {
		write_cmos_sensor(0x1038, 0x4100); //mipi_virtual_channel_ctrl
		write_cmos_sensor(0x1042, 0x0108); //mipi_pd_sep_ctrl_h, mipi_pd_sep_ctrl_l
		write_cmos_sensor(0x0b04, 0x0349);
		write_cmos_sensor(0x0C0A, 0x0000);

	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_no_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0b00, 0x0100); // stream on
	else
		write_cmos_sensor(0x0b00, 0x0000); // stream off

	mdelay(3);
	return ERROR_NONE;
}

static kal_uint32 feature_control(
			MSDK_SENSOR_FEATURE_ENUM feature_id,
			UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;

#if ENABLE_PDAF
    struct SET_PD_BLOCK_INFO_T *PDAFinfo;
    //struct SENSOR_VC_INFO_STRUCT *pvcinfo;
#endif

	unsigned long long *feature_data =
		(unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PIXEL_RATE:
	switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80)) * imgsensor_info.cap.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80)) * imgsensor_info.normal_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80)) * imgsensor_info.hs_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80)) * imgsensor_info.slim_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.custom1.pclk /
			(imgsensor_info.custom1.linelength - 80)) * imgsensor_info.custom1.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = (imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80)) * imgsensor_info.pre.grabwindow_width;
			break;
		}
	break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(
			(UINT16) *feature_data, (UINT16) *(feature_data + 1));
	break;
	case SENSOR_FEATURE_GET_PERIOD:
	    *feature_return_para_16++ = imgsensor.line_length;
	    *feature_return_para_16 = imgsensor.frame_length;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
	    *feature_return_para_32 = imgsensor.pclk;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_ESHUTTER:
	    set_shutter(*feature_data);
	break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
	    night_mode((BOOL) * feature_data);
	break;
	case SENSOR_FEATURE_SET_GAIN:
	    set_gain((UINT16) *feature_data);
	break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
	break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
	break;
	case SENSOR_FEATURE_SET_REGISTER:
	    write_cmos_sensor(sensor_reg_data->RegAddr,
						sensor_reg_data->RegData);
	break;
	case SENSOR_FEATURE_GET_REGISTER:
	    sensor_reg_data->RegData =
				read_cmos_sensor(sensor_reg_data->RegAddr);
	break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
	    *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
	    set_video_mode(*feature_data);
	break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
	    get_imgsensor_id(feature_return_para_32);
	break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
	    set_auto_flicker_mode((BOOL)*feature_data_16,
			*(feature_data_16+1));
	break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
	    set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data+1));
	break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	    get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
	break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
	    set_test_pattern_mode((BOOL)*feature_data);
	break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	    *feature_return_para_32 = imgsensor_info.checksum_value;
	    *feature_para_len = 4;
	break;
	case SENSOR_FEATURE_SET_FRAMERATE:
	    //LOG_INF("current fps :%d\n", (UINT32)*feature_data);
	    spin_lock(&imgsensor_drv_lock);
             imgsensor.current_fps = (UINT16)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_SET_HDR:
	    LOG_INF("ihdr enable :%d\n", (UINT8)*feature_data_32);
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.ihdr_en = (UINT8)*feature_data_32;
	    spin_unlock(&imgsensor_drv_lock);
	break;
	case SENSOR_FEATURE_GET_CROP_INFO:
	    LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
				(UINT32)*feature_data);

	    wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
		break;
		}
	break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	    LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
	#if 0
	    ihdr_write_shutter_gain((UINT16)*feature_data,
			(UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
	#endif
	break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_i32 = 0;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
#if ENABLE_PDAF

		case SENSOR_FEATURE_GET_VC_INFO:
                    LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16)*feature_data);
                    break;

		case SENSOR_FEATURE_GET_PDAF_DATA:
                    LOG_INF("odin GET_PDAF_DATA EEPROM\n");
#if 0
		// read from e2prom
#if e2prom
		read_eeprom((kal_uint16)(*feature_data), 
				(char *)(uintptr_t)(*(feature_data+1)), 
				(kal_uint32)(*(feature_data+2)) );
#else
		// read from file

	        LOG_INF("READ PDCAL DATA\n");
		read_hi1337_eeprom((kal_uint16)(*feature_data), 
				(char *)(uintptr_t)(*(feature_data+1)), 
				(kal_uint32)(*(feature_data+2)) );

#endif
#endif
		break;
		case SENSOR_FEATURE_GET_PDAF_INFO:
			PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));  
			switch( *feature_data) 
			{
		 		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		 		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				case MSDK_SCENARIO_ID_CUSTOM1:
					memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
					break;
		 		//case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		 		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		 		case MSDK_SCENARIO_ID_SLIM_VIDEO:
		 		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	 	 		default:
					break;
			}
		break;

	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		//PDAF capacity enable or not, 2p8 only full size support PDAF
		switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // type2 - VC enable
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
		}
		LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%lld\n", *feature_data);
		break;

	case SENSOR_FEATURE_SET_PDAF:
			 	imgsensor.pdaf_mode = *feature_data_16;
	        	LOG_INF("[odin] pdaf mode : %d \n", imgsensor.pdaf_mode);
				break;
	
#endif



	default:
	break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 HYNIX_HI1337_III_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	/*	HYNIX_HI1337_I_SensorInit	*/
