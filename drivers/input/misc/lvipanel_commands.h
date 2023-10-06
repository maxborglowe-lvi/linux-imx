/* User events and states (output via i2c) */

#define	COMM_ZOOM_INC_DN		(0x10u)			// Zoom Increase Button Pushed
#define	COMM_ZOOM_DEC_DN		(0x11u)			// Zoom Decrease Button Pushed
#define	COMM_ZOOM_UP			(0x12u)			// Both Zoom Buttons Released
#define	COMM_ZOOM_CW			(0x19u)			// Zoom Step Clockwise
#define	COMM_ZOOM_CCW			(0x1Au)			// Zoom Step Counter Clockwise
#define	COMM_ZOOM_PUSH_DN		(0x1Bu)			// Zoom Increment Button Pushed
#define	COMM_ZOOM_PUSH_UP		(0x1Cu)			// Zoom Increment Button Released

#define	COMM_NCOL_DN			(0x13u)			// Natural Color Button Pushed
#define	COMM_NCOL_UP			(0x14u)			// Natural Color Button Released
#define	COMM_ACOL_DN			(0x15u)			// Artificial Color Button Pushed
#define	COMM_ACOL_UP			(0x16u)			// Artificial Color Button Released
#define	COMM_AFLOCK_DN			(0x17u)			// Aufotocus lock Button Pushed
#define	COMM_AFLOCK_UP			(0x18u)			// Aufotocus lock Button Released

#define	COMM_BRIGHTNESS_INC_DN	(0x20u)			// Brightness Increase Button Pushed
#define	COMM_BRIGHTNESS_DEC_DN	(0x21u)			// Brightness Decrease Button Pushed
#define	COMM_BRIGHTNESS_UP		(0x22u)			// Both Brightness Buttons Released
#define	COMM_REFLINE_INC_DN		(0x23u)			// Refline Increase Button Pushed
#define	COMM_REFLINE_DEC_DN		(0x24u)			// Refline Decrease Button Pushed
#define	COMM_REFLINE_UP			(0x25u)			// Both Refline Buttons Released
#define	COMM_TBLSW_DN			(0x26u)			// TurboLight/Standard Button Pushed
#define	COMM_TBLSW_UP			(0x27u)			// TurboLight/Standard Button Released

#define	COMM_FUNCTION_CW		(0x28u)			// Function Increment switch clockwisw Pushed
#define	COMM_FUNCTION_CCW		(0x29u)			// Function Increment switch counter clockwise
#define	COMM_FUNCTION_BTN_DN	(0x2Au)			// Function Button Pushed
#define	COMM_FUNCTION_BTN_UP	(0x2Bu)			// Function Button Released

#define COMM_COLORPOS_BASE		(0x30u)
#define COMM_COLOR_POS_OPEN		(0x30u)			// Color for switch open
#define COMM_COLOR_POS1			(0x31u)			// Color for switch pos 1 CCW
#define COMM_COLOR_POS2			(0x32u)			// Color for switch pos 2
#define COMM_COLOR_POS3			(0x33u)			// Color for switch pos 3
#define COMM_COLOR_POS4			(0x34u)			// Color for switch pos 4
#define COMM_COLOR_POS5			(0x35u)			// Color for switch pos 5

#define	COMM_BRIGHTNESS_POS		(0x36u)			// Brightness pos.
#define	COMM_REFLINE_POS		(0x37u)			// Refline pos.

// Note: Brightness Pos and refline Pos followed by two value-byte LSB (0-0xFF) , MSB (0-0x3F)

#define	COMM_NO_BRIGHTNESS		(0x38u)			// No Brightness functionallity
#define	COMM_NO_REFLINE			(0x39u)			// No Refline functionallity
#define	COMM_NO_FOCUSLOCK		(0x3Au)			// No Focuslock functionallity

#define	COMM_STATE_CMD			(0x40u)
#define	COMM_START_SC			(0x41u)			// Master cmd: Initialize panel, then put panel in SCAN state
#define	COMM_STOP_SC			(0x42u)			// Master cmd: Clear panel, then put panel in IDLE state
#define	COMM_SCAN				(0x43u)			// Scan periphs
#define	COMM_STATE_IDLE			(0x44u)
#define	COMM_FULL_SPEED			(0x45u)			// Master cmd

#define COMM_ZOOMPOS_BASE		(0x70u)			// Zoom for Shorted Switch
#define COMM_ZOOM_POS1			(0x71u)			// Zoom level POSn
#define COMM_ZOOM_POS2			(0x72u)
#define COMM_ZOOM_POS3			(0x73u)
#define COMM_ZOOM_POS4			(0x74u)
#define COMM_ZOOM_POS5			(0x75u)
#define COMM_ZOOM_POS6			(0x76u)
#define COMM_ZOOM_POS7			(0x77u)
#define COMM_ZOOM_POS8			(0x78u)
#define COMM_ZOOM_POS9			(0x79u)
#define COMM_ZOOM_POS10			(0x7Au)
#define COMM_ZOOM_POS11			(0x7Bu)
#define COMM_ZOOM_POS12			(0x7Cu)
#define COMM_ZOOM_POS13			(0x7Du)
#define COMM_ZOOM_POS14			(0x7Eu)
#define COMM_ZOOM_POS15			(0x7Fu)
#define COMM_ZOOM_POS16			(0x80u)
#define COMM_ZOOM_POS_OPEN		(0x81u)


/* Panel internal commands*/

#define	COMM_LD_CMD				(0x50u)
#define	COMM_LD_OFF				(0x51u)			// Master cmd
#define	COMM_LD_RED				(0x52u)			// Master cmd
#define	COMM_LD_GREEN			(0x53u)			// Master cmd
#define	COMM_LD_YELLOW			(0x54u)			// Master cmd
#define	COMM_SFL_YELLOW			(0x55u)			// Master cmd
#define	COMM_LD_REDFLASH		(0x5Au)			// Master cmd
#define	COMM_LD_GREENFLASH		(0x5Bu)			// Master cmd
#define	COMM_LD_YELLOWFLASH		(0x5Cu)			// Master cmd
#define	COMM_LD1				(0x5Du)			// Master cmd: Set Current Led = Led 1
#define	COMM_LD2				(0x5Eu)			// Master cmd: Set Current Led = Led 2
#define	COMM_LD3				(0x5Fu)			// Master cmd: Set Current Led = Led 3

#define	COMM_DEBUG_CMD			(0x60u)
#define	COMM_NO_DEBUG			(0x61u)			// Master cmd
#define	COMM_DEBUG				(0x62u)			// master cmd