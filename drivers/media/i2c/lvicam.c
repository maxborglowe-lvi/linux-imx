// SPDX-License-Identifier: : GPL-2.0-only
/*
 * LVI camera bridge board
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mc.h>
#include <media/v4l2-event.h>
#include <linux/i2c.h>

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#include "tc358746_regs.h"
#include "lvicam.h"
#include <linux/lviconfig_parameters.h>

#include <linux/signal.h>
#include <linux/gpio/consumer.h>

#include <linux/regulator/consumer.h>

MODULE_SOFTDEP("pre: tps55287_pmic");

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-3)");

MODULE_DESCRIPTION("TODO");
MODULE_AUTHOR("tdb");
MODULE_LICENSE("GPL v2");

#define I2C_MAX_XFER_SIZE (512 + 2)

static struct fasync_struct *lvicam_async_queue;

struct lvicam_mode {
	u32 width;
	u32 height;
};

#define DEVICE_NAME "lvicam"
#define CLASS_NAME "lvicam"

struct lvicam {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct v4l2_mbus_framefmt fmt;
	struct mutex mutex;

	const struct lvicam_mode *curr_mode;

	struct gpio_desc *reset_gpio;
	struct gpio_desc *seesaw_gpio;
	struct gpio_desc *power_gpio;
	struct gpio_desc *onoff_gpio;
};

static const struct v4l2_mbus_framefmt tc358746_def_fmt = {
	.width = 1920,
	.height = 1080,
	.code = MEDIA_BUS_FMT_UYVY8_2X8,
	.field = V4L2_FIELD_NONE,
	.colorspace = V4L2_COLORSPACE_DEFAULT,
	.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT,
	.quantization = V4L2_QUANTIZATION_DEFAULT,
	.xfer_func = V4L2_XFER_FUNC_DEFAULT,
};

struct tc358746_mbus_fmt {
	u32 code;
	u8 bus_width;
	u8 bpp; /* total bpp */
	u8 pdformat; /* peripheral data format */
	u8 pdataf; /* parallel data format option */
	u8 ppp; /* pclk per pixel */
	bool csitx_only; /* format only in csi-tx mode supported */
};

static const struct lvicam_mode lvicam_modes[] = { {
	.width = 1920,
	.height = 1080,
} };

/* TODO: Add other formats as required */
/* Todo remove unsoported by LVI*/
static const struct tc358746_mbus_fmt tc358746_formats[] = {
	{
		.code = MEDIA_BUS_FMT_UYVY8_2X8,
		.bus_width = 8,
		.bpp = 16,
		.pdformat = DATAFMT_PDFMT_YCBCRFMT_422_8_BIT,
		.pdataf = CONFCTL_PDATAF_MODE0,
		.ppp = 2,
	}

	// TODO : These are not needed
	// ,{
	// 	.code = MEDIA_BUS_FMT_UYVY8_1X16,
	// 	.bus_width = 16,
	// 	.bpp = 16,
	// 	.pdformat = DATAFMT_PDFMT_YCBCRFMT_422_8_BIT,
	// 	.pdataf = CONFCTL_PDATAF_MODE1,
	// 	.ppp = 1,
	// },
	// {
	// 	.code = MEDIA_BUS_FMT_YUYV8_1X16,
	// 	.bus_width = 16,
	// 	.bpp = 16,
	// 	.pdformat = DATAFMT_PDFMT_YCBCRFMT_422_8_BIT,
	// 	.pdataf = CONFCTL_PDATAF_MODE2,
	// 	.ppp = 1,
	// },
	// {
	// 	.code = MEDIA_BUS_FMT_UYVY10_2X10,
	// 	.bus_width = 10,
	// 	.bpp = 20,
	// 	.pdformat = DATAFMT_PDFMT_YCBCRFMT_422_10_BIT,
	// 	.pdataf = CONFCTL_PDATAF_MODE0, /* don't care */
	// 	.ppp = 2,
	// }, {
	// 	/* in datasheet listed as YUV444 */
	// 	.code = MEDIA_BUS_FMT_GBR888_1X24,
	// 	.bus_width = 24,
	// 	.bpp = 24,
	// 	.pdformat = DATAFMT_PDFMT_YCBCRFMT_444,
	// 	.pdataf = CONFCTL_PDATAF_MODE0, /* don't care */
	// 	.ppp = 2,
	// 	.csitx_only = true,
	// },
};

/* ############### LVICAM CONTROLLER STUFF ############### */

/* This struct is used to store all parameters related to control of the LVI camera.
* Commands are sent via i2c to the FPGA.
* Command documentation: "R:\Konstruktionsavdelningen\Aktuella Projekt\490 ZIP NXTG\11 Mjukvara\FPGA NXTG24\FPGA_Register_Configurations" */
typedef struct {
	struct lvicam *lvicam_ptr; /* Pointer to the lvicam struct */

	struct i2c_client *client;
	struct class *class;
	struct cdev cdev;
	struct i2c_adapter *adapter;
	unsigned int dev_num;

	/* Initial values are imported from EEPROM via lviconfig */
	uint16_t zoom; /* Current zoom value */
	uint16_t zoom_min; /* Minimum zoom value */
	uint16_t zoom_max; /* Maximum zoom value */
	uint16_t zoom_speed; /* Zoom speed */
} lvicam_controller;

