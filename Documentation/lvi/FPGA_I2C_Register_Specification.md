# FPGA I2C Register Specification
## NXTG24 System

**Revision:** 1.0  
**Date:** 2025-03-03  
**Author:** KB

---

## 📋 Table of Contents

- [Overview](#overview)
- [Status Registers](#status-registers)
- [Control Registers](#control-registers)
  - [Artificial Color Control](#artificial-color-control-registers)
  - [ID Control](#id-control-registers)
  - [User Panel Control](#user-panel-control-registers)
  - [Camera Control](#camera-control-registers)

---

## Overview

### Communication Protocol
- **Byte Order:** Big Endian
- **FPGA Slave Address:** 0x10
- **Bus Type:** I2C
- **Master Device:** Higher-level system

### Register Types
- **Status Registers** (0x80-0xCF): Read by I2C Master
- **Control Registers** (0x01-0x??): Written by I2C Master

---

## Status Registers

All status registers are read by the higher-level system (I2C Master).

### 0x80 | FPGA_FLAGS_INIT_STATUS_REG
**Type:** Read | Byte (8-bit)  
**Description:** FPGA flags status register

| Bit(s) | Name | Description |
|--------|------|-------------|
| 7-4 | Reserved | Reserved for future use |
| 3 | System PLL Lock | `0` = PLL not locked<br>`1` = PLL locked |
| 2 | Camera PLL Lock | `0` = PLL not locked<br>`1` = PLL locked |
| 1 | Camera ID Check | `0` = Camera ID mismatch<br>`1` = Camera ID match<br>Verification: FPGA compares read camera ID with configuration file ID |
| 0 | Camera Initiation | `0` = Camera not initiated<br>`1` = Camera initiated<br>Camera has moved to last saved zoom position, ready to display |

---

### 0x81 | CAM_ID_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Camera ID status register

I2C Master reads this register to identify the connected camera model.

#### Supported Camera Models

| Camera ID | Manufacturer | Model |
|-----------|--------------|-------|
| 0x465A | Wonwoo | MC-108-M3 |
| 0x0E52 | Wonwoo | MC-105 |
| 0x0E6A | Wonwoo | MM-405 |
| 0x0711 | Sony | FCB-EV9520L |
| 0x0023 | Tamron | MP1010-VC |
| 0xF017 | Tamron | MP3010M-EV |

*Note: This list will be expanded as additional cameras are supported.*

---

### 0x82-0xBF | Reserved
**Type:** Status Register Reserve

---

### 0xC0 | CAM1_ZOOM_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Direct zoom position value for Virtual Camera 1 (Reading Camera)

I2C Master reads and saves this register.

---

### 0xC1 | CAM2_ZOOM_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Direct zoom position value for Virtual Camera 2 (Distance Camera)

I2C Master reads and saves this register.

---

### 0xC2 | Reserved
**Type:** Status Register Reserve

---

### 0xC3 | CAM1_ZOOM_POS_STEP_STATUS_REG
**Type:** Read | Byte (8-bit)  
**Description:** Zoom position increment (Zoom counter steps 0-22) for Virtual Camera 1 (Reading Camera)

I2C Master reads and saves this register.

---

### 0xC4 | CAM2_ZOOM_POS_STEP_STATUS_REG
**Type:** Read | Byte (8-bit)  
**Description:** Zoom position increment (Zoom counter steps 0-22) for Virtual Camera 2 (Distance Camera)

I2C Master reads and saves this register.

---

### 0xC5 | Reserved
**Type:** Status Register Reserve

---

### 0xC6 | REFLINE_HOR_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Reference line horizontal position status

I2C Master reads and saves this register during system shutdown.

---

### 0xC7 | REFLINE_VER_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Reference line vertical position status

I2C Master reads and saves this register during system shutdown.

---

### 0xC8 | CURTAIN1_HOR_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Curtain upper horizontal position status

I2C Master reads and saves this register during system shutdown.

---

### 0xC9 | CURTAIN2_HOR_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Curtain lower horizontal position status

I2C Master reads and saves this register during system shutdown.

---

### 0xCA | CURTAIN1_VER_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Curtain left vertical position status

I2C Master reads and saves this register during system shutdown.

---

### 0xCB | CURTAIN2_VER_POS_STATUS_REG
**Type:** Read | Word (16-bit)  
**Description:** Curtain right vertical position status

I2C Master reads and saves this register during system shutdown.

---

### 0xCC | PICTURE_MODE_STATUS_REG
**Type:** Read | Byte (8-bit)  
**Description:** Generated image mode status

I2C Master reads and saves this register during system shutdown.

#### Bit Mapping

| Bit(s) | Function | Values |
|--------|----------|--------|
| 7-4 | Artificial Color Palette Counter | Total of 16 palettes (Index 0-15)<br>Palettes 12-15 are reserved |
| 3-2 | Natural Color Palette Counter | Total of 4 palettes (Index 0-3)<br>Palettes 2-3 are reserved |
| 1 | Natural ON/OFF | `0` = Artificial color mode selected<br>`1` = Natural/Monochrome color mode selected |
| 0 | Camera Flip ON/OFF | `0` = Camera in Reading mode<br>`1` = Camera in Distance mode |

#### Artificial Color Palette Mapping (Bits 7-4)

| Index | Positive | Negative |
|-------|----------|----------|
| 0000 | Art. Color 0 Black/White | 0001 Art. Color 1 White/Black |
| 0010 | Art. Color 2 Black/Yellow | 0011 Art. Color 3 Yellow/Black |
| 0100 | Art. Color 4 Black/Green | 0101 Art. Color 5 Green/Black |
| 0110 | Art. Color 6 Black/Red | 0111 Art. Color 7 Red/Black |
| 1000 | Art. Color 8 Blue/Yellow | 1001 Art. Color 9 Yellow/Blue |
| 1010 | Art. Color 10 Blue/White | 1011 Art. Color 11 White/Blue |

#### Natural Color Palette Mapping (Bits 3-2)

| Value | Mode |
|-------|------|
| 00 | Natural |
| 01 | Grey (Monochrome, Y-signal) |
| 10 | Reserved |
| 11 | Reserved |

---

### 0xCD | PN_LEVEL_STATUS_REG
**Type:** Read | Byte (8-bit)  
**Description:** Current P/N (Positive/Negative) level in artificial color mode

I2C Master reads and saves this register during system shutdown.

---

### 0xCE-0xCF | Reserved
**Type:** Status Register Reserve

---

## Control Registers

All control registers are written by the higher-level system (I2C Master).

---

## Artificial Color Control Registers

### 0x01 | ART_COL_PAL_INIT_CONTROL_REG
**Type:** Write | Multi-Byte  
**Description:** Artificial color palette control register

I2C Master writes to RAM memory in the FPGA via the FPGA's I2C slave interface.

> **⚠️ IMPORTANT:** All artificial color palettes must be calculated by the higher-level system before the I2C Master sends them to the FPGA. Documentation for this process is located in the project folder.

---

### 0x02-0x04 | Reserved
**Type:** Control Register Reserve

---

## ID Control Registers

### 0x05 | PRODUCT_ID_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** Product identity control register

I2C Master writes to this register.

#### Product ID List

| Product ID | System Platform |
|------------|-----------------|
| 0x1A | ML ZIP FHD 17" Touch NXTG24<br>Variscite DART SOM Module with LVI Carrier Board |

*Note: This list will be expanded as needed.*

---

### 0x06 | PANEL_ID_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** User panel identity control register

I2C Master writes to this register.

---

### 0x07 | CAMERA_ID_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Camera identity control register

I2C Master reads from the configuration file and writes to this register. The FPGA compares the actual read camera ID with the one from the configuration file. Match/mismatch status can be read from FPGA_FLAGS_INIT_STATUS_REG Camera ID Check Bit (Bit 1).

---

## User Panel Control Registers

### 0x08 | USER_PANEL1_CMD_CONTROL_REG
**Type:** Write | Multi-Byte  
**Description:** Control Panel 1 commands register

#### Command Reference

| Command | Data | Description | Notes |
|---------|------|-------------|-------|
| CMD_NATCOL_DN | 0x13 | Natural/Grey button pressed | ¹ |
| CMD_NATCOL_UP | 0x14 | Natural/Grey button released | |
| CMD_ARTCOL_DN | 0x15 | Artificial color button pressed | ² |
| CMD_ARTCOL_UP | 0x16 | Artificial color button released | |
| CMD_ZOOM_CW | 0x19 | Zoom increment clockwise | ³ |
| CMD_ZOOM_CCW | 0x1A | Zoom increment counter-clockwise | |
| CMD_ZOOM_PUSH_DN | 0x1B | Zoom increment button pressed | ⁴ |
| CMD_ZOOM_PUSH_UP | 0x1C | Zoom increment button released | |
| CMD_FUNCTION_CW | 0x28 | Function increment clockwise | ⁵ |
| CMD_FUNCTION_CCW | 0x29 | Function increment counter-clockwise | |
| CMD_FUNCTION_BTN_DN | 0x2A | Function button pressed | ⁶ |
| CMD_FUNCTION_BTN_UP | 0x2B | Function button released | |

#### Command Notes

**¹ Natural/Grey Button**  
Toggles between natural and grey colors.

**² Artificial Color Button**  
Cycles through artificial colors.

**³ Zoom Direction**  
- CW (Clockwise): Zoom in
- CCW (Counter-clockwise): Zoom out

**⁴ Zoom Push Button**  
Toggles between overview mode (minimum zoom) and last used zoom position.

**⁵ Multifunction Control**  
Behavior depends on ObjectShowView bit in OBJECT_SHOW_CONTROL_REG:

- **When ObjectShowView = 0:**
  - In Natural/Grey mode: Functions as contrast control
  - In Artificial color mode: Functions as P/N level control

- **When ObjectShowView = 1:**
  - Controls (moves) Reference Line/Curtain
  - Cannot simultaneously be used as Contrast/P/N control
  - To regain Contrast/P/N control, ObjectShowView must be reset to '0'
  - When reset, the selected object (Horizontal/Vertical RefLine/Curtain) will be displayed "frozen" in the position selected before ObjectShowView was reset

**⁶ Function Button Long Press**  
A long press (> 2 seconds) sets/resets the ObjectShowView bit:
- `1` = ObjectShowView activated
- `0` = ObjectShowView deactivated

---

### 0x09 | USER_PANEL1_EDGE_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** Control Panel 1 edge control (toggle function)

Controls whether toggling occurs when button is pressed (positive edge) or released (negative edge).

| Bit(s) | Function | Description |
|--------|----------|-------------|
| 7-4 | Reserved | Default set to "1111" |
| 3 | NATCOL Toggle | `1` = Toggle on button press<br>`0` = Toggle on button release |
| 2 | ARTCOL Toggle | `1` = Toggle on button press<br>`0` = Toggle on button release |
| 1 | ZOOM_PUSH Toggle | `1` = Toggle on button press<br>`0` = Toggle on button release |
| 0 | FUNCTION_PUSH Toggle | `1` = Toggle on button press<br>`0` = Toggle on button release |

**Default Value:** 0xF0  
Bits 3-0 are all set to '0', indicating these buttons toggle when released.

---

### 0x0A-0x0F | Reserved
**Type:** Control Register Reserve  
Reserved for future control panels.

---

## Camera Control Registers

### 0x10 | CAM1_ZOOM_SPEED_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Zoom speed for Virtual Camera 1 (Reading Camera)

Up/Down zoom speed is typically the same but can be set differently if desired.  
I2C Master reads from configuration file and writes to this register.

---

### 0x11 | CAM1_ZOOM_POS_MIN_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Virtual Camera 1 minimum zoom position

I2C Master reads from configuration file and writes to this register.  
This register is used when camera zoom is controlled with start zoom/stop zoom.

---

### 0x12 | CAM1_ZOOM_POS_MAX_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Virtual Camera 1 maximum zoom position

I2C Master reads from configuration file and writes to this register.  
This register is used when camera zoom is controlled with start zoom/stop zoom.

---

### 0x13 | CAM1_ZOOM_POS_INIT_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Virtual Camera 1 initial zoom position

I2C Master reads this value from saved user data.  
This register is used when camera zoom is controlled with start zoom/stop zoom.

---

### 0x14 | CAM2_ZOOM_POS_MIN_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Virtual Camera 2 minimum zoom position

I2C Master reads from configuration file and writes to this register.  
This register is used when camera zoom is controlled with start zoom/stop zoom.

---

### 0x15 | CAM2_ZOOM_POS_MAX_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Virtual Camera 2 maximum zoom position

I2C Master reads from configuration file and writes to this register.  
This register is used when camera zoom is controlled with start zoom/stop zoom.

---

### 0x16 | CAM2_ZOOM_POS_INIT_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Virtual Camera 2 initial zoom position

I2C Master reads this value from saved user data.  
This register is used when camera zoom is controlled with start zoom/stop zoom.

---

### 0x17 | CAM2_ZOOM_SPEED_CONTROL_REG
**Type:** Write | Word (16-bit)  
**Description:** Zoom speed for Virtual Camera 2 (Distance Camera)

Up/Down zoom speed is typically the same but can be set differently if desired.  
I2C Master reads from configuration file and writes to this register.

---

### 0x18-0x19 | Reserved
**Type:** Control Register Reserve

---

### 0x1A | CAM1_ZOOM_POS_INC_STEP_CONTROL_REG
**Type:** Write | Multi-Word  
**Description:** Virtual Camera 1 zoom increment steps

I2C Master reads this value from saved user data.  
These registers are used when camera zoom is controlled with zoom increments.  
Variable number (default 22) of zoom increments can create linear/non-linear zoom characteristics.

---

### 0x1B | CAM1_ZOOM_POS_INIT_INC_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** Virtual Camera 1 initial zoom increment position

I2C Master reads this value from saved user data.

> **Note:** This value is NOT a zoom position value, but an increment counter value.

---

### 0x1C | CAM2_ZOOM_POS_INC_STEP_CONTROL_REG
**Type:** Write | Multi-Word  
**Description:** Virtual Camera 2 zoom increment steps

I2C Master reads this value from saved user data.  
These registers are used when camera zoom is controlled with zoom increments.  
Variable number (default 22) of zoom increments can create linear/non-linear zoom characteristics.

---

### 0x1D | CAM2_ZOOM_POS_INIT_INC_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** Virtual Camera 2 initial zoom increment position

I2C Master reads this value from saved user data.  
This register is used when camera zoom is controlled with zoom increments.

---

### 0x1E-0x1F | Reserved
**Type:** Control Register Reserve

---

### 0x?? | ACTIVATED_FUNCTIONS_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** Activated functions control register

| Bit(s) | Function | Description |
|--------|----------|-------------|
| 7-6 | Reserved | Reserved for future use |
| 5 | Mirror | `0` = Camera image not mirrored<br>`1` = Camera image mirrored |
| 4 | Flip | `0` = Camera image not flipped<br>`1` = Camera image flipped |
| 3 | Curtain | `0` = Curtain not active<br>`1` = Curtain active |
| 2 | Reference Line | `0` = Reference line not active<br>`1` = Reference line active |
| 1 | Camera Mode Control | `0` = Controlled by illumination level<br>`1` = Controlled by flip switch |
| 0 | Camera Modes Activated | `0` = Illumination level<br>`1` = Flip switch |

---

### 0x?? | OBJECT_SHOW_CONTROL_REG
**Type:** Write  
**Description:** Object show control register (Reference Line & Curtain)

Controls ObjectShowView functionality.

---

### 0x?? | REFLINE_WIDTH_CONTROL_REG
**Type:** Write | Byte (8-bit)  
**Description:** Reference line size control

Default reference line size: 20 lines (H) / 20 pixels (V)

---

### 0x?? | REFLINE_TIME_CONTROL_REG
**Type:** Write | Multi-Byte (32-bit)  
**Description:** Reference line movement speed control

Each register value corresponds to time in milliseconds.

Register values are retrieved from configuration file and sent to FPGA as follows:

| Byte Position | Function |
|---------------|----------|
| [31...24] | Reference Line Low Speed |
| [23...16] | Reference Line High Speed |
| [15...8] | Reference Line Delay Speed |
| [7...0] | Reference Line Switch Delay |

---

### 0x?? | REFLINE_COLOUR_CONTROL_REG
**Type:** Write | Multi-Byte (32-bit)  
**Description:** Reference line color control

R G B values for the reference line are retrieved from the configuration file by the higher-level system, which then converts these values to Y Cb Cr values. The same reference line color as the background color of the displayed artificial color should be avoided.

---

### 0x?? | REFLINE_HOR_POS_MIN_CONTROL_INIT_REG
**Type:** Write | Word (16-bit)  
**Description:** Reference line horizontal minimum position

Related to the display's active image area.

---

### 0x?? | REFLINE_HOR_POS_MAX_CONTROL_INIT_REG
**Type:** Write | Word (16-bit)  
**Description:** Reference line horizontal maximum position

Related to the display's active image area.

---

### 0x?? | REFLINE_HOR_POS_START_CONTROL_INIT_REG
**Type:** Write | Word (16-bit)  
**Description:** Reference line horizontal start position

Related to the display's active image area.

---

### 0x?? | REFLINE_VER_POS_MIN_CONTROL_INIT_REG
**Type:** Write | Word (16-bit)  
**Description:** Reference line vertical minimum position

Related to the display's active image area.

---

### 0x?? | REFLINE_VER_POS_MAX_CONTROL_INIT_REG
**Type:** Write | Word (16-bit)  
**Description:** Reference line vertical maximum position

Related to the display's active image area.

---

### 0x?? | REFLINE_VER_POS_START_CONTROL_INIT_REG
**Type:** Write | Word (16-bit)  
**Description:** Reference line vertical start position

Related to the display's active image area.

---

### Curtain Position Control Registers

The following registers control curtain position parameters. All are related to the display's active image area.

| Address | Register Name | Type | Description |
|---------|---------------|------|-------------|
| 0x?? | CURTAIN1_HOR_POS_MIN_CONTROL_INIT_REG | Write, Word | Curtain horizontal minimum position |
| 0x?? | CURTAIN1_HOR_POS_MAX_CONTROL_INIT_REG | Write, Word | Curtain horizontal maximum position |
| 0x?? | CURTAIN1_HOR_POS_START_CONTROL_INIT_REG | Write, Word | Curtain horizontal start position |
| 0x?? | CURTAIN2_HOR_POS_MIN_CONTROL_INIT_REG | Write, Word | Curtain horizontal minimum position |
| 0x?? | CURTAIN2_HOR_POS_MAX_CONTROL_INIT_REG | Write, Word | Curtain horizontal maximum position |
| 0x?? | CURTAIN2_HOR_POS_START_CONTROL_INIT_REG | Write, Word | Curtain horizontal start position |
| 0x?? | CURTAIN1_VER_POS_MIN_CONTROL_INIT_REG | Write, Word | Curtain vertical minimum position |
| 0x?? | CURTAIN1_VER_POS_MAX_CONTROL_INIT_REG | Write, Word | Curtain vertical maximum position |
| 0x?? | CURTAIN1_VER_POS_START_CONTROL_INIT_REG | Write, Word | Curtain vertical start position |
| 0x?? | CURTAIN2_VER_POS_MIN_CONTROL_INIT_REG | Write, Word | Curtain vertical minimum position |
| 0x?? | CURTAIN2_VER_POS_MAX_CONTROL_INIT_REG | Write, Word | Curtain vertical maximum position |
| 0x?? | CURTAIN2_VER_POS_START_CONTROL_INIT_REG | Write, Word | Curtain vertical start position |

---

### P/N Level Control Registers - Camera 1

| Address | Register Name | Type | Description |
|---------|---------------|------|-------------|
| 0x?? | PN_LEVEL1_MIN_CONTROL_INIT_REG | Write, Byte | Virtual Camera 1 minimum P/N mode in artificial colors |
| 0x?? | PN_LEVEL1_MAX_CONTROL_INIT_REG | Write, Byte | Virtual Camera 1 maximum P/N mode in artificial colors |
| 0x?? | PN_LEVEL1_START_CONTROL_INIT_REG | Write, Byte | Virtual Camera 1 start P/N mode in artificial colors |
| 0x?? | PN_LEVEL1_CONTROL_INIT_FIXED_REG | Write, Multi-Byte | Virtual Camera 1 fixed P/N mode in artificial colors<br>Each individual artificial color (1-12) can be initialized to an optimal fixed P/N mode |
| 0x?? | PN_LEVEL1_TIME_CONTROL_REG | Write, Word | Virtual Camera 1 timing control for P/N mode in artificial colors<br>High Byte = P/N High-Speed<br>Low Byte = P/N Low-Speed |

---

### P/N Level Control Registers - Camera 2

| Address | Register Name | Type | Description |
|---------|---------------|------|-------------|
| 0x?? | PN_LEVEL2_MIN_CONTROL_INIT_REG | Write, Byte | Virtual Camera 2 minimum P/N mode in artificial colors |
| 0x?? | PN_LEVEL2_MAX_CONTROL_INIT_REG | Write, Byte | Virtual Camera 2 maximum P/N mode in artificial colors |
| 0x?? | PN_LEVEL2_START_CONTROL_INIT_REG | Write, Byte | Virtual Camera 2 start P/N mode in artificial colors |
| 0x?? | PN_LEVEL2_CONTROL_INIT_FIXED_REG | Write, Multi-Byte | Virtual Camera 2 fixed P/N mode in artificial colors<br>Each individual artificial color (1-12) can be initialized to an optimal fixed P/N mode |
| 0x?? | PN_LEVEL2_TIME_CONTROL_REG | Write, Word | Virtual Camera 2 timing control for P/N mode in artificial colors<br>High Byte = P/N High-Speed<br>Low Byte = P/N Low-Speed |

---

### Picture Mode Control Registers

| Address | Register Name | Type | Description |
|---------|---------------|------|-------------|
| 0x?? | PICTURE_MODE1_CONTROL_INIT_REG | Write, Byte | Virtual Camera 1 picture mode control |
| 0x?? | PICTURE_MODE2_CONTROL_INIT_REG | Write, Byte | Virtual Camera 2 picture mode control |

---

## Document Information

**Format Version:** 1.0  
**Last Updated:** 2025-03-03  
**Document Type:** Technical Specification  
**System:** NXTG24 FPGA I2C Interface

---

*This document is subject to change. Please refer to the project folder for the most current version and related documentation.*