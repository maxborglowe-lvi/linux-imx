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
#include <linux/workqueue.h>
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
#include <linux/notifier.h>
#include <linux/node.h>
#include <linux/debugfs.h>
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
	struct delayed_work camera_mode_work;
	struct delayed_work motion_detect_work;
	bool pending_camera_mode_apply;

	struct notifier_block lviconfig_nb;

	lvicam_cameramode_config_t camera_mode_config;
	enum color_mode color_mode;
	unsigned int active_mode_index;
	uint16_t saved_zoom[2]; /* per-mode saved zoom positions; 0 = use config default */
	int seesaw_override; /* -1 = use GPIO, 0/1 = forced mode */
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

} lvicam_controller;

lvicam_controller lvicam_ctrl;

static struct lvicam *lvicam = NULL;

static uint16_t zoom_step_increment = 0xFF; // Default step increment for zoom step commands. Calculate this based on ZoomSteps for specific camera modes.
static uint8_t zoom_step_increment_digital = 0x0F; // Default step increment for digital zoom step commands.

/* Forward declaration needed by cam_zoom_step for the digital->optical transition reset */
static int lvicam_fpga_visca_write(const visca_cmd_t *visca);

/* Forward declaration for debugfs/IOCTL trigger */
static void trigger_camera_mode_apply(struct lvicam *lv);
static void trigger_seesaw_toggle(struct lvicam *lv);

/* ############### LVI/VISCA COMMANDS FORWARD DECLARATIONS ################# */
static uint8_t lvicam_zoom_to_dzoom(uint16_t zoom, uint16_t max_zoom);
static uint8_t lvicam_visca_zoom_speed(uint16_t speed);

/* ############## END LVI/VISCA FORWARD DECLARATIONS ################# */

/* ############### BEGIN BASE COMMANDS #################*/

static const visca_cmd_t cmd_zoom = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_ZOOM, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_dzoom = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_DZOOM, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_dzoom_mode = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_DZOOM_MODE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_zoom_direct = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_ZOOM_DIRECT, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_zoom_direct_variable = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_ZOOM_DIRECT, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 10,
};

static const visca_cmd_t cmd_dzoom_direct = {
	.data = { 0x81, 0x01, 0x04, 0x46, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_r_gain_direct = {
	.data = { 0x81, 0x01, 0x04, 0x43, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_b_gain_direct = {
	.data = { 0x81, 0x01, 0x04, 0x44, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_wb_mode = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_WB, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_ae_mode = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_AE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_picture_effect = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_PICTURE_EFFECT, 0x00, 0xFF },
	.size = 6,
};

//CUSTOM REGISTER SETTING
static const visca_cmd_t cmd_register_set = {
	.data = { 0x81, 0x01, 0x04, 0x24, 0x00, 0x00, 0x00, 0xFF },
	.size = 8,
};

//VERTICAL FLIP
static const visca_cmd_t cmd_picture_flip = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_PICTURE_FLIP, 0x00, 0xFF },
	.size = 6,
};

//LEFT-RIGHT MIRRORING
static const visca_cmd_t cmd_lr_reverse = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_LR_REVERSE, 0x00, 0xFF },
	.size = 6,
};

//HIGH RESOLUTION MODE
static const visca_cmd_t cmd_hr = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_HR, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_monitoring_mode = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_MONITORING_MODE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_lvds_mode = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_LVDS_MODE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_nr = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_NR, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_nr_2D3D_independent = {
	.data = { 0x81, 0x01, CAM_NR_2D3D_INDEPENDENT, CAM_REG_NR, 0x00, 0x00, 0xFF },
	.size = 7,
};

//FOCUS STUFF
static const visca_cmd_t cmd_focus = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_FOCUS, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_focus_direct = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_FOCUS_DIRECT, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_focus_mode = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_FOCUS_MODE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_focus_one_push_trigger = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_FOCUS_ONE_PUSH_TRIGGER, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_focus_near_limit = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_FOCUS_NEAR_LIMIT, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_shutter = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_SHUTTER, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_shutter_direct = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_SHUTTER_DIRECT, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_aperture = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_APERTURE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_aperture_direct = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_APERTURE_DIRECT, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_exp_comp_onoff = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_EXP_COMP_ONOFF, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_exp_comp_setting = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_EXP_COMP_SETTING, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_exp_comp_direct = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_EXP_COMP_DIRECT, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 9,
};

static const visca_cmd_t cmd_freeze = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_FREEZE, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_md = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_MD, 0x00, 0xFF },
	.size = 6,
};

static const visca_cmd_t cmd_md_function_set = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_MD_FUNCTION_SET, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 11,
};