lvicam_controller lvicam_ctrl;

static struct lvicam *lvicam = NULL;

/* Helpers */
static const struct tc358746_mbus_fmt *tc358746_get_format(u32 code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tc358746_formats); i++)
		if (tc358746_formats[i].code == code)
			return &tc358746_formats[i];

	return NULL;
}

static inline struct lvicam *to_lvicam(struct v4l2_subdev *_sd)
{
	return container_of(_sd, struct lvicam, sd);
}

/* --------------- i2c helper ------------ */

static void i2c_rd(struct v4l2_subdev *sd, u16 reg, u8 *values, u32 n)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u8 buf[2] = { reg >> 8, reg & 0xff };
	u8 data[I2C_MAX_XFER_SIZE];

	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = n,
			.buf = data,
		},
	};

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != ARRAY_SIZE(msgs)) {
		v4l2_err(sd, "%s: reading register 0x%x from 0x%x failed\n", __func__, reg, client->addr);
	}

	switch (n) {
	case 1:
		values[0] = data[0];
		break;
	case 2:
		values[0] = data[1];
		values[1] = data[0];
		break;
	case 4:
		values[0] = data[1];
		values[1] = data[0];
		values[2] = data[3];
		values[3] = data[2];
		break;
	default:
		v4l2_info(sd, "unsupported I2C read %d bytes from address 0x%04x\n", n, reg);
	}

	if (debug < 3)
		return;

	switch (n) {
	case 1:
		v4l2_info(sd, "I2C read 0x%04x = 0x%02x", reg, data[0]);
		break;
	case 2:
		v4l2_info(sd, "I2C read 0x%04x = 0x%02x%02x", reg, data[0], data[1]);
		break;
	case 4:
		v4l2_info(sd, "I2C read 0x%04x = 0x%02x%02x%02x%02x", reg, data[2], data[3], data[0], data[1]);
		break;
	default:
		v4l2_info(sd, "I2C unsupported read %d bytes from address 0x%04x\n", n, reg);
	}
}

static void i2c_wr(struct v4l2_subdev *sd, u16 reg, u8 *values, u32 n)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	struct i2c_msg msg;
	u8 data[I2C_MAX_XFER_SIZE];

	if ((2 + n) > I2C_MAX_XFER_SIZE) {
		n = I2C_MAX_XFER_SIZE - 2;
		v4l2_warn(sd, "i2c wr reg=%04x: len=%d is too big!\n", reg, 2 + n);
	}

	msg.addr = client->addr;
	msg.buf = data;
	msg.len = 2 + n;
	msg.flags = 0;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;

	switch (n) {
	case 1:
		data[2 + 0] = values[0];
		break;
	case 2:
		data[2 + 0] = values[1];
		data[2 + 1] = values[0];
		break;
	case 4:
		data[2 + 0] = values[1];
		data[2 + 1] = values[0];
		data[2 + 2] = values[3];
		data[2 + 3] = values[2];
		break;
	default:
		v4l2_info(sd, "unsupported I2C write %d bytes from address 0x%04x\n", n, reg);
	}

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err != 1) {
		pr_err("[%s] writing register 0x%x from 0x%x failed\n", __func__, reg, client->addr);
		return;
	}

	if (debug < 3)
		return;

	switch (n) {
	case 1:
		pr_info("[%s] I2C write 0x%04x = 0x%02x\n", __func__, reg, data[2 + 0]);
		break;
	case 2:
		pr_info("[%s] I2C write 0x%04x = 0x%02x%02x\n", __func__, reg, data[2 + 0], data[2 + 1]);
		break;
	case 4:
		pr_info("[%s] I2C write 0x%04x = 0x%02x%02x%02x%02x\n", __func__, reg, data[2 + 2], data[2 + 3], data[2 + 0], data[2 + 1]);
		break;
	default:
		pr_info("[%s] I2C unsupported write %d bytes from address 0x%04x\n", __func__, n, reg);
	}
}

static noinline u32 i2c_rdreg(struct v4l2_subdev *sd, u16 reg, u32 n)
{
	__le32 val = 0;

	i2c_rd(sd, reg, (u8 __force *)&val, n);

	return le32_to_cpu(val);
}

static noinline void i2c_wrreg(struct v4l2_subdev *sd, u16 reg, u32 val, u32 n)
{
	__le32 raw = cpu_to_le32(val);

	i2c_wr(sd, reg, (u8 __force *)&raw, n);
}

static u16 __maybe_unused i2c_rd8(struct v4l2_subdev *sd, u16 reg)
{
	return i2c_rdreg(sd, reg, 1);
}

static u16 __maybe_unused i2c_rd16(struct v4l2_subdev *sd, u16 reg)
{
	return i2c_rdreg(sd, reg, 2);
}

static u32 __maybe_unused i2c_rd32(struct v4l2_subdev *sd, u16 reg)
{
	return i2c_rdreg(sd, reg, 4);
}

static void __maybe_unused i2c_wr8(struct v4l2_subdev *sd, u16 reg, u16 val)
{
	i2c_wrreg(sd, reg, val, 1);
}

static void i2c_wr16(struct v4l2_subdev *sd, u16 reg, u16 val)
{
	i2c_wrreg(sd, reg, val, 2);
}

