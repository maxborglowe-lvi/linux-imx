#ifndef LVICAM_CTRL_H
#define LVICAM_CTRL_H

#include <linux/ioctl.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <stddef.h>
#endif

typedef struct lvicam_cameramode_config {
	uint16_t Zoom;
	uint16_t ZoomMin;
	uint16_t ZoomMax;
	uint16_t ZoomSpeed;
	uint16_t ZoomSteps; // How many steps are needed to go from min to max zoom (direct mode)
	uint32_t Focus;
	uint32_t FocusMin;
	uint32_t FocusMax;
	uint32_t FocusSpeed;
	uint32_t NaturalColorExposure;
	uint32_t ArtificialColorExposure;
	uint32_t WhiteBalance;
	uint8_t PictureEffect;
	uint8_t NoiseReduction2D3D;
	uint8_t NoiseReduction2D;
	uint8_t NoiseReduction3D;

	uint8_t DZoom; //digital zoom position 0x00-0xEB
	uint8_t DZoomOnOff; // 0 = Off, 1 = On
	uint8_t DZoomMode; // 0 = Combine, 1 = Separate

} lvicam_cameramode_config_t;



#define LVICAM_IOC_MAGIC 'L'
#define LVICAM_CTRL_IOCTL_READ_DATA _IOR(LVICAM_IOC_MAGIC, 1, struct lvicam_i2c_cmd)
#define LVICAM_CTRL_IOCTL_WRITE_DATA _IOW(LVICAM_IOC_MAGIC, 2, struct lvicam_i2c_cmd)
#define LVICAM_CTRL_IOCTL_READ_SEESAW _IOR(LVICAM_IOC_MAGIC, 3, struct lvicam_seesaw_status)
#define LVICAM_CTRL_IOCTL_SET_CAMERAMODE _IOW(LVICAM_IOC_MAGIC, 4, struct lvicam_cameramode_config)

enum register_type { REG_TYPE_FPGA = 0, REG_TYPE_VISCA = 1 };

struct lvicam_i2c_cmd {
	uint8_t subreg; /* FPGA subregister or VISCA command/inquiry selector */
	uint8_t regtype;
	uint32_t data;
	size_t size;
};

struct lvicam_seesaw_status {
	int value; // Current value of the seesaw
};

// ******* FPGA STATUS REGISTERS ********

// FPGA Status Register: 0x80
#define FPGA_FLAGS_INIT_STATUS_REG 0x80

typedef struct {
	uint8_t reserved1 : 1; // Bit 0: Reserved
	uint8_t reserved2 : 1; // Bit 1: Reserved
	uint8_t system_camera_pll_lock : 1; // Bit 2: FPGA PLL lock flag bit. 0 = PLL not locked, 1 = PLL locked
	uint8_t network_change : 1; // Bit 3: Network change flag bit. 0 = Network Change reply not received, 1 = Network Change reply received
	uint8_t motion_detect_alarm : 1; // Bit 4: Motion detect alarm flag bit. 0 = No motion detected, 1 = Motion detected (active-high for 500ms)
	uint8_t visca_error : 1; // Bit 5: VISCA error flag bit. 0 = VISCA cmd syntax valid and executable, 1 = VISCA cmd syntax NOT valid or not executable
	uint8_t visca_ack : 1; // Bit 6: VISCA ack flag bit. 0 = No ack received for VISCA cmd, 1 = Ack received for VISCA cmd
	uint8_t visca_enable : 1; // Bit 7: VISCA enable flag bit. 0 = VISCA disabled, 1 = VISCA enabled
} FPGAFlagsInit_t;

// Camera ID Register: 0x81
#define CAM_ID_STATUS_REG 0x81
// Known camera ID values:
#define CAM_ID_WONWOO_MC108_M3 0x465A
#define CAM_ID_WONWOO_MC105 0x0E52
#define CAM_ID_WONWOO_MM405 0x0E6A
#define CAM_ID_SONY_FCB_EV9520L 0x0711
#define CAM_ID_TAMRON_MP1010_VC 0x0023
#define CAM_ID_TAMRON_MP3010M_EV 0xF017

