/* User events and states output via i2c) */

#define	COMM_ZOOM_INC_DN		0x10			// Zoom Increase Button Pushed
#define	COMM_ZOOM_DEC_DN		0x11			// Zoom Decrease Button Pushed
#define	COMM_ZOOM_UP			0x12			// Both Zoom Buttons Released
#define	COMM_ZOOM_CW			0x19			// Zoom Step Clockwise
#define	COMM_ZOOM_CCW			0x1A			// Zoom Step Counter Clockwise
#define	COMM_ZOOM_PUSH_DN		0x1B			// Zoom Increment Button Pushed
#define	COMM_ZOOM_PUSH_UP		0x1C			// Zoom Increment Button Released

#define	COMM_NCOL_DN			0x13			// Natural Color Button Pushed
#define	COMM_NCOL_UP			0x14			// Natural Color Button Released
#define	COMM_ACOL_DN			0x15			// Artificial Color Button Pushed
#define	COMM_ACOL_UP			0x16			// Artificial Color Button Released
#define	COMM_AFLOCK_DN			0x17			// Aufotocus lock Button Pushed
#define	COMM_AFLOCK_UP			0x18			// Aufotocus lock Button Released

#define	COMM_BRIGHTNESS_INC_DN	0x20			// Brightness Increase Button Pushed
#define	COMM_BRIGHTNESS_DEC_DN	0x21			// Brightness Decrease Button Pushed
#define	COMM_BRIGHTNESS_UP		0x22			// Both Brightness Buttons Released
#define	COMM_REFLINE_INC_DN		0x23			// Refline Increase Button Pushed
#define	COMM_REFLINE_DEC_DN		0x24			// Refline Decrease Button Pushed
#define	COMM_REFLINE_UP			0x25			// Both Refline Buttons Released
#define	COMM_TBLSW_DN			0x26			// TurboLight/Standard Button Pushed
#define	COMM_TBLSW_UP			0x27			// TurboLight/Standard Button Released

#define	COMM_FUNCTION_CW		0x28			// Function Increment switch clockwisw Pushed
#define	COMM_FUNCTION_CCW		0x29			// Function Increment switch counter clockwise
#define	COMM_FUNCTION_BTN_DN	0x2A			// Function Button Pushed
#define	COMM_FUNCTION_BTN_UP	0x2B			// Function Button Released

#define COMM_COLORPOS_BASE		0x30
#define COMM_COLOR_POS_OPEN		0x30			// Color for switch open
#define COMM_COLOR_POS1			0x31			// Color for switch pos 1 CCW
#define COMM_COLOR_POS2			0x32			// Color for switch pos 2
#define COMM_COLOR_POS3			0x33			// Color for switch pos 3
#define COMM_COLOR_POS4			0x34			// Color for switch pos 4
#define COMM_COLOR_POS5			0x35			// Color for switch pos 5

#define	COMM_BRIGHTNESS_POS		0x36			// Brightness pos.
#define	COMM_REFLINE_POS		0x37			// Refline pos.

// Note: Brightness Pos and refline Pos followed by two value-byte LSB 0-0xFF) , MSB 0-0x3F)

#define	COMM_NO_BRIGHTNESS		0x38			// No Brightness functionallity
#define	COMM_NO_REFLINE			0x39			// No Refline functionallity
#define	COMM_NO_FOCUSLOCK		0x3A			// No Focuslock functionallity

#define	COMM_STATE_CMD			0x40
#define	COMM_START_SC			0x41			// Master cmd: Initialize panel, then put panel in SCAN state
#define	COMM_STOP_SC			0x42			// Master cmd: Clear panel, then put panel in IDLE state
#define	COMM_SCAN				0x43			// Scan periphs
#define	COMM_STATE_IDLE			0x44
#define	COMM_FULL_SPEED			0x45			// Master cmd

#define COMM_SYSTEM_ON			0x46			// Master cmd: System has been turned ON.
#define COMM_SYSTEM_OFF			0x47			// Master cmd: System has been turned OFF.

#define COMM_ZOOMPOS_BASE		0x70			// Zoom for Shorted Switch
#define COMM_ZOOM_POS1			0x71			// Zoom level POSn
#define COMM_ZOOM_POS2			0x72
#define COMM_ZOOM_POS3			0x73
#define COMM_ZOOM_POS4			0x74
#define COMM_ZOOM_POS5			0x75
#define COMM_ZOOM_POS6			0x76
#define COMM_ZOOM_POS7			0x77
#define COMM_ZOOM_POS8			0x78
#define COMM_ZOOM_POS9			0x79
#define COMM_ZOOM_POS10			0x7A
#define COMM_ZOOM_POS11			0x7B
#define COMM_ZOOM_POS12			0x7C
#define COMM_ZOOM_POS13			0x7D
#define COMM_ZOOM_POS14			0x7E
#define COMM_ZOOM_POS15			0x7F
#define COMM_ZOOM_POS16			0x80
#define COMM_ZOOM_POS_OPEN		0x81


/* Panel internal commands*/

#define	COMM_LD_CMD				0x50
#define	COMM_LD_OFF				0x51			// Master cmd
#define	COMM_LD_RED				0x52			// Master cmd
#define	COMM_LD_GREEN			0x53			// Master cmd
#define	COMM_LD_YELLOW			0x54			// Master cmd
#define	COMM_SFL_YELLOW			0x55			// Master cmd
#define	COMM_LD_REDFLASH		0x5A			// Master cmd
#define	COMM_LD_GREENFLASH		0x5B			// Master cmd
#define	COMM_LD_YELLOWFLASH		0x5C			// Master cmd
#define	COMM_LD1				0x5D			// Master cmd: Set Current Led = Led 1
#define	COMM_LD2				0x5E			// Master cmd: Set Current Led = Led 2
#define	COMM_LD3				0x5F			// Master cmd: Set Current Led = Led 3

#define	COMM_DEBUG_CMD			0x60
#define	COMM_NO_DEBUG			0x61			// Master cmd
#define	COMM_DEBUG				0x62			// master cmd