static void i2c_wr16_and_or(struct v4l2_subdev *sd, u16 reg, u32 mask, u16 val)
{
	u16 m = (u16)~mask;

	i2c_wrreg(sd, reg, (i2c_rd16(sd, reg) & m) | val, 2);
}

static void i2c_wr32(struct v4l2_subdev *sd, u16 reg, u32 val)
{
	i2c_wrreg(sd, reg, val, 4);
}

static void lvicam_setup(struct v4l2_subdev *sd)
{
	pr_info("[%s] call\n", __func__);

	//*********************************************
	//Start up sequence
	//*********************************************
	//**************************************************
	//TC358746(A)XBG Software Reset
	//**************************************************
	i2c_wr16(sd, 0x0002, 0x0001); // SYSctl, S/W Reset
	usleep_range(10, 100);
	i2c_wr16(sd, 0x0002, 0x0000); // SYSctl, S/W Reset release
	//**************************************************
	//TC358746(A)XBG PLL,Clock Setting
	//**************************************************
	i2c_wr16(sd, 0x0016,
		 0x1031); // PLL Control Register 0 (PLL_PRD,PLL_FBD)
	i2c_wr16(sd, 0x0018,
		 0x0203); // PLL_FRS,PLL_LBWS, PLL oscillation enable
	//delay 1000
	usleep_range(1000, 2000);
	i2c_wr16(sd, 0x0018, 0x0213); // PLL_FRS,PLL_LBWS, PLL clock out enable
	//**************************************************
	//TC358746(A)XBG DPI Input Control
	//**************************************************
	i2c_wr16(sd, 0x0006, 0x0190); // FIFO Control Register
	i2c_wr16(sd, 0x0008, 0x0060); // Data Format setting
	i2c_wr16(sd, 0x0022, 0x0F00); // Word Count
	//**************************************************
	//TC358746XBG MCLK Output
	//**************************************************
	//**************************************************
	//TC358746(A)XBG GPIO2,1 Control (Example)
	//**************************************************
	//**************************************************
	//TC358746(A)XBG D-PHY Setting
	//**************************************************
	i2c_wr16(sd, 0x0140, 0x0000); // D-PHY Clock lane enable
	i2c_wr16(sd, 0x0142, 0x0000);
	i2c_wr16(sd, 0x0144, 0x0000); // D-PHY Data lane0 enable
	i2c_wr16(sd, 0x0146, 0x0000);
	i2c_wr16(sd, 0x0148, 0x0000); // D-PHY Data lane1 enable
	i2c_wr16(sd, 0x014A, 0x0000);
	i2c_wr16(sd, 0x014C, 0x0000); // D-PHY Data lane2 enable
	i2c_wr16(sd, 0x014E, 0x0000);
	i2c_wr16(sd, 0x0150, 0x0000); // D-PHY Data lane3 enable
	i2c_wr16(sd, 0x0152, 0x0000);
	//**************************************************
	//TC358746(A)XBG CSI2-TX PPI Control
	//**************************************************
	i2c_wr16(sd, 0x0210, 0x1B58); // LINEINITCNT
	i2c_wr16(sd, 0x0212, 0x0000);
	i2c_wr16(sd, 0x0214, 0x0005); // LPTXTIMECNT
	i2c_wr16(sd, 0x0216, 0x0000);
	i2c_wr16(sd, 0x0218, 0x2304); // TCLK_HEADERCNT
	i2c_wr16(sd, 0x021A, 0x0000);
	i2c_wr16(sd, 0x0220, 0x0705); // THS_HEADERCNT
	i2c_wr16(sd, 0x0222, 0x0000);
	i2c_wr16(sd, 0x0224, 0x4E20); // TWAKEUPCNT
	i2c_wr16(sd, 0x0226, 0x0000);
	i2c_wr16(sd, 0x022C, 0x0005); // THS_TRAILCNT
	i2c_wr16(sd, 0x022E, 0x0000);
	i2c_wr16(sd, 0x0230, 0x0005); // HSTXVREGCNT
	i2c_wr16(sd, 0x0232, 0x0000);
	i2c_wr16(sd, 0x0234, 0x001F); // HSTXVREGEN enable
	i2c_wr16(sd, 0x0236, 0x0000);
	i2c_wr16(sd, 0x0238, 0x0001); // DSI clock Enable/Disable during LP
	i2c_wr16(sd, 0x023A, 0x0000);
	i2c_wr16(sd, 0x0204, 0x0001); // STARTCNTRL
	i2c_wr16(sd, 0x0206, 0x0000);
	i2c_wr16(sd, 0x0518, 0x0001); // CSI Start
	i2c_wr16(sd, 0x051A, 0x0000);
	//**************************************************
	//Set to HS mode
	//**************************************************
	i2c_wr16(sd, 0x0500, 0x8087); // CSI2 lane setting, CSI2 mode=HS
	i2c_wr16(sd, 0x0502, 0xA300); // bit set
	/*
	 * CONFCTL register: PPEN=1, PDATAF=MODE1 (16-bit parallel bus),
	 * DATALANE=4 lanes. The FPGA sends 16-bit parallel YCbCr data,
	 * so MODE1 is correct despite the CSI output being UYVY8_2X8.
	 */
	i2c_wr16(sd, 0x0004, 0x0143); // Configuration Control Register
}