// Zoom Position Registers
#define CAM1_ZOOM_POS_STATUS_REG 0xC0 // [15:0] Zoom absolute value - camera 1 (Reading camera)
#define CAM2_ZOOM_POS_STATUS_REG 0xC1 // [15:0] Zoom absolute value - camera 2 (Distance camera)

// Zoom Step Registers
#define CAM1_ZOOM_POS_STEP_STATUS_REG 0xC3 // [7:0] Zoom step value (0-22) - camera 1 (Reading camera)
#define CAM2_ZOOM_POS_STEP_STATUS_REG 0xC4 // [7:0] Zoom step value (0-22) - camera 2 (Distance camera)

// Reference Line Position Registers
#define REFLINE_HOR_POS_STATUS_REG 0xC6 // [15:0] Horizontal position of reference line
#define REFLINE_VER_POS_STATUS_REG 0xC7 // [15:0] Vertical position of reference line

// Curtain Position Registers
#define CURTAIN1_HOR_POS_STATUS_REG 0xC8 // [15:0] Upper horizontal curtain position
#define CURTAIN2_HOR_POS_STATUS_REG 0xC9 // [15:0] Lower horizontal curtain position
#define CURTAIN1_VER_POS_STATUS_REG 0xCA // [15:0] Left vertical curtain position
#define CURTAIN2_VER_POS_STATUS_REG 0xCB // [15:0] Right vertical curtain position

// Picture Mode Register: 0xCC
#define PICTURE_MODE_STATUS_REG 0xCC
#define PICTURE_MODE_ART_MASK 0xF0 // Bits 7:4 - Artifical color palette index (0-15)
#define PICTURE_MODE_NAT_MASK 0x0C // Bits 3:2 - Natural color palette index (0-3)
#define PICTURE_MODE_NAT_ON 0x02 // Bit 1: 1 = Natural mode, 0 = Artificial mode
#define PICTURE_MODE_CAMERA_FLIRP 0x01 // Bit 0: 1 = Distance camera, 0 = Readout camera

// PN Level Register: 0xCD
#define PN_LEVEL_STATUS_REG 0xCD // [7:0] P/N level in artificial color mode

// ********* FPGA CONTROL REGISTERS *********

// FPGA I2C Slave Address
#define FPGA_I2C_SLAVE_ADDR 0x10

// ********* Art Colour Control Registers *********
#define ART_COL_PAL_INIT_CONTROL_REG 0x01 // Multi-byte write
// Reserved: 0x02 - 0x04

// ********* ID Control Registers *********
#define PRODUCT_ID_CONTROL_REG 0x05 // [7:0] Write
#define PRODUCT_ID_MLZIP_FHD_17_TOUCH 0x1A // Example product ID for MLZIP FHD 17" Touch

// #define PANEL_ID_CONTROL_REG 0x06  // [7:0] Write
#define CAMERA_ID_CONTROL_REG 0x07 // [15:0] Write

// ********* User Panel Control Registers *********
#define USER_PANEL1_CMD_CONTROL_REG 0x08 // Multi-byte write
#define CMD_NATCOL_DN 0x13
#define CMD_NATCOL_UP 0x14
#define CMD_ARTCOL_DN 0x15
#define CMD_ARTCOL_UP 0x16
#define CMD_ZOOM_CW 0x19
#define CMD_ZOOM_CCW 0x1A
#define CMD_ZOOM_PUSH_DN 0x1B // deprecated
#define CMD_ZOOM_PUSH_UP 0x1C // deprecated
#define CMD_FUNCTION_CW 0x28
#define CMD_FUNCTION_CCW 0x29
#define CMD_FUNCTION_BTN_DN 0x2A
#define CMD_FUNCTION_BTN_UP 0x2B

#define VISCA_COMMAND_REG 0x7A // [7:0] Write - Register to send VISCA commands to FPGA for camera control

