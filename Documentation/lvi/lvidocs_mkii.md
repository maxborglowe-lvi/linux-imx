# LVI Linux Kernel — Developer Documentation (Mk II)

> **Platform**: Variscite DART-MX8M-PLUS (iMX8M Plus) on DT8MCustomBoard 2.x  
> **Kernel base**: NXP/Variscite `lf-5.10.y_var04`  
> **Maintainer contacts**: max.borglowe@lvi.se · johannes.bergman@lvi.se  

---

## Table of Contents

1. [Overview and Architecture](#1-overview-and-architecture)
2. [Patch History](#2-patch-history)
3. [LVI Drivers](#3-lvi-drivers)
   - 3.1 [lviconfig_parameters — Configuration Parameter Store](#31-lviconfig_parameters--configuration-parameter-store)
   - 3.2 [lviconfig_ctrl — EEPROM I2C Controller](#32-lviconfig_ctrl--eeprom-i2c-controller)
   - 3.3 [lvicam — Camera Bridge Driver](#33-lvicam--camera-bridge-driver)
   - 3.4 [lvipanel — User Panel Driver](#34-lvipanel--user-panel-driver)
   - 3.5 [lvirtc — Real-Time Clock Driver](#35-lvirtc--real-time-clock-driver)
   - 3.6 [lvipwm — PWM / LED Dimmer Driver](#36-lvipwm--pwm--led-dimmer-driver)
   - 3.7 [tps55287_pmic — Power Management IC Driver](#37-tps55287_pmic--power-management-ic-driver)
   - 3.8 [ncs8801s — LVDS-to-eDP Converter Driver](#38-ncs8801s--lvds-to-edp-converter-driver)
4. [Modified Native Drivers](#4-modified-native-drivers)
   - 4.1 [lcdifv3-common — Display Pipeline (CSC Extension)](#41-lcdifv3-common--display-pipeline-csc-extension)
   - 4.2 [imx8-isi-cap / imx8-media-dev — ISI / Media Device Patches](#42-imx8-isi-cap--imx8-media-dev--isi--media-device-patches)
   - 4.3 [evdev / mousedev / joydev — EETI Touchscreen Filtering](#43-evdev--mousedev--joydev--eeti-touchscreen-filtering)
5. [Cross-Driver Data Flow](#5-cross-driver-data-flow)
   - 5.1 [Configuration subsystem dependency graph](#51-configuration-subsystem-dependency-graph)
   - 5.2 [Camera pipeline end-to-end](#52-camera-pipeline-end-to-end)
   - 5.3 [Display pipeline end-to-end](#53-display-pipeline-end-to-end)
   - 5.4 [User panel lifecycle](#54-user-panel-lifecycle)
6. [Device Tree Integration](#6-device-tree-integration)
7. [Kconfig Symbols](#7-kconfig-symbols)
8. [Userspace Interface Summary](#8-userspace-interface-summary)
9. [Startup and Shutdown Sequence](#9-startup-and-shutdown-sequence)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Overview and Architecture

The LVI software stack extends the upstream NXP/Variscite Linux kernel with a set of custom kernel modules and targeted modifications to existing native drivers. The goal is to support an LVI industrial camera system (connected via FPGA/LVDS/MIPI-CSI) and an LVI operator panel (connected via I2C), while also managing display quality (CSC), power (PMIC), RTC, PWM-controlled lighting, and a USB touchscreen.

High-level block diagram:

```
┌──────────────────────────────────────────────────────────────────────────┐
│  User space                                                              │
│  ┌─────────────┐  ┌───────────────────────────────────────────────────┐  │
│  │  lviapp.c   │  │    GUI / camera control application               │  │
│  │ (systemd)   │  │ (reads lvicam IOCTL, writes lvipanel IOCTL)       │  │
│  └──────┬──────┘  └──────────────┬──────────────────────────────┬─────┘  │
│         │ SIGUSR1                │ IOCTL                        │ IOCTL  │
└─────────┼────────────────────────┼──────────────────────────────┼────────┘
          │                        │                              │
┌─────────▼────────────────────────▼──────────────────────────────▼────────┐
│  Kernel space — LVI drivers                                              │
│                                                                          │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                  │
│  │  lvipanel.c  │   │  lvicam.c    │   │ lcdifv3-     │                  │
│  │  lvipanel.c  │   │  lvicam.c    │   │ lcdifv3-     │                  │
│  │  lvipanel.c  │   │  lvicam.c    │   │ lcdifv3-     │                  │
│  │  (I2C misc)  │   │ (V4L2 subdev)│   │ common.c*    │                  │
│  └──────┬───────┘   └───────┬──────┘   └────────┬─────┘                  │
│         │                   │                   │                        │
│  ┌──────▼───────────────────▼───────────────────▼────────────────────┐   │
│  │               lviconfig_parameters.c  (shared symbols)            │   │
│  └──────────────────────────┬────────────────────────────────────────┘   │
│                             │                                            │
│                    ┌────────▼─────────┐                                  │
│                    │ lviconfig_ctrl.c │  ←→  EEPROM (I2C)                │
│                    └──────────────────┘                                  │
│                                                                          │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                  │
│  │  lvirtc.c    │   │  lvipwm.c    │   │ tps55287_    │                  │
│  │  (RTC/I2C)   │   │  (PWM)       │   │ pmic.c       │                  │
│  └──────────────┘   └──────────────┘   └──────────────┘                  │
│  ┌──────────────┐                                                        │
│  │ ncs8801s.c   │  ←→ LVDS-to-eDP chip (I2C)                             │
│  └──────────────┘                                                        │
└──────────────────────────────────────────────────────────────────────────┘

* lcdifv3-common.c is a modified native NXP driver.
```

---

## 2. Patch History

The LVI modifications were applied as a series of five git patches on top of the Variscite base branch. Understanding this history helps reviewers see what is LVI-authored versus upstream.

| Patch | Commit | Author | Summary |
|-------|--------|--------|---------|
| `0001` | `930f916` | Johannes Bergman | Camera support: `lvicam.c`, `tc358746.c`, LVI device tree, install script |
| `0002` | `9104fd4` | Johannes Bergman | Prototype keylogger panel driver `lvi_panel.c` (later replaced) |
| `0003` | `1122612` | Max Borglowe | EETI touchscreen filtering in `evdev.c`, `mousedev.c`, `joydev.c` |
| `0004` | `9a1d538` | Max Borglowe | Replace prototype with production `lvipanel.c`; device tree nodes for panel |
| `0005` | `609e14e` | Max Borglowe | Add `lvipanel_shutdown()` callback; update boot/shutdown I2C command sequences |

> **Note**: `lvi_panel.c` from patch 0002 was intentionally deleted in patch 0004. It is present in the patch history for reference only and must not be re-added.

---

## 3. LVI Drivers

### 3.1 lviconfig_parameters — Configuration Parameter Store

| | |
|---|---|
| **File** | `lib/lviconfig_parameters.c` |
| **Header** | `include/linux/lviconfig_parameters.h` |
| **Kconfig** | Built-in (no separate symbol; compiled as part of `lib/`) |

#### Purpose

This module defines and owns all persistent system configuration parameters. It is the single source of truth for configuration state that needs to survive reboots (stored in EEPROM). At boot it allocates kernel memory for every parameter and populates values by reading the EEPROM via a platform callback.

#### Core types

```c
typedef struct {
    const char *name;     // Human-readable name, used for lookup
    uint32_t    address;  // EEPROM byte address
    DataType    type;     // TYPE_8, TYPE_16, TYPE_32, TYPE_ARRAY, TYPE_STRING
    uint8_t    *data;     // Pointer to live in-kernel value
    uint8_t     size;     // Size in bytes
} ConfigParam;
```

`ConfigParam` structs are grouped into higher-level config structs (e.g., `ConfigMonitorMode`, `ConfigCamera`, `ConfigLighting`). These structs are declared as **exported symbols**, making them directly accessible in any kernel module that includes `lviconfig_parameters.h`.

#### Exported global instances

```c
EXPORT_SYMBOL(confLicenseKey);
EXPORT_SYMBOL(confFactoryDefaultVersion);
EXPORT_SYMBOL(confProductId);
EXPORT_SYMBOL(confFunctions);
EXPORT_SYMBOL(confColor);          // [CONFIG_COLOR_AMT]
EXPORT_SYMBOL(confCamera);
EXPORT_SYMBOL(confCameraMode);     // [CONFIG_CAMERA_MODE_AMT]
EXPORT_SYMBOL(confMonitorMode);    // [CONFIG_MONITOR_MODE_AMT]
EXPORT_SYMBOL(confVideo);          // [CONFIG_VIDEO_AMT]
EXPORT_SYMBOL(confGraphics);
EXPORT_SYMBOL(confPTZ);
EXPORT_SYMBOL(confBattery);
EXPORT_SYMBOL(confLighting);
```

#### Key API

| Function | Description |
|---|---|
| `ConfigParam_InitAll()` | Allocate and zero-initialize all parameters; must be called first |
| `ConfigParam_ParseEEPROM(read_fn)` | Populate parameters from EEPROM using the supplied read callback |
| `ConfigParam_ReadAndSaveAll(save, read_fn, write_fn)` | Read all from EEPROM, optionally write back defaults for unset entries |
| `ConfigParam_SetData(param, data, read_fn, write_fn)` | Update a single parameter in memory and optionally persist to EEPROM |
| `ConfigParam_GetData(param, read_fn)` | Read a single parameter value (from in-memory copy) |
| `ConfigParam_FindByName(path)` | Locate a `ConfigParam` by dotted name path |
| `ConfigParam_IsInitialized()` | Returns non-zero once `InitAll` has been called — used by consumers to guard against early access |

#### Dependency note

Because these symbols are exported, **any driver that reads config parameters will have a module-level dependency on `lviconfig_parameters`**. If the module is not loaded before a consumer (e.g., `lcdifv3-common`), the consumer will fall back to zero-initialized values, which may produce incorrect behavior (e.g., a black display). The load order is controlled by `MODULE_SOFTDEP` in relevant drivers and by the built-in status of this module.

---

### 3.2 lviconfig_ctrl — EEPROM I2C Controller

| | |
|---|---|
| **File** | `drivers/input/misc/lviconfig_ctrl.c` |
| **Kconfig** | `CONFIG_INPUT_LVIPANEL` (shares the misc input directory) |

#### Purpose

`lviconfig_ctrl` is the hardware access layer for the on-board EEPROM. It exposes the `platform_eeprom_read` and `platform_eeprom_write` C functions that `lviconfig_parameters` calls through its callback interface. It also exposes a character device with IOCTL commands for userspace access to individual parameters.

#### EEPROM addressing

The driver targets M24C-family I2C EEPROMs. It maps the flat address space across up to 4 EEPROM cells, each with 256 sub-addresses (1-byte internal address). The device tree node supplies the base I2C address; addresses into cells 1–3 are resolved by incrementing the I2C device address.

```
EEPROM_SUBADDRESSES = EEPROM_CELLS (4) × EEPROM_CELL_SIZE (256) = 1024 bytes
```

#### IOCTL commands

| Macro | Direction | Description |
|---|---|---|
| `IOCTL_WRITE_DATA` | `_IOW('e', 1, struct eeprom_data)` | Write raw byte at EEPROM register |
| `IOCTL_READ_DATA` | `_IOR('e', 2, struct eeprom_data)` | Read raw byte from EEPROM register |
| `IOCTL_SET_CONFIG_PARAM` | `_IOW('C', 3, struct lviconfig_ctrl_mediator)` | Write a named config parameter by string path |
| `IOCTL_GET_CONFIG_PARAM` | `_IOR('C', 4, struct lviconfig_ctrl_mediator)` | Read a named config parameter by string path |
| `IOCTL_SET_CONFIG_PARAM_EEPROM` | `_IOW('C', 5, struct lviconfig_ctrl_mediator)` | Write param and persist to EEPROM |
| `IOCTL_SAVE_ALL_CONFIG_PARAMS` | `_IO('C', 6)` | Persist all in-memory parameters to EEPROM |
| `IOCTL_READ_ALL_CONFIG_PARAMS` | `_IO('C', 7)` | Reload all parameters from EEPROM into memory |
| `IOCTL_PRINT_ALL_CONFIG_PARAMS` | `_IO('C', 8)` | Dump all parameter values to kernel log |

#### Relationship with lviconfig_parameters

`lviconfig_ctrl` **owns** the I2C client handle for the EEPROM and provides the `platform_eeprom_read`/`platform_eeprom_write` implementations. `lviconfig_parameters` is the logical layer on top — it calls these functions to serialize/deserialize data for each `ConfigParam`. Neither module is useful without the other.

---

### 3.3 lvicam — Camera Bridge Driver

| | |
|---|---|
| **File** | `drivers/media/i2c/lvicam.c` |
| **Header** | `drivers/media/i2c/lvicam.h` |
| **Kconfig** | `CONFIG_VIDEO_LVICAM=y` |
| **Soft dependency** | `tps55287_pmic` (declared via `MODULE_SOFTDEP("pre: tps55287_pmic")`) |

#### Purpose

`lvicam` is a V4L2 subdevice driver that manages the LVI camera system. It handles I2C communication with the on-board **FPGA**, which in turn drives the camera over UART via a TC358746 LVDS-to-MIPI-CSI bridge chip.

#### Camera data path

```
Camera sensor
  → LVDS cable
    → TC358746 bridge chip  (LVDS → MIPI-CSI-2; configured by tc358746.c)
      → FPGA processing
        → MIPI-CSI-2 lanes  (4 data lanes @ 594 MHz link frequency)
          → iMX8 MIPI-CSI host (imx8-mipi-csi2-sam.c)
            → ISI (Image Sensor Interface; imx8-isi-cap.c)
              → V4L2 video device → user space
```

#### Key GPIO lines

| GPIO | Role |
|---|---|
| `reset_gpio` | Hardware reset of TC358746/bridge |
| `seesaw_gpio` | Controls seesaw/reference-line optics |
| `power_gpio` | Enables power rail to camera assembly |
| `onoff_gpio` | Camera on/off signaling |

#### FPGA I2C register communication

All camera feature control (zoom, artificial color mode, focus, etc.) is done by writing specific I2C registers on the FPGA. The register map is documented in `Documentation/lvi/FPGA_I2C_Register_Specification.md`. This design allows userspace applications to implement new camera commands **without kernel changes**, by using the IOCTL channels below.

#### IOCTL interface

```c
LVICAM_CTRL_IOCTL_READ_DATA   // Read I2C data from FPGA register
LVICAM_CTRL_IOCTL_WRITE_DATA  // Write I2C data to FPGA register
```

See `Documentation/lvi/examples/lvicam_userspace_app` for a working example.

#### Interaction with lviconfig_parameters

`lvicam.c` includes `lviconfig_parameters.h` and reads camera-related config parameters (e.g., zoom limits, exposure settings from `confCamera`, `confCameraMode`) after EEPROM initialization has completed.

#### tc358746.c

The TC358746 bridge driver (`drivers/media/i2c/tc358746.c`) is a companion to `lvicam`. It is responsible for programming the Toshiba TC358746 chip — which converts the parallel/LVDS camera signal into a MIPI-CSI-2 stream the iMX8 can consume. It is registered as a V4L2 subdevice and configured from the device tree. The compatible string is `"lvi,lvicam"` / `"toshiba,tc358746"`.

---

### 3.4 lvipanel — User Panel Driver

| | |
|---|---|
| **File** | `drivers/input/misc/lvipanel.c` |
| **Header** | `drivers/input/misc/lvipanel_events.h` |
| **Events header** | `drivers/input/misc/lvipanel_peripheral_events.h` |
| **Kconfig** | `CONFIG_INPUT_LVIPANEL=y` |
| **I2C address** | `0x49` (device tree node `lvipanel@49`) |
| **I2C bus** | `i2c3` (400 kHz) |

#### Purpose

This is the kernel-space interface to the LVI operator panel. The panel communicates via I2C: it sends button and encoder events to the iMX8, and the iMX8 sends back state events (LED control, system on/off signals).

#### Architecture

The driver registers a character device (`/dev/lvipanel`) and spawns a **kernel polling thread** (`lvipanel_polling_thread`) that reads one byte from the I2C bus every 50 ms. Each received byte is matched against the `PanelEvent` enumeration:

- If it is `PANEL_EVENT_SYSTEM_SHUTDOWN`, `orderly_poweroff(true)` is called directly from the kernel thread, initiating a clean system shutdown.
- If it is any other non-zero event, a `SIGUSR1` signal is sent to the registered userspace PID. The userspace application then calls `IOCTL_READ_DATA` to fetch the byte.

#### Event types (lvipanel_events.h)

| Direction | Enum | Examples |
|---|---|---|
| **Panel → iMX8** (`PANEL_EVENT_*`) | `PanelEvent` | `PANEL_EVENT_ZOOM_CW`, `PANEL_EVENT_ONOFF_PRESS`, `PANEL_EVENT_SYSTEM_SHUTDOWN` |
| **iMX8 → Panel** (`SYSTEM_EVENT_*`) | `SystemEvent` | `SYSTEM_EVENT_LIGHT_GREEN_SOLID`, `SYSTEM_EVENT_OFF`, `SYSTEM_EVENT_ON` |

#### IOCTL interface

| Macro | Description |
|---|---|
| `IOCTL_READ_DATA _IOR('i', 1, char)` | Copy the last I2C byte received into userspace |
| `IOCTL_WRITE_DATA _IOW('i', 2, char)` | Write a `SystemEvent` byte to the panel via I2C |

#### LED feedback conventions

| Driver state | LED state set |
|---|---|
| `probe` (panel found) | `SYSTEM_EVENT_LIGHT_GREEN_SOLID` |
| `open()` (userspace app connected) | `SYSTEM_EVENT_LIGHT_GREEN_SOLID` |
| `release()` (userspace app disconnected) | `SYSTEM_EVENT_LIGHT_OFF` |
| System shutdown/halt/power-off | `SYSTEM_EVENT_LIGHT_OFF` (via reboot notifier) |

#### Shutdown handling (two paths)

1. **Panel-initiated shutdown**: The polling thread detects `PANEL_EVENT_SYSTEM_SHUTDOWN` and calls `orderly_poweroff(true)`.
2. **OS-initiated shutdown** (e.g., `systemctl poweroff`): The `lvipanel_reboot_notifier` is called by the kernel reboot chain, which sends `SYSTEM_EVENT_LIGHT_OFF` to the panel before power-off.

Additionally, the `lvipanel_shutdown()` I2C driver callback (registered on the `i2c_driver` struct) ensures that even if the module is removed without a full reboot, the panel receives `SYSTEM_EVENT_OFF` commands and the polling thread is stopped cleanly.

---

### 3.5 lvirtc — Real-Time Clock Driver

| | |
|---|---|
| **File** | `drivers/rtc/lvirtc.c` |
| **Chip** | PCF85063A (NXP) |
| **Interface** | I2C |

The `lvirtc` driver manages the hardware RTC chip on the LVI carrier board. It registers with the kernel's RTC subsystem, making the clock available via `/dev/rtc0` (or similar). Time is preserved across power cycles via the battery-backed PCF85063A. The `confBattery` config group in `lviconfig_parameters` stores battery-related calibration data used by this driver.

---

### 3.6 lvipwm — PWM / LED Dimmer Driver

| | |
|---|---|
| **File** | `drivers/pwm/lvipwm.c` |
| **Kconfig** | Part of `CONFIG_INPUT_LVIPANEL` build group |

#### Purpose

`lvipwm` controls the system LED brightness by setting the duty cycle on an iMX8 PWM channel. The duty cycle range is **0–255**. At boot, the initial duty cycle is read from `confLighting.Intensity` in `lviconfig_parameters`.

#### IOCTL interface

```c
#define PWM_IOCTL_SET_DUTY _IOW(PWM_MAGIC, 0, unsigned long)
```

Userspace writes a value in `[0, 255]` to adjust brightness at runtime. The driver translates this to the corresponding PWM register value.

#### Relationship with lviconfig_parameters

`lvipwm` reads `confLighting.Intensity.data` during `probe()` to set the power-on brightness. This is a read-only access — the driver does not write back to the config store.

---

### 3.7 tps55287_pmic — Power Management IC Driver

| | |
|---|---|
| **File** | `drivers/power/supply/tps55287_pmic.c` |
| **Chip** | Texas Instruments TPS55287 buck-boost converter |
| **Interface** | I2C + regmap |

#### Purpose

The TPS55287 is a buck-boost PMIC that provides a stable regulated output voltage for the camera assembly and display. The output voltage (`ti,vout-microvolt`) is configured via the device tree.

The driver exports the atomic variable:
```c
atomic_t tps55287_ready;
EXPORT_SYMBOL(tps55287_ready);
```

`lvicam.c` uses a `MODULE_SOFTDEP("pre: tps55287_pmic")` declaration and checks this flag to ensure the power rail is stable before initializing camera hardware.

#### Device tree configuration

```dts
tps55287: pmic@74 {
    compatible = "ti,tps55287";
    reg = <0x74>;
    ti,vout-microvolt = <5000000>; /* Example: 5V output */
};
```

---

### 3.8 ncs8801s — LVDS-to-eDP Converter Driver

| | |
|---|---|
| **File** | `drivers/video/backlight/ncs8801s.c` |
| **Header** | `drivers/video/backlight/ncs8801s.h` |
| **Chip** | NewCoSemi NCS8801S |
| **Interface** | I2C |

#### Purpose

The NCS8801S converts the LVDS signal from the iMX8 `lcdifv3` display controller into an eDP signal that drives the connected display panel.

The header file contains the register initialization array `id1_1920_1080_regs[]` which programs the chip for a specific display resolution and timing. New display configurations can be generated using the included `ncs8801s_configurator.py` / `ncs8801s_configurator.exe` tool, which produces register arrays ready to paste into the header.

#### Updating display configuration

1. Run `ncs8801s_configurator.exe` (or the Python equivalent) with the target display timing parameters.
2. Copy the generated register array into `id1_1920_1080_regs[]` in `ncs8801s.h`.
3. Rebuild and flash. The tool will prompt for confirmation that the display output is correct.

---

## 4. Modified Native Drivers

### 4.1 lcdifv3-common — Display Pipeline (CSC Extension)

| | |
|---|---|
| **File** | `drivers/gpu/imx/lcdifv3/lcdifv3-common.c` |
| **Type** | Modified NXP native driver |

#### What was added

LVI extended the NXP LCDIFv3 driver to support runtime adjustment of display quality parameters via a **Color Space Conversion (CSC)** matrix applied to the framebuffer. The matrix is written into the iMX8 LCDIFv3 CSC hardware registers.

#### CSC parameters struct

```c
struct lcdifv3_csc_params {
    int brightness;   // luminance offset
    int contrast;     // luminance scale
    int saturation;   // chroma scale
    int r_gain;       // per-channel gain — red
    int g_gain;       // per-channel gain — green
    int b_gain;       // per-channel gain — blue
};
```

The function `lcdifv3_build_color_matrix()` converts these scalar parameters into a `3×4` integer matrix, which is then applied via `lcdifv3_config_rgb_to_ycbcr()`.

#### IOCTL interface (added by LVI)

```c
#define LCDIFV3_IOC_SET_CSC  _IOW(LCDIFV3_IOC_MAGIC, 1, struct lcdifv3_csc_params)
#define LCDIFV3_IOC_GET_CSC  _IOR(LCDIFV3_IOC_MAGIC, 2, struct lcdifv3_csc_params)
```

On `LCDIFV3_IOC_SET_CSC`, the driver:
1. Copies the struct from userspace.
2. Writes individual fields back into `confMonitorMode[0]` (e.g., `confMonitorMode[0].Brightness.data`).
3. Rebuilds the color matrix.
4. Applies it to the CSC hardware registers.

On `LCDIFV3_IOC_GET_CSC`, it returns the currently active `lcdifv3_csc_params`.

#### Boot initialization

During `probe()`, the driver reads `confMonitorMode[0].Brightness.data` (and other fields) from `lviconfig_parameters` to restore the display settings that were persisted in EEPROM. If `lviconfig_parameters` is not initialized at this point (EEPROM missing/unresponsive), all fields fall back to zero, causing a **black screen**. See [Section 10](#10-troubleshooting) for diagnostics.

---

### 4.2 imx8-isi-cap / imx8-media-dev — ISI / Media Device Patches

| | |
|---|---|
| **Files** | `drivers/staging/media/imx/imx8-isi-cap.c`, `imx8-isi-m2m.c`, `imx8-media-dev.c`, `imx8-mipi-csi2-sam.c` |
| **Type** | Modified NXP staging driver |

These patches (from patch 0001) adjust the Image Sensor Interface and media device framework to correctly enumerate and route the lvicam V4L2 subdevice and the TC358746 bridge into the media graph. Key changes:

- Added `lvicam` as a recognized subdevice entity in `imx8-media-dev.c`.
- Extended `imx8-isi-cap.c` to handle the lvicam frame format (UYVY8 1920×1080) and buffer management for the 4-lane MIPI-CSI input.
- Minor fix in `imx8-mipi-csi2-sam.c` to accept the TC358746 clock-continuous mode.

---

### 4.3 evdev / mousedev / joydev — EETI Touchscreen Filtering

| | |
|---|---|
| **Files** | `drivers/input/evdev.c`, `drivers/input/mousedev.c`, `drivers/input/joydev.c` |
| **Type** | Modified kernel input subsystem |
| **Vendor ID** | `VID_EETI = 0x0EEF` |

The LVI system uses an EETI touchscreen (USB, vendor ID `0x0EEF`). Without filtering, the Linux input subsystem mistakenly routes touchscreen events through the mouse and joystick handlers in addition to the event handler, causing duplicate or conflicting input events in the Wayland compositor.

#### Fix applied (patch 0003)

A `.match` callback was added to the `evdev_handler`, `mousedev_handler`, and `joydev_handler`:

```c
// evdev.c — allows EETI through evdev normally
static bool evdev_match(...) {
    if (BUS_USB == dev->id.bustype && VID_EETI == dev->id.vendor)
        return false;  // Do not bind evdev to EETI USB device
    return true;
}

// mousedev.c — prevents EETI from appearing as /dev/input/mice
static bool mousedev_match(...) {
    if (BUS_USB == dev->id.bustype && VID_EETI == dev->id.vendor)
        return false;
    if (BUS_VIRTUAL == dev->id.bustype && VID_EETI == dev->id.vendor)
        return false;
    return true;
}

// joydev.c — prevents EETI virtual devices from appearing as joysticks
static bool joydev_match(...) {
    if (BUS_VIRTUAL == dev->id.bustype && VID_EETI == dev->id.vendor)
        return false;
    return true;
}
```

For instructions on mapping the EETI touchscreen to a specific Wayland output, see `device_instructions.md`.

---

## 5. Cross-Driver Data Flow

### 5.1 Configuration subsystem dependency graph

```
EEPROM (I2C hardware)
    ↕  platform_eeprom_read / platform_eeprom_write
lviconfig_ctrl.c   ← probed via device tree node
    ↕  function pointers passed to lviconfig_parameters API
lviconfig_parameters.c   ← compiled into lib/
    ↓  EXPORT_SYMBOL (confMonitorMode, confLighting, confCamera, ...)
    ├─→ lcdifv3-common.c    (reads confMonitorMode[0] for CSC init)
    ├─→ lvicam.c            (reads confCamera, confCameraMode)
    └─→ lvipwm.c            (reads confLighting.Intensity for duty cycle)
```

### 5.2 Camera pipeline end-to-end

```
Camera sensor (LVDS)
  → TC358746 bridge chip
      [tc358746.c, I2C @ 0x0E, i2c3]
  → MIPI-CSI-2 (4 lanes, 594 MHz)
      → imx8-mipi-csi2-sam.c
          → imx8-isi-cap.c (ISI capture)
              → V4L2 video device (/dev/video0 or similar)
                  → userspace camera app
                      ↕ IOCTL (LVICAM_CTRL_IOCTL_WRITE_DATA)
                  → lvicam.c (FPGA I2C control @ 0x0E, i2c3)
                      → FPGA (I2C commands: zoom, color mode, etc.)
                          → Camera sensor (UART via FPGA)
```

### 5.3 Display pipeline end-to-end

```
iMX8 framebuffer (DRM/KMS)
  → lcdifv3 controller
      → lcdifv3-common.c (LVI CSC extension)
          ↕ lviconfig_parameters (confMonitorMode → CSC matrix)
          ↕ IOCTL (LCDIFV3_IOC_SET_CSC from userspace)
      → LVDS output
          → NCS8801S chip (LVDS → eDP)
              [ncs8801s.c, I2C]
          → eDP display panel
```

### 5.4 User panel lifecycle

```
[Boot]
  lvipanel probe() 
    → register_reboot_notifier()
    → register_chrdev("/dev/lvipanel")
    → SYSTEM_EVENT_LIGHT_GREEN_SOLID → panel LED green

  lviapp (systemd service) opens /dev/lvipanel
    → lvipanel_open() → stores user_pid
    → polling thread: reads I2C every 50ms

[Normal operation]
  Panel button press → I2C byte → polling thread
    → non-shutdown event: SIGUSR1 → lviapp
        → lviapp: IOCTL_READ_DATA → reads byte
        → lviapp: interprets PanelEvent, calls camera IOCTL or GUI action
    → PANEL_EVENT_SYSTEM_SHUTDOWN: orderly_poweroff(true) ← kernel initiates

[OS-initiated shutdown]
  Kernel reboot chain → lvipanel_reboot_notifier()
    → SYSTEM_EVENT_LIGHT_OFF → panel LED off
  lvipanel_shutdown() callback
    → SYSTEM_EVENT_OFF (×2), COMM_STOP_SC (×2) → panel resets to standby
    → kthread_stop(polling_thread)
    → netlink_kernel_release()
```

---

## 6. Device Tree Integration

All LVI hardware is described in:
```
arch/arm64/boot/dts/freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dts
```

This file `#include`s the Variscite base DTSI and adds the following LVI-specific nodes:

| Node | Bus | Address | Driver |
|---|---|---|---|
| `lvipanel@49` | `i2c3` | `0x49` | `lvipanel.c` |
| `csi-bridge@e` | `i2c3` | `0x0e` | `lvicam.c` / `tc358746.c` |
| `lviconfig_ctrl@...` | `i2c_x` | varies | `lviconfig_ctrl.c` |
| `lvirtc@...` | `i2c_x` | varies | `lvirtc.c` |
| `tps55287@74` | `i2c_x` | `0x74` | `tps55287_pmic.c` |
| `ncs8801s@...` | `i2c_x` | varies | `ncs8801s.c` |

The `csi-bridge` node includes the MIPI-CSI endpoint with:
```dts
data-lanes = <1 2 3 4>;
clock-continuous;
link-frequencies = /bits/ 64 <594000000>;
```

A fixed 27 MHz oscillator (`clk_cam_ref`) supplies the reference clock to the TC358746.

---

## 7. Kconfig Symbols

The following Kconfig symbols control the LVI build. They are set in `arch/arm64/configs/my_defconfig.config`:

| Symbol | Default | Module |
|---|---|---|
| `CONFIG_VIDEO_LVICAM` | `y` | `lvicam.c`, `tc358746.c` |
| `CONFIG_INPUT_LVIPANEL` | `y` | `lvipanel.c`, `lviconfig_ctrl.c` |
| `CONFIG_INPUT_EVDEV` | `y` | Required for touchscreen (evdev) |
| `CONFIG_INPUT_UINPUT` | `y` | Required for virtual input devices |
| `CONFIG_HIDRAW` | `y` | Raw HID access for touchscreen |
| `CONFIG_HID_MULTITOUCH` | `y` | Multi-touch HID support |
| `CONFIG_TOUCHSCREEN_USB_COMPOSITE` | `n` | **Disabled** — conflicts with EETI driver |

---

## 8. Userspace Interface Summary

| Device node | Driver | Read | Write | Signal |
|---|---|---|---|---|
| `/dev/lvicam` | `lvicam.c` | `LVICAM_CTRL_IOCTL_READ_DATA` | `LVICAM_CTRL_IOCTL_WRITE_DATA` | — |
| `/dev/lvipanel` | `lvipanel.c` | `IOCTL_READ_DATA` | `IOCTL_WRITE_DATA` | `SIGUSR1` on new data |
| `/dev/lviconfig_ctrl` | `lviconfig_ctrl.c` | `IOCTL_GET_CONFIG_PARAM`, `IOCTL_READ_DATA` | `IOCTL_SET_CONFIG_PARAM`, `IOCTL_WRITE_DATA`, `IOCTL_SAVE_ALL_CONFIG_PARAMS` | — |
| `/dev/lcdifv3` | `lcdifv3-common.c` | `LCDIFV3_IOC_GET_CSC` | `LCDIFV3_IOC_SET_CSC` | — |
| `/dev/lvipwm` | `lvipwm.c` | — | `PWM_IOCTL_SET_DUTY` | — |
| `/dev/video0` (or similar) | V4L2 / ISI | V4L2 streaming API | — | — |

> **Camera feature commands** (zoom, color mode, etc.) are sent via the `lvicam` IOCTL using I2C register addresses defined in `Documentation/lvi/FPGA_I2C_Register_Specification.md`. No kernel changes are required to add new commands — see `Documentation/lvi/examples/lvicam_userspace_app`.

---

## 9. Startup and Shutdown Sequence

### Startup (kernel driver init order)

1. **`lviconfig_parameters`** — compiled into `lib/`; initialized before module drivers.
2. **`lviconfig_ctrl`** — probed by I2C bus; calls `ConfigParam_InitAll()` then `ConfigParam_ParseEEPROM()` to fill config values from EEPROM.
3. **`tps55287_pmic`** — sets `tps55287_ready = 1` once the power rail is stable.
4. **`lvicam`** — waits for `tps55287_ready`, then initializes TC358746 bridge and registers V4L2 subdevice.
5. **`lcdifv3-common`** — reads `confMonitorMode[0]` to build initial CSC matrix and programs display hardware.
6. **`lvipanel`** — probes at I2C address `0x49`; starts polling thread; signals green LED.
7. **`lviapp`** (userspace, via systemd) — opens `/dev/lvipanel`; begins event loop.

### Shutdown

1. User presses power button → `PANEL_EVENT_SYSTEM_SHUTDOWN` received in polling thread → `orderly_poweroff(true)`.
   *— OR —*  
   System shutdown via `systemctl`/`reboot`/`halt`.
2. Kernel reboot notifier chain: `lvipanel_reboot_notifier()` → sends `SYSTEM_EVENT_LIGHT_OFF`.
3. I2C driver `.shutdown` callback: `lvipanel_shutdown()` → sends `SYSTEM_EVENT_OFF` (×2), `COMM_STOP_SC` (×2) → stops polling thread.
4. Normal kernel driver cleanup proceeds (`remove()` callbacks).

---

## 10. Troubleshooting

### Black screen after boot

**Cause**: `lcdifv3-common.c` could not read `confMonitorMode` from `lviconfig_parameters`. This happens if `lviconfig_ctrl` failed to probe (EEPROM missing, not responding, or I2C bus error). All CSC parameters fall back to zero → fully black display.

**Diagnostics**:
```bash
dmesg | grep lviconfig
dmesg | grep lcdifv3
i2cdetect -y 3   # Check if EEPROM appears at its expected address
```

**Resolution**: Check EEPROM wiring and I2C address in device tree. If EEPROM is blank, run `IOCTL_SAVE_ALL_CONFIG_PARAMS` via `lviconfig_ctrl` to write default values.

---

### lvipanel not probing

**Symptoms**: No green LED on panel after boot; `dmesg` shows `EPROBE_DEFER` or `I2C adapter not available`.

**Diagnostics**:
```bash
dmesg | grep lvipanel
i2cdetect -y 3   # Should show device at 0x49
```

**Resolution**: Confirm `lvipanel@49` node exists in the device tree and I2C bus 3 is enabled.

---

### Camera not streaming

**Checklist**:
1. `dmesg | grep lvicam` — confirm probe succeeded and `tps55287_ready` was seen.
2. `dmesg | grep tc358746` — confirm bridge initialized.
3. Check 27 MHz reference clock is active: `cat /sys/kernel/debug/clk/clk_summary | grep cam`.
4. Verify MIPI-CSI link: `dmesg | grep mipi_csi`.
5. Confirm `tps55287_pmic` probed: `dmesg | grep tps55287`.

---

### EETI touchscreen appearing as mouse or joystick

**Cause**: The input filtering patches (patch 0003) may not be built or are not applied.

**Verification**:
```bash
cat /proc/bus/input/devices | grep -A5 EETI
# Should only show an evdev handler, not mouse/js
```

**Resolution**: Ensure `CONFIG_INPUT_EVDEV=y` and the patches in `drivers/input/evdev.c`, `mousedev.c`, `joydev.c` are present. Also make sure `CONFIG_TOUCHSCREEN_USB_COMPOSITE=n`.

---

*End of LVI Developer Documentation Mk II*