/* Ops */
static int lvicam_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_mbus_code_enum *code)
{
	struct lvicam *lvicam = to_lvicam(sd);

	pr_info("[%s] call\n", __func__);

	if (code->index >= ARRAY_SIZE(tc358746_formats))
		return -EINVAL;

	mutex_lock(&lvicam->mutex);
	code->code = tc358746_formats[code->index].code;
	mutex_unlock(&lvicam->mutex);

	return 0;
}

static int lvicam_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *fmt)
{
	struct lvicam *lvicam = to_lvicam(sd);

	pr_info("[%s] call\n", __func__);

	if (fmt->pad)
		return -EINVAL;

	fmt->format.code = lvicam->fmt.code;
	fmt->format.colorspace = lvicam->fmt.colorspace;
	fmt->format.field = V4L2_FIELD_NONE;

	fmt->format.width = lvicam->curr_mode->width;
	fmt->format.height = lvicam->curr_mode->height;

	return 0;
}

static int lvicam_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *fmt)
{
	struct lvicam *lvicam = to_lvicam(sd);
	struct v4l2_mbus_framefmt *framefmt;
	const struct lvicam_mode *mode;

	pr_info("[%s] call\n", __func__);

	//TODO: Where is format actually set?
	mutex_lock(&lvicam->mutex);

	mode = v4l2_find_nearest_size(lvicam_modes, ARRAY_SIZE(lvicam_modes), width, height, fmt->format.width, fmt->format.height);

	fmt->format.code = lvicam->fmt.code;
	fmt->format.colorspace = lvicam->fmt.colorspace;
	fmt->format.field = V4L2_FIELD_NONE;

	fmt->format.width = mode->width;
	fmt->format.height = mode->height;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		framefmt = v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
		*framefmt = fmt->format;
	} else {
		lvicam->curr_mode = mode;
	}

	mutex_unlock(&lvicam->mutex);

	return 0;
}

static int lvicam_set_stream(struct v4l2_subdev *sd, int enable)
{
	if (enable) {
		pr_info("[%s] enable:%d\n", __func__, enable);
		lvicam_setup(sd);
	} else {
		pr_info("[%s] disable:%d\n", __func__, enable);
	}

	return 0;
}

static int lvicam_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_frame_size_enum *fse)
{
	pr_info("[%s] call\n", __func__);

	struct lvicam *lvicam = to_lvicam(sd);

	if (fse->index >= ARRAY_SIZE(lvicam_modes)) {
		pr_info("[%s] FAIL : fse-index = %d\n", __func__, fse->index);
		return -EINVAL;
	}

	// mutex_lock(&lvicam->mutex);
	// if (fse->code != lvicam->fmt.code) {
	// 	pr_info("[%s] FAIL : fse->code = %d, lvicam->fmt.code = %d, they should be the same", __func__, fse->code, lvicam->fmt.code);

	// 	mutex_unlock(&lvicam->mutex);
	// 	return -EINVAL;
	// }
	// mutex_unlock(&lvicam->mutex);

	fse->max_width = lvicam_modes[fse->index].width;
	fse->min_width = fse->max_width;

	fse->max_height = lvicam_modes[fse->index].height;
	fse->min_height = fse->max_height;

	pr_info("[%s] SUCCESS : width = %d, height = %d\n", __func__, fse->min_width, fse->min_height);

	return 0;
}

static int lvicam_enum_frame_interval(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_frame_interval_enum *fie)
{
	pr_info("[%s] call\n", __func__);

	struct lvicam *lvicam = to_lvicam(sd);

	if (fie->index != 0)
		return -EINVAL;

	fie->code = lvicam->fmt.code;
	fie->width = lvicam_modes[fie->index].width;
	fie->height = lvicam_modes[fie->index].height;
	fie->interval.numerator = 1;
	fie->interval.denominator = 60;

	return 0;
}

static int lvicam_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	pr_info("[%s] call\n", __func__);

	fi->interval.numerator = 1;
	fi->interval.denominator = 60;

	return 0;
}

static int lvicam_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	pr_info("[%s] call\n", __func__);

	/* Fixed 60 fps output from FPGA - ignore requested interval */
	fi->interval.numerator = 1;
	fi->interval.denominator = 60;

	return 0;
}

static const struct v4l2_subdev_pad_ops lvicam_pad_ops = {
	.enum_mbus_code = lvicam_enum_mbus_code,
	.get_fmt = lvicam_get_fmt,
	.set_fmt = lvicam_set_fmt,
	.enum_frame_size = lvicam_enum_frame_size,
	.enum_frame_interval = lvicam_enum_frame_interval,
};

static const struct v4l2_subdev_video_ops tc358746_video_ops = {
	.s_stream = lvicam_set_stream,
	.g_frame_interval = lvicam_g_frame_interval,
	.s_frame_interval = lvicam_s_frame_interval,
};