#define VISCA_CMD_ZOOM_STOP 0x1C
#define VISCA_CMD_ZOOM_TELE 0x1D
#define VISCA_CMD_ZOOM_WIDE 0x1E
#define VISCA_CMD_ZOOM_SPEED 0x1F
#define VISCA_CMD_ZOOM_DIRECT 0x20 // absolute zoom target; values above 0x4000 use DZoom when supported
#define VISCA_CMD_ZOOM_STEP 0x21 // incremented/decremented by panel events, sent as data byte for step size
#define VISCA_CMD_ZOOM_STEP_INC 1
#define VISCA_CMD_ZOOM_STEP_DEC 0
#define VISCA_CMD_R_GAIN 0x22
#define VISCA_CMD_B_GAIN 0x23
#define VISCA_CMD_PICTURE_EFFECT 0x24
#define VISCA_CMD_NR_2D 0x25
#define VISCA_CMD_NR_3D 0x26
#define VISCA_CMD_MONITORING_MODE 0x27
#define VISCA_CMD_LVDS_MODE 0x28

/*
 * REG_TYPE_VISCA inquiries return the driver-maintained setup state used by
 * lvicam's VISCA path. They are not raw camera-originated VISCA reply bytes.
 */
#define VISCA_INQ_DZOOM 0x24
#define VISCA_INQ_DZOOM_ONOFF 0x25
#define VISCA_INQ_DZOOM_MODE 0x26
#define VISCA_INQ_WB_MODE 0x27

/* For VISCA_CMD_R_GAIN / VISCA_CMD_B_GAIN in i2c_cmd.data */
#define VISCA_GAIN_DIRECT_MAX 0xFF
#define VISCA_GAIN_RESET 0x100
#define VISCA_GAIN_UP 0x101
#define VISCA_GAIN_DOWN 0x102

#define ZOOM_LIMIT_OPTICAL_MAX 0x4000 //max zoom position (direct) for Sony-based cameras.
#define ZOOM_LIMIT_DIGITAL_MAX 0xEB //max zoom position (direct) for digital zoom.

#define VISCA_MAX_CMD_SIZE 12 // Max 12 bytes of data for VISCA commands (e.g. zoom value)

typedef struct {
	uint8_t data[VISCA_MAX_CMD_SIZE];
	size_t size;
} visca_cmd_t;

// ********* Panel1 Edge Control Bits *********
#define USER_PANEL1_EDGE_CONTROL_REG 0x09 // [7:0] Write
#define EDGE_CTRL_NATCOL_BIT (1 << 3)
#define EDGE_CTRL_ARTCOL_BIT (1 << 2)
#define EDGE_CTRL_ZOOM_PUSH_BIT (1 << 1)
#define EDGE_CTRL_FUNCTION_PUSH_BIT (1 << 0)
#define EDGE_CTRL_DEFAULT 0xF0 // Reserved bits 7:4 set to 1, all edge bits inactive by default

// ********* Camera Control Registers *********
// Camera 1 (Readout)
#define CAM1_ZOOM_SPEED_CONTROL_REG 0x10 // [15:0] Write
#define CAM1_ZOOM_POS_MIN_CONTROL_REG 0x11 // [15:0] Write
#define CAM1_ZOOM_POS_MAX_CONTROL_REG 0x12 // [15:0] Write
#define CAM1_ZOOM_POS_INIT_CONTROL_REG 0x13 // [15:0] Write

// Camera 2 (Distance)
#define CAM2_ZOOM_POS_MIN_CONTROL_REG 0x14 // [15:0] Write
#define CAM2_ZOOM_POS_MAX_CONTROL_REG 0x15 // [15:0] Write
#define CAM2_ZOOM_POS_INIT_CONTROL_REG 0x16 // [15:0] Write
#define CAM2_ZOOM_SPEED_CONTROL_REG 0x17 // [15:0] Write

// Reserved: 0x18, 0x19