static const visca_cmd_t cmd_md_window_set = {
	.data = { 0x81, 0x01, 0x04, CAM_REG_MD_WINDOW_SET, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
	.size = 10,
};



static const visca_cmd_t cmd_cancel = {
	.data = { 0x81, 0x20, 0xFF },
	.size = 3,
};

//


/* ############### END BASE COMMANDS #################*/

/* ############### BEGIN CAMERA COMMANDS ################# */

static visca_cmd_t cam_zoom_direct(uint16_t zoom_value)
{
	visca_cmd_t cmd = cmd_zoom_direct;
	cmd.data[4] = (zoom_value >> 12) & 0x0F;
	cmd.data[5] = (zoom_value >> 8) & 0x0F;
	cmd.data[6] = (zoom_value >> 4) & 0x0F;
	cmd.data[7] = zoom_value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_zoom_direct_variable(uint16_t zoom_value)
{
	visca_cmd_t cmd = cmd_zoom_direct_variable;
	cmd.data[4] = lvicam->camera_mode_config.ZoomSpeed & 0x0F; // Zoom value is 16-bit, split into 4 nibbles
	cmd.data[5] = (zoom_value >> 12) & 0x0F;
	cmd.data[6] = (zoom_value >> 8) & 0x0F;
	cmd.data[7] = (zoom_value >> 4) & 0x0F;
	cmd.data[8] = zoom_value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_dzoom_direct(uint8_t zoom_value)
{
	visca_cmd_t cmd = cmd_dzoom_direct;
	cmd.data[6] = (zoom_value >> 4) & 0x0F;
	cmd.data[7] = zoom_value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_wb_mode(uint8_t mode){
	visca_cmd_t cmd = cmd_wb_mode;
	cmd.data[4] = mode & 0x0F;
	return cmd;
}

static visca_cmd_t cam_r_gain_direct(uint8_t value)
{
	visca_cmd_t cmd = cmd_r_gain_direct;

	cmd.data[6] = (value >> 4) & 0x0F;
	cmd.data[7] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_b_gain_direct(uint8_t value)
{
	visca_cmd_t cmd = cmd_b_gain_direct;

	cmd.data[6] = (value >> 4) & 0x0F;
	cmd.data[7] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_ae_mode(enum ae_mode mode){
	visca_cmd_t cmd = cmd_ae_mode;
	cmd.data[4] = mode & 0x0F;
	return cmd;
}

static visca_cmd_t cam_picture_effect(enum picture_effect_mode mode){
	visca_cmd_t cmd = cmd_picture_effect;
	cmd.data[4] = mode & 0xFF;
	return cmd;
}

static visca_cmd_t cam_monitoring_mode(enum monitoring_mode mode)
{
	visca_cmd_t cmd = cmd_register_set;
	cmd.data[4] = CAM_REG_MONITORING_MODE;
	cmd.data[5] = (mode >> 4) & 0x0F;
	cmd.data[6] = mode & 0x0F;
	return cmd;
}

static visca_cmd_t cam_lvds_mode(enum lvds_mode mode)
{
	visca_cmd_t cmd = cmd_register_set;
	cmd.data[4] = CAM_REG_LVDS_MODE;
	cmd.data[6] = mode & 0x0F;
	return cmd;
}

static visca_cmd_t cam_picture_flip(enum picture_flip_mode mode)
{
	visca_cmd_t cmd = cmd_picture_flip;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_lr_reverse(enum lr_reverse_mode mode)
{
	visca_cmd_t cmd = cmd_lr_reverse;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_hr(enum hr_mode mode){
	visca_cmd_t cmd = cmd_hr;
	cmd.data[4] = mode & 0x0F;
	return cmd;
}

static visca_cmd_t cam_focus(enum focus mode)
{
	visca_cmd_t cmd = cmd_focus;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_focus_variable(enum focus mode, uint8_t value)
{
	visca_cmd_t cmd = cmd_focus;
	cmd.data[4] = (mode | value) & 0xFF;
	return cmd;
};

static visca_cmd_t cam_focus_direct(uint16_t focus_value)
{
	visca_cmd_t cmd = cmd_focus_direct;
	cmd.data[4] = (focus_value >> 12) & 0x0F;
	cmd.data[5] = (focus_value >> 8) & 0x0F;
	cmd.data[6] = (focus_value >> 4) & 0x0F;
	cmd.data[7] = focus_value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_focus_mode(enum focus_mode mode)
{
	visca_cmd_t cmd = cmd_focus_mode;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_focus_one_push_trigger(void)
{
	visca_cmd_t cmd = cmd_focus_one_push_trigger;
	cmd.data[4] = FOCUS_ONE_PUSH_TRIGGER & 0x0F;
	return cmd;
};

static visca_cmd_t cam_focus_near_limit(uint16_t value)
{
	visca_cmd_t cmd = cmd_focus_near_limit;
	cmd.data[4] = (value >> 12) & 0x0F;
	cmd.data[5] = (value >> 8) & 0x0F;
	cmd.data[6] = (value >> 4) & 0x0F;
	cmd.data[7] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_shutter(enum shutter mode)
{
	visca_cmd_t cmd = cmd_shutter;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_shutter_direct(uint8_t value)
{
	visca_cmd_t cmd = cmd_shutter_direct;
	cmd.data[6] = (value >> 4) & 0x0F;
	cmd.data[7] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_aperture(enum aperture mode)
{
	visca_cmd_t cmd = cmd_aperture;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_aperture_direct(uint8_t value)
{
	visca_cmd_t cmd = cmd_aperture_direct;
	cmd.data[6] = (value >> 4) & 0x0F;
	cmd.data[7] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_exp_comp_onoff(enum exp_comp_onoff mode)
{
	visca_cmd_t cmd = cmd_exp_comp_onoff;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_exp_comp_setting(enum exp_comp_setting mode)
{
	visca_cmd_t cmd = cmd_exp_comp_setting;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_exp_comp_direct(uint8_t value)
{
	visca_cmd_t cmd = cmd_exp_comp_direct;
	cmd.data[6] = (value >> 4) & 0x0F;
	cmd.data[7] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_nr(enum nr_mode mode)
{
	visca_cmd_t cmd = cmd_nr;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_nr_2D3D_independent(uint8_t mode)
{
	visca_cmd_t cmd = cmd_nr_2D3D_independent;
	cmd.data[4] = (mode >> 4) & 0x0F;
	cmd.data[5] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_focus_speed(enum af_speed mode)
{
	visca_cmd_t cmd = cmd_register_set;
	cmd.data[4] = CAM_REG_AF_SPEED;
	cmd.data[6] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_zoom(enum zoom_mode mode, uint8_t value)
{
	visca_cmd_t cmd = cmd_zoom;
	cmd.data[4] = mode & 0x0F;
	if(mode == ZOOM_TELE_VARIABLE || mode == ZOOM_WIDE_VARIABLE)
		cmd.data[4] = mode | (value & 0x0F);
	return cmd;
};

static visca_cmd_t cam_freeze(enum freeze_mode mode)
{
	visca_cmd_t cmd = cmd_freeze;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_cancel(uint8_t socket){
	visca_cmd_t cmd = cmd_cancel;
	cmd.data[1] |= (socket & 0x0F);
	return cmd;
};

static visca_cmd_t cam_md(enum md_mode mode)
{
	visca_cmd_t cmd = cmd_md;
	cmd.data[4] = mode & 0x0F;
	return cmd;
};

static visca_cmd_t cam_md_function_set(enum md_display_mode display_mode, enum md_detection_frame detection_frame, uint8_t threshold, uint8_t interval)
{
	visca_cmd_t cmd = cmd_md_function_set;
	cmd.data[4] = display_mode & 0x0F;
	cmd.data[5] = detection_frame & 0x0F;
	cmd.data[6] = (threshold >> 4) & 0x0F;
	cmd.data[7] = (threshold & 0x0F);
	cmd.data[8] = (interval >> 4) & 0x0F;
	cmd.data[9] = (interval & 0x0F);
	return cmd;
};

static visca_cmd_t cam_md_window_set(enum md_select_detection_frame frame, uint8_t start_hor_pos, uint8_t start_ver_pos, uint8_t stop_hor_pos, uint8_t stop_ver_pos)
{
	visca_cmd_t cmd = cmd_md_window_set;
	cmd.data[4] = frame & 0x0F;
	cmd.data[5] = start_hor_pos & 0x3C;
	cmd.data[6] = start_ver_pos & 0x28;
	if(stop_hor_pos < 0) stop_hor_pos = 1;
	cmd.data[7] = stop_hor_pos & 0x3C;
	if(stop_ver_pos < 0) stop_ver_pos = 1;
	cmd.data[8] = stop_ver_pos & 0x28;
	return cmd;
};

static visca_cmd_t cam_zoom_absolute(uint16_t zoom)
{
	uint16_t prev_zoom = lvicam->camera_mode_config.Zoom;
	uint16_t min_zoom = lvicam->camera_mode_config.ZoomMin;
	uint16_t max_zoom = lvicam->camera_mode_config.ZoomMax;
	uint8_t prev_dzoom = lvicam->camera_mode_config.DZoom;
	uint8_t dzoom = 0;

	if (zoom < min_zoom)
		zoom = min_zoom;
	if (zoom > max_zoom)
		zoom = max_zoom;

	if (zoom > ZOOM_LIMIT_OPTICAL_MAX && max_zoom > ZOOM_LIMIT_OPTICAL_MAX) {
		if (prev_dzoom == 0 && prev_zoom < ZOOM_LIMIT_OPTICAL_MAX) {
			lvicam->camera_mode_config.Zoom = ZOOM_LIMIT_OPTICAL_MAX;
			return cam_zoom_direct_variable(ZOOM_LIMIT_OPTICAL_MAX);
		}

		dzoom = lvicam_zoom_to_dzoom(zoom, max_zoom);
		lvicam->camera_mode_config.Zoom = zoom;
		lvicam->camera_mode_config.DZoom = dzoom;

		return cam_dzoom_direct(dzoom);
	}

	if (prev_dzoom != 0) {
		visca_cmd_t dzoom_reset = cam_dzoom_direct(0);

		lvicam_fpga_visca_write(&dzoom_reset);
		lvicam->camera_mode_config.DZoom = 0;
	}

	if (zoom > ZOOM_LIMIT_OPTICAL_MAX)
		zoom = ZOOM_LIMIT_OPTICAL_MAX;

	lvicam->camera_mode_config.Zoom = zoom;
	return cam_zoom_direct_variable(zoom);
}

static visca_cmd_t cam_zoom_step(uint8_t dir)
{
	uint16_t zoom;
	uint16_t min_zoom = lvicam->camera_mode_config.ZoomMin;
	uint16_t max_zoom = lvicam->camera_mode_config.ZoomMax;

	zoom = lvicam->camera_mode_config.Zoom;

	if (!zoom_step_increment)
		zoom_step_increment = 0xff;

	if (dir == VISCA_CMD_ZOOM_STEP_INC) {
		if (zoom >= max_zoom || max_zoom - zoom < zoom_step_increment)
			zoom = max_zoom;
		else
			zoom += zoom_step_increment;
	} else {
		if (zoom <= min_zoom || zoom - min_zoom < zoom_step_increment)
			zoom = min_zoom;
		else
			zoom -= zoom_step_increment;
	}

	return cam_zoom_absolute(zoom);
};

static visca_cmd_t cam_dzoom(uint8_t value)
{
	visca_cmd_t cmd = cmd_dzoom;

	cmd.data[4] = value;
	return cmd;
};

static visca_cmd_t cam_dzoom_mode(uint8_t value)
{
	visca_cmd_t cmd = cmd_dzoom_mode;

	/* Sony VISCA: 0x00 = Combine mode, 0x01 = Separate mode */
	cmd.data[4] = value;
	return cmd;
};

static visca_cmd_t cam_register_set(uint8_t reg, uint8_t value)
{
	visca_cmd_t cmd = cmd_register_set;
	cmd.data[4] = reg; // Register address
	cmd.data[5] = (value >> 4) & 0x0F; // Value to set
	cmd.data[6] = value & 0x0F;
	return cmd;
};

static visca_cmd_t cam_reg_wide_limit(uint8_t value)
{
	// This is a specific register set command for setting the wide limit, which is a common operation
	return cam_register_set(0x50, value);
};

static visca_cmd_t cam_reg_tele_limit(uint8_t value)
{
	// This is a specific register set command for setting the tele limit, which is a common operation
	return cam_register_set(0x51, value);
};


/* ############### END CAMERA COMMANDS ################# */


/* ############## BEGIN LVI/VISCA HELPERS ################# */
static uint8_t lvicam_zoom_to_dzoom(uint16_t zoom, uint16_t max_zoom)
{
	u32 digital_span = max_zoom - ZOOM_LIMIT_OPTICAL_MAX;
	u32 digital_pos = zoom - ZOOM_LIMIT_OPTICAL_MAX;

	if (!digital_span)
		return 0;

	if (digital_pos > digital_span)
		digital_pos = digital_span;

	return (uint8_t)((digital_pos * ZOOM_LIMIT_DIGITAL_MAX) / digital_span);
}

static uint8_t lvicam_visca_zoom_speed(uint16_t speed)
{
	uint8_t visca_speed = (uint8_t)(speed & 0x0F);

	if (visca_speed > 0x07)
		visca_speed = 0x07;

	return visca_speed;
}

/* ############## END LVI/VISCA HELPERS ################# */

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
	i2c_wr16(sd, 0x021C, 0x0005); // TCLK_TRAILCNT
	i2c_wr16(sd, 0x021E, 0x0000);
	i2c_wr16(sd, 0x0220, 0x0705); // THS_HEADERCNT
	i2c_wr16(sd, 0x0222, 0x0000);
	i2c_wr16(sd, 0x0224, 0x4E20); // TWAKEUPCNT
	i2c_wr16(sd, 0x0226, 0x0000);
	i2c_wr16(sd, 0x0228, 0x000C); // TCLK_POSTCNT
	i2c_wr16(sd, 0x022A, 0x0000);
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
	fie->interval.numerator = 1;
	fie->interval.denominator = 60;

	return 0;
}

static int lvicam_g_frame_interval(struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *fi)
{
	pr_info("[%s] call\n", __func__);

	fi->interval.numerator = 1;
	fi->interval.denominator = 60;

	return 0;
}

static int lvicam_s_frame_interval(struct v4l2_subdev *sd, struct v4l2_subdev_frame_interval *fi)
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

/**
 * fpga_i2c_write - Write bytes directly to an FPGA register
 * @reg:  Register address (e.g. 0x14 for zoomMin)
 * @data: Pointer to data bytes
 * @len:  Number of data bytes (excluding the register byte)
 *
 * Sends: [reg, data[0], data[1], ..., data[len-1]]
 */
static int fpga_i2c_write(uint8_t reg, const uint8_t *data, size_t len)
{
	uint8_t buf[I2C_MAX_XFER_SIZE];
	struct i2c_msg msg;
	int ret;

	if (1 + len > sizeof(buf))
		return -EINVAL;

	buf[0] = reg;
	memcpy(&buf[1], data, len);

	msg.addr = lvicam_ctrl.client->addr;
	msg.flags = 0;
	msg.len = 1 + len;
	msg.buf = buf;

	ret = i2c_transfer(lvicam_ctrl.adapter, &msg, 1);
	if (ret != 1) {
		pr_err("[%s] i2c write to reg 0x%02X failed: %d\n", __func__, reg, ret);
		return -EIO;
	}

	return 0;
}

/**
 * fpga_i2c_read - Read one byte from an FPGA register
 * @reg: Register address to read from
 * @val: Pointer to store the read byte
 */
static int fpga_i2c_read(uint8_t reg, uint8_t *val)
{
	uint8_t reg_buf = reg;
	struct i2c_msg msgs[2];
	int ret;

	msgs[0].addr = lvicam_ctrl.client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;

	msgs[1].addr = lvicam_ctrl.client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = val;

	ret = i2c_transfer(lvicam_ctrl.adapter, msgs, 2);
	if (ret != 2) {
		pr_err("[%s] i2c read from reg 0x%02X failed: %d\n", __func__, reg, ret);
		return -EIO;
	}

	return 0;
}

/**
 * fpga_i2c_read_multi - Read multiple bytes from an FPGA register
 * @reg: Register address to read from
 * @data: Pointer to store the read bytes
 * @len: Number of bytes to read
 */
static int fpga_i2c_read_multi(uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t reg_buf = reg;
	struct i2c_msg msgs[2];
	int ret;

	if (len > I2C_MAX_XFER_SIZE)
		return -EINVAL;

	msgs[0].addr = lvicam_ctrl.client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg_buf;

	msgs[1].addr = lvicam_ctrl.client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	ret = i2c_transfer(lvicam_ctrl.adapter, msgs, 2);
	if (ret != 2) {
		pr_err("[%s] i2c read %zu bytes from reg 0x%02X failed: %d\n", __func__, len, reg, ret);
		return -EIO;
	}

	return 0;
}

#define NETWORK_CHANGE_TIMEOUT_MS 5000

/**
 * lvicam_fpga_wait_network_change - Poll FPGA_FLAGS_INIT_STATUS_REG until the
 * network_change bit (bit 3) is set, indicating the FPGA is ready to accept
 * VISCA commands.  Times out after NETWORK_CHANGE_TIMEOUT_MS milliseconds.
 */
static int lvicam_fpga_wait_network_change(void)
{
	static bool network_change_ready;
	unsigned long timeout = jiffies + msecs_to_jiffies(NETWORK_CHANGE_TIMEOUT_MS);
	FPGAFlagsInit_t flags;
	uint8_t raw = 0;
	int ret;

	if (network_change_ready)
		return 0;

	do {
		ret = fpga_i2c_read(FPGA_FLAGS_INIT_STATUS_REG, &raw);
		if (ret)
			return ret;

		memcpy(&flags, &raw, sizeof(flags));

		pr_info("[lvicam] FPGA_FLAGS_INIT_STATUS_REG (0x%02X): "
			"visca_enable=%u visca_ack=%u visca_error=%u "
			"motion_detect_alarm=%u network_change=%u "
			"system_camera_pll_lock=%u\n",
			raw, flags.visca_enable, flags.visca_ack, flags.visca_error, flags.motion_detect_alarm, flags.network_change, flags.system_camera_pll_lock);

		if (flags.network_change) {
			pr_info("[lvicam] network_change set, waiting 10s before sending VISCA commands\n");
			msleep(5000);
			network_change_ready = true;
			return 0;
		}

		usleep_range(1000, 2000);
	} while (time_before(jiffies, timeout));

	pr_err("[lvicam] Timeout waiting for FPGA network_change bit (flags=0x%02X)\n", raw);
	return -ETIMEDOUT;
}

#define VISCA_ACK_TIMEOUT_MS 2000

/**
 * lvicam_fpga_wait_visca_ack - Poll FPGA_FLAGS_INIT_STATUS_REG until the
 * visca_ack bit is set, indicating the FPGA has received the camera's ACK
 * and is ready to accept the next VISCA command.
 */
static int lvicam_fpga_wait_visca_ack(void)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(VISCA_ACK_TIMEOUT_MS);
	FPGAFlagsInit_t flags;
	uint8_t raw = 0;
	bool saw_visca_error = false;
	int ret;

	/* Wait 20ms after the VISCA command was written before polling,
	 * to avoid congesting the FPGA while it processes the command. */
	// msleep(2);

	do {
		ret = fpga_i2c_read(FPGA_FLAGS_INIT_STATUS_REG, &raw);
		if (ret)
			return ret;

		memcpy(&flags, &raw, sizeof(flags));

		if (flags.visca_enable) {
			pr_info("[lvicam] FPGA visca_enable set (flags=0x%02X)\n", raw);
			return 0;
		}

		if (flags.visca_error && !saw_visca_error) {
			/* visca_error appears to be sticky in this firmware, so keep
			 * waiting for visca_enable instead of aborting or returning early.
			 */
			pr_warn("[lvicam] FPGA visca_error set while waiting for visca_enable (flags=0x%02X)\n", raw);
			saw_visca_error = true;
		}

		usleep_range(2500, 5000);
	} while (time_before(jiffies, timeout));

	/* Some firmware revisions do not surface a reliable visca_enable bit.
	 * Preserve command sequencing by waiting up to the timeout, then allow
	 * the next command to proceed instead of aborting the whole sequence.
	 */
	pr_warn("[lvicam] visca_enable not set after timeout (flags=0x%02X), continuing\n", raw);
	return 0;
}

/**
 * fpga_visca_write - Write a VISCA command via the FPGA's embedded VISCA register (0x7A)
 * @visca_cmd: Pointer to raw VISCA command bytes (e.g. {0x81, 0x01, 0x04, reg, msb, lsb, 0xFF})
 * @cmd_len:   Number of bytes in the VISCA command
 *
 * Wire format: [0x7A, cmd_len, visca_cmd[0], ..., visca_cmd[cmd_len-1]]
 */
static int lvicam_fpga_visca_write(const visca_cmd_t *visca)
{
	uint8_t buf[I2C_MAX_XFER_SIZE];
	struct i2c_msg msg;
	int ret;
	char hexbuf[(VISCA_MAX_CMD_SIZE * 3) + 1];
	int i;
	int pos;

	ret = lvicam_fpga_wait_network_change();
	if (ret)
		return ret;

	/* 1 byte reg (0x7A) + 1 byte length + cmd_len */
	if (2 + visca->size > sizeof(buf))
		return -EINVAL;

	buf[0] = VISCA_COMMAND_REG;
	buf[1] = (uint8_t)visca->size;
	memcpy(&buf[2], visca->data, visca->size);

	pos = 0;
	for (i = 0; i < visca->size && i < VISCA_MAX_CMD_SIZE; i++)
		pos += scnprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X ", visca->data[i]);

	pr_info("[%s] VISCA cmd len=%zu: %s\n", __func__, visca->size, hexbuf);

	msg.addr = lvicam_ctrl.client->addr;
	msg.flags = 0;
	msg.len = 2 + visca->size;
	msg.buf = buf;

	ret = i2c_transfer(lvicam_ctrl.adapter, &msg, 1);
	if (ret != 1) {
		pr_err("[%s] VISCA i2c write failed: %d\n", __func__, ret);
		return -EIO;
	}

	msleep(10);

	return lvicam_fpga_wait_visca_ack();
}

static int lvicam_fpga_write_word(uint8_t reg, u16 value)
{
	uint8_t data[2] = {
		(uint8_t)(value >> 8),
		(uint8_t)(value & 0xFF),
	};

	return fpga_i2c_write(reg, data, sizeof(data));
}

/**
 * lvicam_fpga_read_zoom_pos - Read camera 1 zoom position from FPGA register
 * @zoom_pos: Pointer to store the 16-bit zoom position value
 *
 * Reads the CAM1_ZOOM_POS_STATUS_REG (0xC0), which is maintained by the
 * FPGA's embedded VISCA engine and reflects the current camera zoom position.
 */
static int lvicam_fpga_read_zoom_pos(uint16_t *zoom_pos)
{
	uint8_t hi, lo;
	int ret;

	/*
	 * The FPGA does not support multi-byte reads from data registers.
	 * Read the two adjacent 8-bit sub-registers separately and
	 * combine them as a big-endian 16-bit value.
	 */
	ret = fpga_i2c_read(CAM1_ZOOM_POS_STATUS_REG, &hi);
	if (ret)
		return ret;

	ret = fpga_i2c_read(CAM1_ZOOM_POS_STATUS_REG + 1, &lo);
	if (ret)
		return ret;

	*zoom_pos = ((uint16_t)hi << 8) | lo;

	return 0;
}

#define ZOOM_POLL_INTERVAL_MS 100
#define ZOOM_POLL_TIMEOUT_MS 10000

/**
 * lvicam_fpga_wait_camcmd_complete - Poll FPGA bit 6 (visca_ack / CamCmdCompleteFlag)
 *                                    until set, indicating the camera has finished
 *                                    processing the last VISCA command.
 *
 * Return: 0 on success, -ETIMEDOUT if bit never set, or negative errno from I2C.
 */
static int lvicam_fpga_wait_camcmd_complete(void)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(ZOOM_POLL_TIMEOUT_MS);
	FPGAFlagsInit_t flags;
	uint8_t raw;
	int ret;

	do {
		msleep(ZOOM_POLL_INTERVAL_MS);

		ret = fpga_i2c_read(FPGA_FLAGS_INIT_STATUS_REG, &raw);
		if (ret)
			return ret;

		memcpy(&flags, &raw, sizeof(flags));

		pr_info("[lvicam] Waiting for CamCmdComplete... flags=0x%02X (ack=%u err=%u ena=%u)\n",
			raw, flags.visca_ack, flags.visca_error, flags.visca_enable);

		if (flags.visca_ack) {
			pr_info("[lvicam] CamCmdComplete (visca_ack) set (flags=0x%02X)\n", raw);
			return 0;
		}
	} while (time_before(jiffies, timeout));

	pr_warn("[lvicam] CamCmdComplete timeout (flags=0x%02X)\n", raw);
	return -ETIMEDOUT;
}

static bool lvicam_is_camera_mode_param(const ConfigParam *param)
{
	return param == &confCameraMode[0].Zoom || param == &confCameraMode[0].ZoomMin || param == &confCameraMode[0].ZoomMax || param == &confCameraMode[0].ZoomSpeed ||
	       param == &confCameraMode[0].Focus || param == &confCameraMode[0].FocusMin || param == &confCameraMode[0].FocusMax || param == &confCameraMode[0].FocusPos ||
	       param == &confCameraMode[0].FocusAFSpeed || param == &confCameraMode[0].FocusAFMode ||
	       param == &confCameraMode[0].NaturalColorExposure || param == &confCameraMode[0].ArtificialColorExposure || param == &confCameraMode[0].WhiteBalance ||
		   param == &confCameraMode[0].PictureEffect;
}

static void lvicam_load_camera_mode(lvicam_cameramode_config_t *config, unsigned int mode_index)
{
	uint8_t *p;

	p = confCameraMode[mode_index].Zoom.data;
	config->Zoom = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].ZoomMin.data;
	config->ZoomMin = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].ZoomMax.data;
	config->ZoomMax = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].ZoomSpeed.data;
	config->ZoomSpeed = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].Focus.data;
	config->Focus = p[0];

	p = confCameraMode[mode_index].FocusMin.data;
	config->FocusMin = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].FocusMax.data;
	config->FocusMax = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].FocusPos.data;
	config->FocusPos = p[0] | (p[1] << 8);

	p = confCameraMode[mode_index].FocusAFSpeed.data;
	config->FocusAFSpeed = p[0];

	p = confCameraMode[mode_index].FocusAFMode.data;
	config->FocusAFMode = p[0];

	p = confCameraMode[mode_index].NaturalColorExposure.data;
	config->NaturalColorExposure = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);

	p = confCameraMode[mode_index].ArtificialColorExposure.data;
	config->ArtificialColorExposure = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);

	p = confCameraMode[mode_index].WhiteBalance.data;
	config->WhiteBalance = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);

	p = confCameraMode[mode_index].PictureEffect.data;
	config->PictureEffect = p[0];

	p = confCameraMode[mode_index].NoiseReduction2D3D.data;
	config->NoiseReduction2D3D = p[0];
	config->NoiseReduction2D = (p[0] >> 4) & 0x0F;
	config->NoiseReduction3D = p[0] & 0x0F;

	config->DZoomOnOff = 1;
	config->DZoomMode = 1;
	config->ZoomSteps = 32;
	config->DZoom = 0;

	config->MDEnable = confCamera.MDEnable.data[0];
	config->MDThreshold = confCamera.MDThreshold.data[0];
	config->MDIntervalTime = confCamera.MDIntervalTime.data[0];
	config->MDStartPos = (confCamera.MDStartPos.data[1] << 8) | confCamera.MDStartPos.data[0];
	config->MDStopPos = (confCamera.MDStopPos.data[1] << 8) | confCamera.MDStopPos.data[0];

	zoom_step_increment = ZOOM_LIMIT_OPTICAL_MAX / config->ZoomSteps;
	zoom_step_increment_digital = ZOOM_LIMIT_DIGITAL_MAX / config->ZoomSteps;
}

static int lvicamera_apply_visca_init(struct lvicam *lvicam_device, unsigned int mode_index)
{
	int ret = 0;
	uint8_t r_gain = 0;
	uint8_t b_gain = 0;
	uint8_t natcol_exp_comp = 0;
	uint8_t picture_effect = 0;
	uint8_t *p;
	visca_cmd_t visca;

	if (!lvicam_device) {
		pr_warn("[%s] no device\n", __func__);
		return -EINVAL;
	}

	if (!confCamera.RGain.data || !confCamera.BGain.data) {
		pr_warn("[%s] RGain/BGain data pointer NULL\n", __func__);
		return -EINVAL;
	}

	if (!lvicam_ctrl.adapter || !lvicam_ctrl.client) {
		pr_warn("[%s] I2C not ready\n", __func__);
		return -ENODEV;
	}

	{
		uint8_t pic_mode = 0;
		if (fpga_i2c_read(PICTURE_MODE_STATUS_REG, &pic_mode) == 0) {
			lvicam_device->color_mode = (pic_mode & PICTURE_MODE_NAT_ON)
				? COLOR_MODE_NATURAL : COLOR_MODE_ARTIFICIAL;
		} else {
			lvicam_device->color_mode = COLOR_MODE_NATURAL;
		}
		pr_info("[%s] Initial color mode: %s (reg 0xCC = 0x%02X)\n",
			__func__,
			lvicam_device->color_mode == COLOR_MODE_NATURAL ? "natural" : "artificial",
			pic_mode);
	}

	if (lvicam_device->camera_mode_config.Zoom != 0)
		lvicam_device->saved_zoom[lvicam_device->active_mode_index] = lvicam_device->camera_mode_config.Zoom;
	lvicam_device->active_mode_index = mode_index;

	mutex_lock(&lvicam_device->mutex);

		p = confCameraMode[mode_index].PictureEffect.data;
		picture_effect = p[0];

		// visca = cam_cancel(2);
		// ret = lvicam_fpga_visca_write(&visca);
		// pr_info("[%s] Zoom stop sent\n", __func__);
		// if (ret)
		// 	goto out_unlock;

		visca = cam_dzoom_mode(1);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_dzoom(DZOOM_ON);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_picture_effect(picture_effect);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_picture_flip(PICTURE_FLIP_OFF);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_lr_reverse(LR_REVERSE_OFF);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_hr(HR_ON);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		p = confCameraMode[mode_index].FocusMin.data;
		uint16_t focus_min = p[0] | (p[1] << 8);
		visca = cam_focus_near_limit(focus_min);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		p = confCameraMode[mode_index].FocusAFMode.data;
		uint8_t focus_af_mode = p[0];
		visca = cam_focus_mode(focus_af_mode);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		p = confCameraMode[mode_index].FocusAFSpeed.data;
		uint8_t focus_af_speed = p[0];
		visca = cam_focus_speed(focus_af_speed);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		p = confCameraMode[mode_index].FocusPos.data;
		uint16_t focus_pos_val = p[0] | (p[1] << 8);
		p = confCameraMode[mode_index].FocusMin.data;
		uint16_t fmin = p[0] | (p[1] << 8);
		p = confCameraMode[mode_index].FocusMax.data;
		uint16_t fmax = p[0] | (p[1] << 8);

		pr_info("[%s] Focus: mode=0x%02X, speed=0x%02X, near_limit=0x%04X, pos=0x%04X, min=0x%04X, max=0x%04X\n",
			__func__, focus_af_mode, focus_af_speed, focus_min,
			focus_pos_val, fmin, fmax);

		if (focus_af_mode != FOCUS_MODE_AUTO) {
			if (fmin <= fmax) {
				if (focus_pos_val < fmin)
					focus_pos_val = fmin;
				if (focus_pos_val > fmax)
					focus_pos_val = fmax;
			} else {
				if (focus_pos_val < fmin)
					focus_pos_val = fmin;
			}
			pr_info("[%s] Focus direct position: 0x%04X\n", __func__, focus_pos_val);
			visca = cam_focus_direct(focus_pos_val);
			ret = lvicam_fpga_visca_write(&visca);
			if (ret)
				goto out_unlock;
		} else {
			pr_info("[%s] Focus in auto mode, skipping direct position\n", __func__);
		}

		p = confCameraMode[mode_index].NaturalColorExposure.data;
		uint8_t natural_color_exposure = p[0];
		p = confCameraMode[mode_index].ArtificialColorExposure.data;
		uint8_t artificial_color_exposure = p[0];

		uint8_t ae_value = (lvicam_device->color_mode == COLOR_MODE_NATURAL)
			? natural_color_exposure : artificial_color_exposure;
		visca = cam_ae_mode(ae_value);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		if (confCamera.MonitoringMode.data) {
			visca = cam_monitoring_mode((enum monitoring_mode)*confCamera.MonitoringMode.data);
			ret = lvicam_fpga_visca_write(&visca);
			if (ret)
				goto out_unlock;
		}

		if (confCamera.LVDSMode.data) {
			visca = cam_lvds_mode((enum lvds_mode)*confCamera.LVDSMode.data);
			ret = lvicam_fpga_visca_write(&visca);
			if (ret)
				goto out_unlock;
		}

		pr_info("[%s] Calculated zoom step increment: %u\n", __func__, zoom_step_increment);

		r_gain = *confCamera.RGain.data;
		b_gain = *confCamera.BGain.data;
		natcol_exp_comp = (lvicam_device->color_mode == COLOR_MODE_NATURAL)
			? natural_color_exposure : artificial_color_exposure;

		visca = cam_wb_mode(WB_MODE_MANUAL);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_exp_comp_onoff(EXP_COMP_ON);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		
		visca = cam_exp_comp_direct(natcol_exp_comp);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_r_gain_direct(r_gain);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		visca = cam_b_gain_direct(b_gain);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		p = confCameraMode[mode_index].NoiseReduction2D3D.data;
		uint8_t nr_2d = (p[0] >> 4) & 0x0F;
		uint8_t nr_3d = p[0] & 0x0F;
		visca = cam_nr_2D3D_independent((nr_2d << 4) | nr_3d);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		p = confCameraMode[mode_index].Zoom.data;
		uint16_t cfg_zoom = p[0] | (p[1] << 8);
		p = confCameraMode[mode_index].ZoomMin.data;
		uint16_t cfg_zoom_min = p[0] | (p[1] << 8);
		p = confCameraMode[mode_index].ZoomMax.data;
		uint16_t cfg_zoom_max = p[0] | (p[1] << 8);
		p = confCameraMode[mode_index].ZoomSpeed.data;
		uint16_t cfg_zoom_speed = p[0] | (p[1] << 8);

		uint16_t zoom_to_apply = (lvicam_device->saved_zoom[mode_index] != 0)
			? lvicam_device->saved_zoom[mode_index] : cfg_zoom;
		bool zoom_from_saved = (lvicam_device->saved_zoom[mode_index] != 0);

		if (zoom_to_apply < cfg_zoom_min)
			zoom_to_apply = cfg_zoom_min;
		if (zoom_to_apply > cfg_zoom_max)
			zoom_to_apply = cfg_zoom_max;


		uint16_t zoom_speed_saved = lvicam_device->camera_mode_config.ZoomSpeed;

		visca = cam_zoom_direct_variable(zoom_to_apply);
		ret = lvicam_fpga_visca_write(&visca);
		pr_info("[%s] Zoom direct speed %d: 0x%04X\n", __func__, zoom_speed_saved, zoom_to_apply);
		if (ret)
			goto out_unlock;

		lvicam_device->camera_mode_config.ZoomSpeed = zoom_speed_saved;
		visca = cam_zoom_direct_variable(zoom_to_apply);
		ret = lvicam_fpga_visca_write(&visca);
		if (ret)
			goto out_unlock;

		lvicam_fpga_wait_camcmd_complete();

		/* Motion Detection - Frame 0 */
		uint8_t md_enable = confCamera.MDEnable.data[0];
		uint8_t md_threshold = confCamera.MDThreshold.data[0];
		uint8_t md_interval = confCamera.MDIntervalTime.data[0];
		uint8_t md_pp = confCamera.MDStartPos.data[1];
		uint8_t md_qq = confCamera.MDStartPos.data[0];
		uint8_t md_rr = confCamera.MDStopPos.data[1];
		uint8_t md_ss = confCamera.MDStopPos.data[0];

		// visca = cam_md(md_enable ? MD_MODE_ON : MD_MODE_OFF);
		// ret = lvicam_fpga_visca_write(&visca);
		// if (ret)
		// 	goto out_unlock;

		// visca = cam_md_function_set(MD_DISPLAY_MODE_ON,
		// 			    MD_DETECTION_FRAME0,
		// 			    md_threshold,
		// 			    md_interval);
		// ret = lvicam_fpga_visca_write(&visca);r
		// if (ret)
		// 	goto out_unlock;

		// visca = cam_md_window_set(MD_SELECT_DETECTION_FRAME0,
		// 			   md_pp, md_qq, md_rr, md_ss);
		// ret = lvicam_fpga_visca_write(&visca);
		// if (ret)
		// 	goto out_unlock;

		

		p = confCameraMode[mode_index].Focus.data;
		uint8_t cfg_focus = p[0];
		p = confCameraMode[mode_index].ArtificialColorExposure.data;
		uint8_t cfg_art_col_exp = p[0];
		p = confCameraMode[mode_index].WhiteBalance.data;
		uint8_t cfg_wb = p[0];

		pr_info("[%s] Applied mode %u: Zoom=0x%04X%s Min=0x%04X Max=0x%04X Speed=0x%04X\n",
			__func__, mode_index, zoom_to_apply,
			zoom_from_saved ? " (saved)" : "",
			cfg_zoom_min, cfg_zoom_max, cfg_zoom_speed);
		pr_info("[%s] Applied mode %u: Focus=0x%02X Pos=0x%04X Min=0x%04X Max=0x%04X\n",
			__func__, mode_index, cfg_focus, focus_pos_val, fmin, fmax);
		pr_info("[%s] Applied mode %u: FocusAFMode=0x%02X AFSpeed=0x%02X\n",
			__func__, mode_index, focus_af_mode, focus_af_speed);
		pr_info("[%s] Applied mode %u: NatColExp=0x%02X ArtColExp=0x%02X WB=0x%02X PicEffect=0x%02X (active=%s)\n",
			__func__, mode_index, natural_color_exposure, cfg_art_col_exp, cfg_wb, picture_effect,
			lvicam_device->color_mode == COLOR_MODE_NATURAL ? "natural" : "artificial");
		pr_info("[%s] Applied mode %u: NoiseReduction2D3D=0x%02X (2D=0x%X 3D=0x%X)\n",
			__func__, mode_index, (nr_2d << 4) | nr_3d, nr_2d, nr_3d);
		pr_info("[%s] Applied color gains: RGain=0x%02X BGain=0x%02X\n", __func__, r_gain, b_gain);
		pr_info("[%s] Applied exposure compensation: %d\n", __func__, EXP_COMP_ON);
		pr_info("[%s] Applied exposure compensation setting: %d (%s)\n", __func__,
			(uint8_t)natcol_exp_comp,
			lvicam_device->color_mode == COLOR_MODE_NATURAL ? "natural" : "artificial");
		pr_info("[%s] MD: enable=%u threshold=0x%02X interval=%u start=(0x%02X,0x%02X) stop=(0x%02X,0x%02X)\n",
			__func__,
			confCamera.MDEnable.data[0],
			confCamera.MDThreshold.data[0],
			confCamera.MDIntervalTime.data[0],
			confCamera.MDStartPos.data[1],
			confCamera.MDStartPos.data[0],
			confCamera.MDStopPos.data[1],
			confCamera.MDStopPos.data[0]);

	lvicam_load_camera_mode(&lvicam_device->camera_mode_config, mode_index);
	lvicam_device->camera_mode_config.Zoom = zoom_to_apply;
	lvicam_device->camera_mode_config.DZoom = 0;

out_unlock:
	mutex_unlock(&lvicam_device->mutex);
	return ret;
}

static bool lvicam_param_needs_apply(const ConfigParam *param)
{
	return param == &confCameraMode[0].Zoom || param == &confCameraMode[0].ZoomMin || param == &confCameraMode[0].ZoomMax || param == &confCameraMode[0].ZoomSpeed ||
	       param == &confCameraMode[0].Focus || param == &confCameraMode[0].FocusMin || param == &confCameraMode[0].FocusMax || param == &confCameraMode[0].FocusPos ||
	       param == &confCameraMode[0].FocusAFSpeed || param == &confCameraMode[0].FocusAFMode ||
	       param == &confCameraMode[0].NaturalColorExposure || param == &confCameraMode[0].ArtificialColorExposure || param == &confCameraMode[0].WhiteBalance ||
		   param == &confCameraMode[0].PictureEffect || param == &confCameraMode[0].NoiseReduction2D3D ||
		   param == &confCamera.RGain || param == &confCamera.BGain ||
		   param == &confCamera.MonitoringMode || param == &confCamera.LVDSMode ||
		   param == &confCamera.MDEnable || param == &confCamera.MDThreshold ||
		   param == &confCamera.MDIntervalTime || param == &confCamera.MDStartPos ||
		   param == &confCamera.MDStopPos;
}

static void lvicam_camera_mode_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct lvicam *lvicam_soc = container_of(delayed_work, struct lvicam, camera_mode_work);
	bool apply_camera_mode;
	unsigned int mode_index = 0;
	int ret;

	mutex_lock(&lvicam_soc->mutex);
	apply_camera_mode = lvicam_soc->pending_camera_mode_apply;
	lvicam_soc->pending_camera_mode_apply = false;
	mutex_unlock(&lvicam_soc->mutex);

	if (apply_camera_mode) {
		if (lvicam_soc->seesaw_override >= 0)
			mode_index = lvicam_soc->seesaw_override;
		else if (lvicam_soc->seesaw_gpio)
			mode_index = gpiod_get_value(lvicam_soc->seesaw_gpio) ? 0 : 1;
		pr_info("[%s] seesaw_override=%d, seesaw_gpio=%d, mode_index=%u\n", __func__,
			lvicam_soc->seesaw_override,
			lvicam_soc->seesaw_gpio ? gpiod_get_value(lvicam_soc->seesaw_gpio) : -1,
			mode_index);

		ret = lvicamera_apply_visca_init(lvicam_soc, mode_index);
		if (ret)
			pr_err("[%s] Failed to apply camera mode to FPGA: %d\n", __func__, ret);
	}
}

visca_cmd_t convert_subreg_to_visca(uint8_t reg, uint32_t value)
{
	uint8_t speed = lvicam_visca_zoom_speed(lvicam->camera_mode_config.ZoomSpeed);
	visca_cmd_t cmd;

	switch (reg) {
	case VISCA_CMD_ZOOM_SPEED:
		lvicam->camera_mode_config.ZoomSpeed = (uint16_t)value;
		return (visca_cmd_t){ .data = { 0 }, .size = 0 };
	case VISCA_CMD_ZOOM_DIRECT:
		if (value > 0xFFFF)
			value = 0xFFFF;
		return cam_zoom_absolute((uint16_t)value);
	case VISCA_CMD_ZOOM_STEP:
		if (value != VISCA_CMD_ZOOM_STEP_INC && value != VISCA_CMD_ZOOM_STEP_DEC) {
			lvicam->camera_mode_config.ZoomSteps = (uint16_t)value;
			return (visca_cmd_t){ .data = { 0 }, .size = 0 };
		}
		return cam_zoom_step(value);
	case VISCA_CMD_R_GAIN:
		if (value <= VISCA_GAIN_DIRECT_MAX)
			confCamera.RGain.data[0] = (uint8_t)value;
		return cam_r_gain_direct(value);
	case VISCA_CMD_B_GAIN:
		if (value <= VISCA_GAIN_DIRECT_MAX)
			confCamera.BGain.data[0] = (uint8_t)value;
		return cam_b_gain_direct(value);
	case VISCA_CMD_NR_2D:
		if (value <= NR_2D_MODE_5) {
			lvicam->camera_mode_config.NoiseReduction2D = (uint8_t)value;
			confCameraMode[0].NoiseReduction2D3D.data[0] =
				(lvicam->camera_mode_config.NoiseReduction2D << 4) |
				lvicam->camera_mode_config.NoiseReduction3D;
		}
		return cam_nr_2D3D_independent(
			(lvicam->camera_mode_config.NoiseReduction2D << 4) |
			lvicam->camera_mode_config.NoiseReduction3D);
	case VISCA_CMD_NR_3D:
		if (value <= NR_3D_MODE_5) {
			lvicam->camera_mode_config.NoiseReduction3D = (uint8_t)value;
			confCameraMode[0].NoiseReduction2D3D.data[0] =
				(lvicam->camera_mode_config.NoiseReduction2D << 4) |
				lvicam->camera_mode_config.NoiseReduction3D;
		}
		return cam_nr_2D3D_independent(
			(lvicam->camera_mode_config.NoiseReduction2D << 4) |
			lvicam->camera_mode_config.NoiseReduction3D);
	case VISCA_CMD_MONITORING_MODE:
		if (confCamera.MonitoringMode.data)
			confCamera.MonitoringMode.data[0] = (uint8_t)value;
		return cam_monitoring_mode((enum monitoring_mode)value);
	case VISCA_CMD_LVDS_MODE:
		if (confCamera.LVDSMode.data)
			confCamera.LVDSMode.data[0] = (uint8_t)value;
		return cam_lvds_mode((enum lvds_mode)value);
	case VISCA_CMD_PICTURE_EFFECT:
		if (lvicam && lvicam->camera_mode_config.PictureEffect != (uint8_t)value) {
			lvicam->camera_mode_config.PictureEffect = (uint8_t)value;
			pr_info("[%s] Picture effect updated to 0x%02X\n", __func__, (uint8_t)value);
		}
		return cam_picture_effect((enum picture_effect_mode)value);
	default:
		pr_err("[%s] Unsupported VISCA subreg: 0x%02X\n", __func__, reg);
		return (visca_cmd_t){ .data = { 0 }, .size = 0 };
	}
}

static long lvicam_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case LVICAM_CTRL_IOCTL_WRITE_DATA: {
		struct lvicam_i2c_cmd i2c_cmd;
		struct i2c_msg msg;
		visca_cmd_t visca = { { 0 }, 0 };
		uint8_t buf[5] = { 0 };
		int buf_len;

		if (copy_from_user(&i2c_cmd, (void __user *)arg, sizeof(i2c_cmd)))
			return -EFAULT;

		if (i2c_cmd.regtype == REG_TYPE_FPGA) {
			if (i2c_cmd.size != 1 && i2c_cmd.size != 2)
				return -EINVAL;

			buf_len = 1 + i2c_cmd.size;
			buf[0] = i2c_cmd.subreg;

			if (i2c_cmd.size == 1) {
				buf[1] = (uint8_t)(i2c_cmd.data);
			} else if (i2c_cmd.size == 2) {
				buf[1] = (uint8_t)(i2c_cmd.data >> 8); // MSB
				buf[2] = (uint8_t)(i2c_cmd.data & 0xFF); // LSB
			}

			msg.addr = lvicam_ctrl.client->addr;
			msg.flags = 0;
			msg.len = buf_len;
			msg.buf = buf;

			ret = i2c_transfer(lvicam_ctrl.adapter, &msg, 1);
			if (ret != 1)
				return -EIO;

			pr_info("[%s] [FPGA] WRITE: subreg=0x%02X, size=%zu -> 0x%02X%02X\n", __func__, i2c_cmd.subreg, i2c_cmd.size, buf[1], i2c_cmd.size >= 2 ? buf[2] : 0);

			if (i2c_cmd.subreg == USER_PANEL1_CMD_CONTROL_REG && lvicam_ctrl.lvicam_ptr) {
				enum color_mode new_mode;
				if (buf[1] == CMD_NATCOL_DN || buf[1] == CMD_NATCOL_UP)
					new_mode = COLOR_MODE_NATURAL;
				else if (buf[1] == CMD_ARTCOL_DN || buf[1] == CMD_ARTCOL_UP)
					new_mode = COLOR_MODE_ARTIFICIAL;
				else
					goto skip_color_mode_track;

				if (lvicam_ctrl.lvicam_ptr->color_mode != new_mode) {
					uint8_t *p;
					uint8_t exp_value;
					visca_cmd_t visca;

					lvicam_ctrl.lvicam_ptr->color_mode = new_mode;
					pr_info("[%s] Color mode changed to %s\n", __func__,
						new_mode == COLOR_MODE_NATURAL ? "natural" : "artificial");

					if (new_mode == COLOR_MODE_NATURAL)
						p = confCameraMode[lvicam_ctrl.lvicam_ptr->active_mode_index].NaturalColorExposure.data;
					else
						p = confCameraMode[lvicam_ctrl.lvicam_ptr->active_mode_index].ArtificialColorExposure.data;
					exp_value = p[0];

					visca = cam_exp_comp_direct(exp_value);
					lvicam_fpga_visca_write(&visca);
					pr_info("[%s] Applied exposure compensation: %d (%s)\n", __func__,
						exp_value, new_mode == COLOR_MODE_NATURAL ? "natural" : "artificial");
				}
			}
skip_color_mode_track:

			return 0;
		}

		if (i2c_cmd.regtype != REG_TYPE_VISCA)
			return -EINVAL;

		visca = convert_subreg_to_visca(i2c_cmd.subreg, i2c_cmd.data);
		if (!visca.size) {
			pr_info("[%s] [VISCA] cached subreg=0x%02X, data=0x%08X\n", __func__, i2c_cmd.subreg, i2c_cmd.data);
			return 0;
		}

		ret = lvicam_fpga_visca_write(&visca);
		if (!ret && (i2c_cmd.subreg == VISCA_CMD_ZOOM_DIRECT ||
			     i2c_cmd.subreg == VISCA_CMD_ZOOM_STEP))
			lvicam_fpga_wait_camcmd_complete();

		return 0;
	}

	case LVICAM_CTRL_IOCTL_READ_DATA: {
		struct lvicam_i2c_cmd i2c_cmd;
		uint8_t subreg;
		uint8_t read_buf[2] = { 0 };
		struct i2c_msg msgs[2];

		if (copy_from_user(&i2c_cmd, (void __user *)arg, sizeof(i2c_cmd)))
			return -EFAULT;

		if (i2c_cmd.size != 1 && i2c_cmd.size != 2)
			return -EINVAL;

		subreg = i2c_cmd.subreg;
		msgs[0].addr = lvicam_ctrl.client->addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &subreg;
		msgs[1].addr = lvicam_ctrl.client->addr;
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = i2c_cmd.size;
		msgs[1].buf = read_buf;

		ret = i2c_transfer(lvicam_ctrl.adapter, msgs, 2);
		if (ret != 2)
			return -EIO;

		if (i2c_cmd.size == 1)
			i2c_cmd.data = read_buf[0];
		else
			i2c_cmd.data = (read_buf[0] << 8) | read_buf[1];

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

	case LVICAM_CTRL_IOCTL_SET_CAMERAMODE: {
		struct lvicam_cameramode_config config;

		if (copy_from_user(&config, (void __user *)arg, sizeof(config)))
			return -EFAULT;

		confCameraMode[0].Zoom.data[0] = config.Zoom & 0xFF;
		confCameraMode[0].Zoom.data[1] = (config.Zoom >> 8) & 0xFF;
		confCameraMode[0].ZoomMin.data[0] = config.ZoomMin & 0xFF;
		confCameraMode[0].ZoomMin.data[1] = (config.ZoomMin >> 8) & 0xFF;
		confCameraMode[0].ZoomMax.data[0] = config.ZoomMax & 0xFF;
		confCameraMode[0].ZoomMax.data[1] = (config.ZoomMax >> 8) & 0xFF;
		confCameraMode[0].ZoomSpeed.data[0] = config.ZoomSpeed & 0xFF;
		confCameraMode[0].ZoomSpeed.data[1] = (config.ZoomSpeed >> 8) & 0xFF;
		confCameraMode[0].Focus.data[0] = config.Focus;
		confCameraMode[0].FocusPos.data[0] = config.FocusPos & 0xFF;
		confCameraMode[0].FocusPos.data[1] = (config.FocusPos >> 8) & 0xFF;
		confCameraMode[0].FocusMin.data[0] = config.FocusMin & 0xFF;
		confCameraMode[0].FocusMin.data[1] = (config.FocusMin >> 8) & 0xFF;
		confCameraMode[0].FocusMax.data[0] = config.FocusMax & 0xFF;
		confCameraMode[0].FocusMax.data[1] = (config.FocusMax >> 8) & 0xFF;
		confCameraMode[0].FocusAFSpeed.data[0] = config.FocusAFSpeed;
		confCameraMode[0].FocusAFMode.data[0] = config.FocusAFMode;
		confCameraMode[0].NaturalColorExposure.data[0] = config.NaturalColorExposure;
		confCameraMode[0].ArtificialColorExposure.data[0] = config.ArtificialColorExposure;
		confCameraMode[0].WhiteBalance.data[0] = config.WhiteBalance;
		confCameraMode[0].PictureEffect.data[0] = config.PictureEffect;
		confCameraMode[0].NoiseReduction2D3D.data[0] = config.NoiseReduction2D3D;

		return lvicamera_apply_visca_init(lvicam_ctrl.lvicam_ptr, 0);
	}

	case LVICAM_CTRL_IOCTL_TRIGGER_CAMERA_MODE: {
		pr_info("[%s] TRIGGER_CAMERA_MODE\n", __func__);
		trigger_seesaw_toggle(lvicam_ctrl.lvicam_ptr);

		return 0;
	}

	default:
		return -ENOTTY;
	}
}

/* ############# DEBUGFS ################ */

static struct dentry *lvicam_debugfs_dir;

static void trigger_camera_mode_apply(struct lvicam *lv)
{
	mutex_lock(&lv->mutex);
	lv->pending_camera_mode_apply = true;
	mutex_unlock(&lv->mutex);

	mod_delayed_work(system_wq, &lv->camera_mode_work, msecs_to_jiffies(100));
	kill_fasync(&lvicam_async_queue, SIGIO, POLL_IN);
}

static void trigger_seesaw_toggle(struct lvicam *lv)
{
	int override;

	mutex_lock(&lv->mutex);
	if (lv->seesaw_override < 0)
		lv->seesaw_override = 0;
	else
		lv->seesaw_override = !lv->seesaw_override;
	override = lv->seesaw_override;
	lv->pending_camera_mode_apply = true;
	mutex_unlock(&lv->mutex);

	pr_info("[%s] seesaw_override -> %d\n", __func__, override);
	mod_delayed_work(system_wq, &lv->camera_mode_work, msecs_to_jiffies(100));
	kill_fasync(&lvicam_async_queue, SIGIO, POLL_IN);
}

static int debugfs_fpga_write(uint8_t reg, uint8_t data)
{
	return fpga_i2c_write(reg, &data, 1);
}

static ssize_t lvicam_debugfs_write(struct file *file, const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	struct lvicam *lv = file_inode(file)->i_private;
	const struct dentry *dentry = file_dentry(file);
	const char *name = dentry->d_name.name;
	char buf[16];
	size_t len = min(count, sizeof(buf) - 1);
	int ret = 0;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	buf[len] = '\0';

	if (buf[0] != '1')
		return count;

	if (strcmp(name, "zoom_cw") == 0) {
		pr_info("[%s] zoom_cw\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_CW);
		if (ret)
			pr_err("[%s] zoom_cw failed: %d\n", __func__, ret);
	} else if (strcmp(name, "zoom_ccw") == 0) {
		pr_info("[%s] zoom_ccw\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_CCW);
		if (ret)
			pr_err("[%s] zoom_ccw failed: %d\n", __func__, ret);
	} else if (strcmp(name, "zoom_reset") == 0) {
		pr_info("[%s] zoom_reset\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_PUSH_DN);
		if (ret == 0)
			ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_PUSH_UP);
		if (ret)
			pr_err("[%s] zoom_reset failed: %d\n", __func__, ret);
	} else if (strcmp(name, "zoom_visca_cw") == 0) {
		visca_cmd_t step_cmd;
		pr_info("[%s] zoom_visca_cw\n", __func__);
		step_cmd = cam_zoom_step(VISCA_CMD_ZOOM_STEP_INC);
		if (step_cmd.size) {
			ret = lvicam_fpga_visca_write(&step_cmd);
			if (ret == 0)
				lvicam_fpga_wait_camcmd_complete();
		}
	} else if (strcmp(name, "zoom_visca_ccw") == 0) {
		visca_cmd_t step_cmd;
		pr_info("[%s] zoom_visca_ccw\n", __func__);
		step_cmd = cam_zoom_step(VISCA_CMD_ZOOM_STEP_DEC);
		if (step_cmd.size) {
			ret = lvicam_fpga_visca_write(&step_cmd);
			if (ret == 0)
				lvicam_fpga_wait_camcmd_complete();
		}
	} else if (strcmp(name, "seesaw_toggle") == 0) {
		pr_info("[%s] seesaw_toggle\n", __func__);
		trigger_seesaw_toggle(lv);
	} else if (strcmp(name, "natcol") == 0) {
		pr_info("[%s] natcol\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_NATCOL_DN);
		if (ret == 0)
			ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_NATCOL_UP);
		if (ret)
			pr_err("[%s] natcol failed: %d\n", __func__, ret);
	} else if (strcmp(name, "artcol") == 0) {
		pr_info("[%s] artcol\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ARTCOL_DN);
		if (ret == 0)
			ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ARTCOL_UP);
		if (ret)
			pr_err("[%s] artcol failed: %d\n", __func__, ret);
	} else if (strcmp(name, "func_cw") == 0) {
		pr_info("[%s] func_cw\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_CW);
		if (ret)
			pr_err("[%s] func_cw failed: %d\n", __func__, ret);
	} else if (strcmp(name, "func_ccw") == 0) {
		pr_info("[%s] func_ccw\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_CCW);
		if (ret)
			pr_err("[%s] func_ccw failed: %d\n", __func__, ret);
	} else if (strcmp(name, "func_press") == 0) {
		pr_info("[%s] func_press\n", __func__);
		ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_BTN_DN);
		if (ret == 0)
			ret = debugfs_fpga_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_BTN_UP);
		if (ret)
			pr_err("[%s] func_press failed: %d\n", __func__, ret);
	}

	return count;
}

static const struct file_operations lvicam_debugfs_fops = {
	.write = lvicam_debugfs_write,
};

#define LVICAM_DEBUGFS_FILE(name) \
	debugfs_create_file(name, 0200, lvicam_debugfs_dir, lv, &lvicam_debugfs_fops)

static void lvicam_debugfs_init(struct lvicam *lv)
{
	if (!lv)
		return;

	lvicam_debugfs_dir = debugfs_create_dir("lvicam", NULL);
	if (IS_ERR(lvicam_debugfs_dir))
		return;

	LVICAM_DEBUGFS_FILE("zoom_cw");
	LVICAM_DEBUGFS_FILE("zoom_ccw");
	LVICAM_DEBUGFS_FILE("zoom_visca_cw");
	LVICAM_DEBUGFS_FILE("zoom_visca_ccw");
	LVICAM_DEBUGFS_FILE("zoom_reset");
	LVICAM_DEBUGFS_FILE("seesaw_toggle");
	LVICAM_DEBUGFS_FILE("natcol");
	LVICAM_DEBUGFS_FILE("artcol");
	LVICAM_DEBUGFS_FILE("func_cw");
	LVICAM_DEBUGFS_FILE("func_ccw");
	LVICAM_DEBUGFS_FILE("func_press");
}

static void lvicam_debugfs_cleanup(void)
{
	debugfs_remove_recursive(lvicam_debugfs_dir);
	lvicam_debugfs_dir = NULL;
}

/* ############# END DEBUGFS ################ */

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lvicam_ioctl,
	.fasync = lvicam_fasync,
};

#define I2C_BUS_NUM 2 // /dev/i2c-2
#define I2C_DEV_ADDR 0x10 //FPGA I2C address for control commands

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

	usleep_range(150000, 155000); // Wait 150 milliseconds

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

static irqreturn_t lvicam_seesaw_irq_handler(int irq, void *dev_id)
{
	struct lvicam *lvicam = dev_id;

	pr_info("[%s] seesaw GPIO interrupt triggered\n", __func__);

	mutex_lock(&lvicam->mutex);
	lvicam->pending_camera_mode_apply = true;
	lvicam->seesaw_override = -1; /* Physical change resets override */
	mutex_unlock(&lvicam->mutex);

	mod_delayed_work(system_wq, &lvicam->camera_mode_work, msecs_to_jiffies(100));
	kill_fasync(&lvicam_async_queue, SIGIO, POLL_IN);
	return IRQ_HANDLED;
}

/**
 * @brief Notifier callback for changes in the lviconfig module, such as changes to the camera mode configuration.
 */
static int lvicam_lviconfig_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct lvicam *lvicam_soc = container_of(nb, struct lvicam, lviconfig_nb);
	const ConfigParam *param = data;

	if (!lvicam_param_needs_apply(param))
		return NOTIFY_DONE;

	pr_info("[%s] param='%s' needs_apply=true\n", __func__, param->name ? param->name : "(null)");

	mutex_lock(&lvicam_soc->mutex);
	lvicam_soc->pending_camera_mode_apply = true;
	mutex_unlock(&lvicam_soc->mutex);

	mod_delayed_work(system_wq, &lvicam_soc->camera_mode_work, msecs_to_jiffies(100));

	return NOTIFY_OK;
}

/* Probe & Remove */

static void lvicam_motion_detect_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct lvicam *lvicam = container_of(delayed_work, struct lvicam, motion_detect_work);
	FPGAFlagsInit_t flags;
	uint8_t raw;
	int ret;

	ret = fpga_i2c_read(FPGA_FLAGS_INIT_STATUS_REG, &raw);
	if (ret)
		goto reschedule;

	memcpy(&flags, &raw, sizeof(flags));

	if (flags.motion_detect_alarm)
		pr_info("[lvicam] Motion detected!\n");

reschedule:
	mod_delayed_work(system_wq, &lvicam->motion_detect_work,
			 msecs_to_jiffies(MOTION_DETECT_POLL_MS));
}

static int lvicam_probe(struct i2c_client *client)
{
	int err = 0;
	struct regulator *vcc = NULL;
	int seesaw_irq = 0;
	int ret = 0;

	pr_info("[%s] call\n", __func__);

	if (ConfigParam_IsInitialized() == 0) {
		pr_warn("[%s] ConfigParam not yet initialized, deferring probe\n", __func__);
		return -EPROBE_DEFER;
	} else {
		pr_info("[%s] ConfigParam is initialized, proceeding with probe\n", __func__);
	}

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
	INIT_DELAYED_WORK(&lvicam->camera_mode_work, lvicam_camera_mode_work);
	INIT_DELAYED_WORK(&lvicam->motion_detect_work, lvicam_motion_detect_work);
	mod_delayed_work(system_wq, &lvicam->motion_detect_work,
			 msecs_to_jiffies(MOTION_DETECT_POLL_MS));

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

	lvicam_debugfs_init(lvicam);

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
	lvicam->pending_camera_mode_apply = false;
	lvicam->seesaw_override = -1;
	lvicam->lviconfig_nb.notifier_call = lvicam_lviconfig_notifier;
	err = lviconfig_register_notifier(&lvicam->lviconfig_nb);
	if (err) {
		pr_err("[%s] Failed to register lviconfig notifier: %d\n", __func__, err);
		goto error_media_entity;
	}

	{
		unsigned int mode_index = 0;
		if (lvicam->seesaw_gpio)
			mode_index = gpiod_get_value(lvicam->seesaw_gpio) ? 0 : 1;
		pr_info("[%s] seesaw=%d, mode_index=%u\n", __func__,
			lvicam->seesaw_gpio ? gpiod_get_value(lvicam->seesaw_gpio) : -1, mode_index);

		err = lvicamera_apply_visca_init(lvicam, mode_index);
	}
	if (err)
		pr_warn("[%s] Failed to apply initial VISCA init: %d\n", __func__, err);

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
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct lvicam *lvicam = to_lvicam(sd);

	pr_info("[%s] Removing lvicam.\n", __func__);

	lviconfig_unregister_notifier(&lvicam->lviconfig_nb);
	cancel_delayed_work_sync(&lvicam->camera_mode_work);
	cancel_delayed_work_sync(&lvicam->motion_detect_work);

	lvicam_debugfs_cleanup();

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