static void lvicam_gpio_reset(struct lvicam *lvicam)
{
	pr_info("[%s] call\n", __func__);
	if (!lvicam->reset_gpio)
		return;
	usleep_range(5000, 10000);
	gpiod_set_value_cansleep(lvicam->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(lvicam->reset_gpio, 0);
	msleep(20);
}

static void lvicam_gpio_on_set(struct lvicam *lvicam)
{
	pr_info("[%s] call\n", __func__);
	if (lvicam->onoff_gpio)
		gpiod_set_value_cansleep(lvicam->onoff_gpio, 1);
}

static void lvicam_gpio_off_set(struct lvicam *lvicam)
{
	pr_info("[%s] call\n", __func__);
	if (lvicam->onoff_gpio)
		gpiod_set_value_cansleep(lvicam->onoff_gpio, 0);
}

static int lvicam_s_power(struct v4l2_subdev *sd, int on)
{
	pr_info("[%s] %d\n", __func__, on);

	struct lvicam *lvicam = to_lvicam(sd);

	if (on) {
		pr_info("[%s] Asserting power pin.\n", __func__);
		if (lvicam->power_gpio)
			gpiod_set_value(lvicam->power_gpio, 1);

		pr_info("[%s] Resetting Toshiba converter chip.\n", __func__);
		lvicam_gpio_reset(lvicam);
	} else {
		pr_info("[%s] Powering down.\n", __func__);
		/* Do not reset the TC358746 on power-off;
		 * a reset here would wipe registers that
		 * s_stream(1) already programmed.
		 */
	}

	return 0;
}
#ifdef CONFIG_VIDEO_ADV_DEBUG
static void tc358746_print_register_map(struct v4l2_subdev *sd)
{
	v4l2_info(sd, "0x0000-0x0050: Global Register\n");
	v4l2_info(sd, "0x0056-0x0070: Rx Control Registers\n");
	v4l2_info(sd, "0x0080-0x00F8: Rx Status Registers\n");
	v4l2_info(sd, "0x0100-0x0150: Tx D-PHY Register\n");
	v4l2_info(sd, "0x0204-0x0238: Tx PPI Register\n");
	v4l2_info(sd, "0x040c-0x0518: Tx Control Register\n");
}

static int tc358746_get_reg_size(u16 address)
{
	if (address <= 0x00ff)
		return 2;
	else if ((address >= 0x0100) && (address <= 0x05FF))
		return 4;
	else
		return 1;
}

static int tc358746_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	if (reg->reg > 0xffff) {
		tc358746_print_register_map(sd);
		return -EINVAL;
	}

	reg->size = tc358746_get_reg_size(reg->reg);

	reg->val = i2c_rdreg(sd, reg->reg, reg->size);

	pr_info("[%s] 0x%X=0x%X, %d\n", __func__, reg->reg, reg->val, reg->size);

	return 0;
}

static int tc358746_s_register(struct v4l2_subdev *sd, const struct v4l2_dbg_register *reg)
{
	if (reg->reg > 0xffff) {
		tc358746_print_register_map(sd);
		return -EINVAL;
	}

	i2c_wrreg(sd, (u16)reg->reg, reg->val, tc358746_get_reg_size(reg->reg));

	return 0;
}
#endif

static int tc358746_log_status(struct v4l2_subdev *sd)
{
	struct lvicam *lvicam = to_lvicam(sd);
	//struct tc358746_state *state = to_state(sd);
	uint16_t sysctl = i2c_rd16(sd, SYSCTL);

	v4l2_info(sd, "-----Chip status-----\n");
	v4l2_info(sd, "Chip ID: 0x%02lx\n", (i2c_rd16(sd, CHIPID) & CHIPID_CHIPID_MASK) >> 8);
	v4l2_info(sd, "Chip revision: 0x%02lx\n", i2c_rd16(sd, CHIPID) & CHIPID_REVID_MASK);
	v4l2_info(sd, "Sleep mode: %s\n", sysctl & SYSCTL_SLEEP_MASK ? "on" : "off");

	v4l2_info(sd, "-----CSI-TX status-----\n");
	v4l2_info(sd, "Waiting for particular sync signal: %s\n", (i2c_rd16(sd, CSI_STATUS) & CSI_STATUS_S_WSYNC_MASK) ? "yes" : "no");
	v4l2_info(sd, "Transmit mode: %s\n", (i2c_rd16(sd, CSI_STATUS) & CSI_STATUS_S_TXACT_MASK) ? "yes" : "no");
	v4l2_info(sd, "Stopped: %s\n", (i2c_rd16(sd, CSI_STATUS) & CSI_STATUS_S_HLT_MASK) ? "yes" : "no");
	v4l2_info(sd, "Color space: %s\n", lvicam->fmt.code == MEDIA_BUS_FMT_UYVY8_2X8 ? "YCbCr 422 8-bit" : "Unsupported");

	return 0;
}