// Zoom Increment Step Tables (Multi-word writes)
#define CAM1_ZOOM_POS_INC_STEP_CONTROL_REG 0x1A // [15:0] * N steps
#define CAM1_ZOOM_POS_INIT_INC_CONTROL_REG 0x1B // [7:0]  Write

#define CAM2_ZOOM_POS_INC_STEP_CONTROL_REG 0x1C // [15:0] * N steps
#define CAM2_ZOOM_POS_INIT_INC_CONTROL_REG 0x1D // [7:0]  Write

// Reserved: 0x1E, 0x1F

// ********* Activated Functions Control Register *********
// NOTE: Sub Address not clearly defined in doc (marked as 0x??), define as placeholder
#define ACTIVATED_FUNCTIONS_CONTROL_REG 0x20 // [7:0] Write (bitmask)
#define ACT_FUNC_BITMASK_OBJECT_SHOW_VIEW (1 << 0) // Example use: Bit 0 toggles ObjectShowView

#define CAM_REG_EXP_COMP_ONOFF 0x3E
enum exp_comp_onoff {
	EXP_COMP_ON = 0x02,
	EXP_COMP_OFF = 0x03,
};
#define CAM_REG_EXP_COMP_SETTING 0x0E
enum exp_comp_setting {
	EXP_COMP_SETTING_RESET = 0x00,
	EXP_COMP_SETTING_UP = 0x02,
	EXP_COMP_SETTING_DOWN = 0x03,
};
#define CAM_REG_EXP_COMP_DIRECT 0x4E

#define CAM_REG_APERTURE 0x02
enum aperture {
	APERTURE_RESET = 0x00,
	APERTURE_UP = 0x02,
	APERTURE_DOWN = 0x03,
};
#define CAM_REG_APERTURE_DIRECT 0x42

#define CAM_REG_FOCUS 0x08
enum focus {
	FOCUS_STOP = 0x00,
	FOCUS_FAR = 0x02,
	FOCUS_NEAR = 0x03,
	FOCUS_FAR_VARIABLE = 0x20, // set lower nibble to change far focus
	FOCUS_NEAR_VARIABLE = 0x30, // set lower nibble to change near focus
};
#define CAM_REG_FOCUS_DIRECT 0x48


#define CAM_REG_FOCUS_MODE 0x38
enum focus_mode {
	FOCUS_MODE_AUTO = 0x02,
	FOCUS_MODE_MANUAL = 0x03,
	FOCUS_MODE_AUTO_MANUAL = 0x10 //the fuck is this??
};
#define CAM_REG_FOCUS_ONE_PUSH_TRIGGER 0x18
enum focus_one_push_trigger {
	FOCUS_ONE_PUSH_TRIGGER = 0x01,
};
#define CAM_REG_FOCUS_NEAR_LIMIT 0x28


#define CAM_REG_AE 0x39
enum ae_mode {
	AE_MODE_AUTO = 0,
	AE_MODE_MANUAL = 0x3,
	AE_MODE_SHUTTER_PRIORITY = 0xA,
	AE_MODE_IRIS_PRIORITY = 0xB,
	AE_MODE_BRIGHT = 0xD
};
#define CAM_REG_WB 0x35
enum wb_mode {
	WB_MODE_AUTO = 0,
	WB_MODE_INDOOR = 0x1,
	WB_MODE_OUTDOOR = 0x2,
	WB_MODE_ONE_PUSH = 0x3,
	WB_MODE_ATW = 0x4,
	WB_MODE_MANUAL = 0x5,
	WB_MODE_OUTDOOR_AUTO = 0x6,
	WB_MODE_SODIUM_LAMP_AUTO = 0x7,
	WB_MODE_SODIUM_LAMP = 0x8,
	WB_MODE_SODIUM_LAMP_OUTDOOR_AUTO = 0x9
};

#define CAM_REG_HR 0x52
enum hr_mode {
	HR_ON = 0x02,
	HR_OFF = 0x03
};