static const struct v4l2_subdev_core_ops lvicam_core_ops = {
	.log_status = tc358746_log_status,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = tc358746_g_register,
	.s_register = tc358746_s_register,
#endif
	.s_power = lvicam_s_power,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_ops lvicam_subdev_ops = {
	.core = &lvicam_core_ops,
	.video = &tc358746_video_ops,
	.pad = &lvicam_pad_ops,
};

static int lvicam_link_setup(struct media_entity *entity, const struct media_pad *local, const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations lvicam_entity_ops = {
	.link_setup = lvicam_link_setup,
	/* Makes sure that width, height, media bus pixel code are
	 * the same on both source and sink side of the link.
	 */
	.link_validate = v4l2_subdev_link_validate,
};

/* seesaw gpio interrupt stuff */
static int lvicam_fasync(int fd, struct file *file, int on)
{
	return fasync_helper(fd, file, on, &lvicam_async_queue);
}

static long lvicam_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case LVICAM_CTRL_IOCTL_WRITE_DATA: {
		struct lvicam_i2c_cmd i2c_cmd;
		if (copy_from_user(&i2c_cmd, (void __user *)arg, sizeof(i2c_cmd)))
			return -EFAULT;

		if (i2c_cmd.size != 1 && i2c_cmd.size != 2)
			return -EINVAL;

		uint8_t buf[3] = { 0 };
		int buf_len = 1 + i2c_cmd.size;
		buf[0] = i2c_cmd.subreg;

		if (i2c_cmd.size == 1) {
			buf[1] = (uint8_t)(i2c_cmd.data);
		} else if (i2c_cmd.size == 2) {
			buf[1] = (uint8_t)(i2c_cmd.data >> 8); // MSB
			buf[2] = (uint8_t)(i2c_cmd.data & 0xFF); // LSB
		}

		struct i2c_msg msg = {
			.addr = lvicam_ctrl.client->addr,
			.flags = 0,
			.len = buf_len,
			.buf = buf,
		};

		ret = i2c_transfer(lvicam_ctrl.adapter, &msg, 1);

		pr_info("[%s] WRITE: subreg=0x%02X, size=%zu -> buf[1]=0x%02X buf[2]=0x%02X\n", __func__, i2c_cmd.subreg, i2c_cmd.size, buf[1], buf[2]);

		return (ret == 1) ? 0 : -EIO;
	}

	case LVICAM_CTRL_IOCTL_READ_DATA: {
		struct lvicam_i2c_cmd i2c_cmd;
		if (copy_from_user(&i2c_cmd, (void __user *)arg, sizeof(i2c_cmd)))
			return -EFAULT;

		if (i2c_cmd.size != 1 && i2c_cmd.size != 2)
			return -EINVAL;

		uint8_t subreg = i2c_cmd.subreg;
		uint8_t read_buf[2] = { 0 };

		struct i2c_msg msgs[2] = {
			{
				.addr = lvicam_ctrl.client->addr,
				.flags = 0,
				.len = 1,
				.buf = &subreg,
			},
			{
				.addr = lvicam_ctrl.client->addr,
				.flags = I2C_M_RD,
				.len = i2c_cmd.size,
				.buf = read_buf,
			},
		};

		ret = i2c_transfer(lvicam_ctrl.adapter, msgs, 2);
		if (ret != 2)
			return -EIO;

		/* Combine bytes read from single subreg read */
		if (i2c_cmd.size == 1) {
			i2c_cmd.data = read_buf[0];
		} else if (i2c_cmd.size == 2) {
			i2c_cmd.data = (read_buf[0] << 8) | read_buf[1]; /* MSB first */
		}

		pr_info("[%s] READ: subreg=0x%02X, size=%zu -> data=0x%02X%02X\n", __func__, subreg, i2c_cmd.size, read_buf[0], read_buf[1]);

		if (copy_to_user((void __user *)arg, &i2c_cmd, sizeof(i2c_cmd)))
			return -EFAULT;

		return 0;
	}

	case LVICAM_CTRL_IOCTL_READ_SEESAW: {
		struct lvicam_seesaw_status status;
		status.value = gpiod_get_value(lvicam_ctrl.lvicam_ptr->seesaw_gpio);
		if (copy_to_user((void __user *)arg, &status, sizeof(status)))
			return -EFAULT;
		return 0;
	}

	default:
		return -ENOTTY;
	}
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lvicam_ioctl,
	.fasync = lvicam_fasync,
};

#define I2C_BUS_NUM 2 // /dev/i2c-2
#define I2C_DEV_ADDR 0x10

static int lvicam_ctrl_device_init(struct i2c_client *client)
{
	int ret;

	// lvicam structure is already allocated and GPIOs are available

	// Allocate character device region
	ret = alloc_chrdev_region(&lvicam_ctrl.dev_num, 0, 1, DEVICE_NAME);
	if (ret)
		return ret;

	cdev_init(&lvicam_ctrl.cdev, &fops);
	ret = cdev_add(&lvicam_ctrl.cdev, lvicam_ctrl.dev_num, 1);
	if (ret)
		goto unregister;

	lvicam_ctrl.class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(lvicam_ctrl.class)) {
		ret = PTR_ERR(lvicam_ctrl.class);
		goto del_cdev;
	}

	device_create(lvicam_ctrl.class, NULL, lvicam_ctrl.dev_num, NULL, DEVICE_NAME);

	lvicam_ctrl.adapter = i2c_get_adapter(I2C_BUS_NUM);
	if (!lvicam_ctrl.adapter) {
		ret = -ENODEV;
		goto destroy_device;
	}

	lvicam_ctrl.client = i2c_new_dummy_device(lvicam_ctrl.adapter, I2C_DEV_ADDR);
	if (IS_ERR(lvicam_ctrl.client)) {
		ret = PTR_ERR(lvicam_ctrl.client);
		goto put_adapter;
	}

	// NOW the GPIOs are available! Control onoff GPIO
	if (lvicam->onoff_gpio) {
		pr_info("[%s] Setting the onoff gpio LOW.\n", __func__);
		lvicam_gpio_off_set(lvicam);
	}

	usleep_range(80000, 100000); // Wait 80-100 milliseconds

	if (lvicam->onoff_gpio) {
		pr_info("[%s] Setting the onoff gpio HIGH.\n", __func__);
		lvicam_gpio_on_set(lvicam);
	}

	pr_info("[%s] Device initialized with I2C address 0x%02x\n", __func__, lvicam_ctrl.client->addr << 1);
	return 0;

put_adapter:
	i2c_put_adapter(lvicam_ctrl.adapter);
destroy_device:
	device_destroy(lvicam_ctrl.class, lvicam_ctrl.dev_num);
	class_destroy(lvicam_ctrl.class);
del_cdev:
	cdev_del(&lvicam_ctrl.cdev);
unregister:
	unregister_chrdev_region(lvicam_ctrl.dev_num, 1);
	return ret;
}

static void lvicam_ctrl_device_cleanup(void)
{
	if (lvicam_ctrl.client) {
		i2c_unregister_device(lvicam_ctrl.client);
		lvicam_ctrl.client = NULL;
	}

	if (lvicam_ctrl.adapter) {
		i2c_put_adapter(lvicam_ctrl.adapter);
		lvicam_ctrl.adapter = NULL;
	}

	if (lvicam_ctrl.class) {
		device_destroy(lvicam_ctrl.class, lvicam_ctrl.dev_num);
		class_destroy(lvicam_ctrl.class);
		lvicam_ctrl.class = NULL;
	}

	cdev_del(&lvicam_ctrl.cdev);
	unregister_chrdev_region(lvicam_ctrl.dev_num, 1);
}

/** @brief IRQ handler for seesaw input GPIO
 *	@param 
 */
static irqreturn_t lvicam_seesaw_irq_handler(int irq, void *dev_id)
{
	pr_info("[%s] seesaw GPIO interrupt triggered\n", __func__);
	kill_fasync(&lvicam_async_queue, SIGIO, POLL_IN);
	return IRQ_HANDLED;
}
/* Probe & Remove */