#define CAM_REG_NR 0x53
enum nr_mode {
	NR_MODE_OFF = 0x00,
	NR_MODE_1 = 0x01,
	NR_MODE_2 = 0x02,
	NR_MODE_3 = 0x03,
	NR_MODE_4 = 0x04,
	NR_MODE_5 = 0x05
};
#define CAM_NR_2D3D_INDEPENDENT 0x05 //instead of 0x04 on index 2
enum nr_2d_mode {
	NR_2D_MODE_OFF = 0x00,
	NR_2D_MODE_1 = 0x01,
	NR_2D_MODE_2 = 0x02,
	NR_2D_MODE_3 = 0x03,
	NR_2D_MODE_4 = 0x04,
	NR_2D_MODE_5 = 0x05
};
enum nr_3d_mode {
	NR_3D_MODE_OFF = 0x00,
	NR_3D_MODE_1 = 0x01,
	NR_3D_MODE_2 = 0x02,
	NR_3D_MODE_3 = 0x03,
	NR_3D_MODE_4 = 0x04,
	NR_3D_MODE_5 = 0x05
};

#define CAM_REG_SHUTTER 0x0A
enum shutter {
	SHUTTER_RESET = 0x00,
	SHUTTER_UP = 0x02,
	SHUTTER_DOWN = 0x03,
};
#define CAM_REG_SHUTTER_DIRECT 0x4A

#define CAM_REG_LR_REVERSE 0x61
enum lr_reverse_mode {
	LR_REVERSE_ON = 0x02,
	LR_REVERSE_OFF = 0x03
};

#define CAM_REG_PICTURE_EFFECT 0x63
enum picture_effect_mode {
	PICTURE_EFFECT_OFF = 0x00,
	PICTURE_EFFECT_NEGATIVE = 0x02,
	PICTURE_EFFECT_BW = 0x04,
	PICTURE_EFFECT_REDDISH1 = 0x10,
	PICTURE_EFFECT_REDDISH2 = 0x11,
	PICTURE_EFFECT_REDDISH3 = 0x12,
	PICTURE_EFFECT_REDDISH4 = 0x13,
	PICTURE_EFFECT_BLUISH1 = 0x20,
	PICTURE_EFFECT_BLUISH2 = 0x21, 
	PICTURE_EFFECT_BLUISH3 = 0x22,
	PICTURE_EFFECT_BLUISH4 = 0x23,
	PICTURE_EFFECT_GREENISH1 = 0x30,
	PICTURE_EFFECT_GREENISH2 = 0x31,
	PICTURE_EFFECT_GREENISH3 = 0x32,
	PICTURE_EFFECT_GREENISH4 = 0x33,
};

#define CAM_REG_PICTURE_FLIP 0x66
enum picture_flip_mode {
	PICTURE_FLIP_ON = 0x02,
	PICTURE_FLIP_OFF = 0x03
};

#define CAM_REG_MONITORING_MODE 0x72
enum monitoring_mode {
	MONITORING_MODE_1080i_59_94 = 0x01,
	MONITORING_MODE_1080i_60 = 0x02,
	MONITORING_MODE_1080i_50 = 0x04,
	MONITORING_MODE_1080p_29_97 = 0x06,
	MONITORING_MODE_1080p_30 = 0x07,
	MONITORING_MODE_1080p_25 = 0x08,
	MONITORING_MODE_720p_59_94 = 0x09,
	MONITORING_MODE_720p_60 = 0x0A,
	MONITORING_MODE_720p_50 = 0x0C,
	MONITORING_MODE_720p_29_97 = 0x0E,
	MONITORING_MODE_720p_30 = 0x0F,
	MONITORING_MODE_720p_25 = 0x11,
	MONITORING_MODE_1080p_59_94 = 0x13,
	MONITORING_MODE_1080p_50 = 0x14,
	MONITORING_MODE_1080p_60 = 0x15,
};

#define CAM_REG_LVDS_MODE 0x74
enum lvds_mode {
	LVDS_MODE_SINGLE = 0x00,
	LVDS_MODE_DUAL = 0x02
};

#endif // LVICAM_CTRL_H