static int lvicam_probe(struct i2c_client *client)
{
	int err = 0;
	struct regulator *vcc = NULL;
	int seesaw_irq = 0;
	int ret = 0;

	pr_info("[%s] call\n", __func__);

	/* --- Get regulator 'vcc' --- */
	vcc = devm_regulator_get(&client->dev, "vcc");
	if (IS_ERR(vcc)) {
		ret = PTR_ERR(vcc);
		if (ret == -EPROBE_DEFER) {
			pr_info("[%s] vcc regulator not ready, deferring probe\n", __func__);
			return -EPROBE_DEFER;
		}
		pr_info("[%s] Failed to get vcc regulator: %d\n", __func__, ret);
		return ret;
	}

	ret = regulator_enable(vcc);
	if (ret) {
		pr_info("[%s] regulator_enable(vcc) failed: %d\n", __func__, ret);
		return ret;
	}
	pr_info("[%s] regulator 'vcc' enabled\n", __func__);

	usleep_range(1000, 2000);

	// Allocate lvicam structure FIRST (before GPIO acquisition)
	lvicam = devm_kzalloc(&client->dev, sizeof(*lvicam), GFP_KERNEL);
	if (!lvicam) {
		pr_err("[%s] Failed to allocate lvicam structure\n", __func__);
		return -ENOMEM;
	}

	// Initialize mutex early
	mutex_init(&lvicam->mutex);

	// Initialize v4l2 subdev early so we can use v4l2_err if needed
	v4l2_i2c_subdev_init(&lvicam->sd, client, &lvicam_subdev_ops);

	// Get power enable GPIO (optional)
	lvicam->power_gpio = devm_gpiod_get_optional(&client->dev, "power", GPIOD_OUT_HIGH);
	if (IS_ERR(lvicam->power_gpio)) {
		err = PTR_ERR(lvicam->power_gpio);
		if (err == -EPROBE_DEFER) {
			pr_info("[%s] Power GPIO dependency not ready, deferring probe.\n", __func__);
		} else {
			pr_err("[%s] Failed to get power gpio: %d\n", __func__, err);
		}
		goto destroy_mutex;
	}
	if (lvicam->power_gpio)
		pr_info("[%s] power GPIO found\n", __func__);
	else
		pr_info("[%s] power GPIO not specified, assuming always enabled\n", __func__);

	// Get ONOFF GPIO (optional)
	lvicam->onoff_gpio = devm_gpiod_get_optional(&client->dev, "onoff", GPIOD_OUT_HIGH);
	if (IS_ERR(lvicam->onoff_gpio)) {
		err = PTR_ERR(lvicam->onoff_gpio);
		pr_err("[%s] Failed to get onoff gpio: %d\n", __func__, err);
		goto destroy_mutex;
	}
	if (lvicam->onoff_gpio)
		pr_info("[%s] onoff GPIO found\n", __func__);
	else
		pr_info("[%s] onoff GPIO not specified, assuming always enabled\n", __func__);

	// Get reset GPIO (optional)
	lvicam->reset_gpio = devm_gpiod_get_optional(&client->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(lvicam->reset_gpio)) {
		err = PTR_ERR(lvicam->reset_gpio);
		pr_err("[%s] Failed to get reset gpio: %d\n", __func__, err);
		goto destroy_mutex;
	}
	if (lvicam->reset_gpio) {
		pr_info("[%s] reset GPIO found\n", __func__);
		gpiod_set_value_cansleep(lvicam->reset_gpio, 1);
	} else {
		pr_info("[%s] reset GPIO not specified, assuming no reset needed\n", __func__);
	}

	// Get seesaw GPIO
	lvicam->seesaw_gpio = devm_gpiod_get(&client->dev, "seesaw", GPIOD_IN);

	if (IS_ERR(lvicam->seesaw_gpio)) {
		pr_err("[%s] : ERROR 2B\n", __func__);
		pr_info("[%s] Failed to get seesaw gpio\n", __func__);
		err = PTR_ERR(lvicam->seesaw_gpio);
		// Don't fail probe if seesaw GPIO is missing - just continue without it
	} else if (lvicam->seesaw_gpio) {
		// Request IRQ for seesaw GPIO
		seesaw_irq = gpiod_to_irq(lvicam->seesaw_gpio);
		if (seesaw_irq < 0) {
			pr_err("[%s] Failed to get IRQ for seesaw gpio\n", __func__);
			err = seesaw_irq;
			goto error_media_entity;
		}
		err = devm_request_threaded_irq(&client->dev, seesaw_irq, NULL, lvicam_seesaw_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lvicam_seesaw", lvicam);
		if (err) {
			pr_err("[%s] Failed to request IRQ for seesaw gpio: %d\n", __func__, err);
			goto error_media_entity;
		}
		pr_info("[%s] seesaw GPIO interrupt registered on IRQ %d\n", __func__, seesaw_irq);
	}

	// NOW initialize the character device and I2C client
	// At this point, all GPIOs are available so lvicam_ctrl_device_init can use them
	ret = lvicam_ctrl_device_init(client);
	if (ret) {
		pr_err("[%s] Failed to initialize lvicam_ctrl: %d\n", __func__, ret);
		err = ret;
		goto error_media_entity;
	}

	// Set lvicam pointer in controller
	lvicam_ctrl.lvicam_ptr = lvicam;

	// Reset the Toshiba converter chip
	if (lvicam->reset_gpio) {
		pr_info("[%s] resetting TC358746.\n", __func__);
		lvicam_gpio_reset(lvicam);
	}

	msleep(10);

	/* Check ID of the connected TC358746 */
	v4l2_err(&lvicam->sd, "Fetching device\n");

	/* Set current mode */
	lvicam->curr_mode = &lvicam_modes[0];

	/* Initialize subdev */
	lvicam->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	lvicam->sd.entity.ops = &lvicam_entity_ops;
	lvicam->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;

	/* Initialize pad */
	lvicam->pad.flags = MEDIA_PAD_FL_SOURCE;
	err = media_entity_pads_init(&lvicam->sd.entity, 1, &lvicam->pad);
	if (err) {
		pr_err("[%s] : ERROR 4\n", __func__);
		v4l2_err(&lvicam->sd, "failed to init entity pads: %d", err);
		goto ctrl_cleanup;
	}

	err = v4l2_async_register_subdev_sensor_common(&lvicam->sd);
	if (err < 0) {
		pr_err("[%s] : ERROR 5\n", __func__);
		v4l2_err(&lvicam->sd, "failed to register subdev: %d", err);
		goto error_media_entity;
	}

	lvicam->fmt = tc358746_def_fmt;

	pr_info("[%s] Probe completed successfully\n", __func__);
	return 0;

error_media_entity:
	pr_err("[%s] : ERROR : error_media_entity\n", __func__);
	media_entity_cleanup(&lvicam->sd.entity);

ctrl_cleanup:
	pr_err("[%s] : ERROR : ctrl_cleanup\n", __func__);
	lvicam_ctrl_device_cleanup();

destroy_mutex:
	mutex_destroy(&lvicam->mutex);
	return err;
}

static int lvicam_remove(struct i2c_client *client)
{
	pr_info("[%s] Removing lvicam.\n", __func__);

	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lvicam *lvicam = to_lvicam(sd);

	pr_info("[%s] Resetting toshiba.\n", __func__);
	if (lvicam->reset_gpio)
		gpiod_set_value(lvicam->reset_gpio, 0);
	pr_info("[%s] De-asserting power pin.\n", __func__);
	if (lvicam->power_gpio)
		gpiod_set_value(lvicam->power_gpio, 0);
	pr_info("[%s] Setting the onoff gpio LOW.\n", __func__);
	if (lvicam->onoff_gpio)
		gpiod_set_value(lvicam->onoff_gpio, 0);

	v4l2_async_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);

	// Cleanup character device
	lvicam_ctrl_device_cleanup();

	mutex_destroy(&lvicam->mutex);

	return 0;
}

static const struct of_device_id __maybe_unused lvicam_of_match[] = {
	{ .compatible = "lvi,lvicam" },
	{},
};
MODULE_DEVICE_TABLE(of, lvicam_of_match);

static struct i2c_driver lvicam_driver = {
	.driver = {
		.name = "lvicam",
		.of_match_table = of_match_ptr(lvicam_of_match),
	},
	.probe_new = lvicam_probe,
	.remove = lvicam_remove,
};

module_i2c_driver(lvicam_driver);