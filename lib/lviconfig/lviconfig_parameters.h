#ifndef LVICONFIG_PARAMETERS_H
#define LVICONFIG_PARAMETERS_H

#ifdef __KERNEL__
// Kernel space definitions
#include <linux/types.h>
#include <linux/string.h> // For strcmp, strlen, etc. in kernel
#include <linux/slab.h> // For kmalloc/kfree
#include <linux/kernel.h> // For printk, KERN_ERR, etc.
// Forward declare i2c_client if needed for platform functions, though not directly used in this header
// struct i2c_client;
#else
// User space definitions
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#endif

// The amount of configuration structs for each type
#define CONFIG_COLOR_AMT 16
#define CONFIG_VIDEO_AMT 8
#define CONFIG_LICENSE_KEY_AMT 3
#define CONFIG_CAMERA_MODE_AMT 2
#define CONFIG_MONITOR_MODE_AMT CONFIG_CAMERA_MODE_AMT
#define TYPE_ARRAY_SIZE_MAX 16
#define CONFIG_LINE_MAX_LENGTH 256

typedef enum { TYPE_8, TYPE_16, TYPE_32, TYPE_ARRAY, TYPE_STRING } DataType;

typedef struct {
	const char *name;
	uint32_t address;
	DataType type;
	uint8_t *data;
	uint8_t size;
} ConfigParam;

typedef struct {
	const char *name;
	ConfigParam Seesaw;
	ConfigParam ReferenceLine;
	ConfigParam Curtain;
} ConfigFunctions;

typedef struct {
	const char *name;
	ConfigParam Group;
	ConfigParam ArtificialColorPoint1;
	ConfigParam ArtificialColorPoint2;
	ConfigParam ArtificialColorPoint3;
	ConfigParam ArtificialColorPoint4;
	ConfigParam CameraBackground;
	ConfigParam ReferenceLine;
	ConfigParam Restricted;
} ConfigColor;

typedef struct {
	const char *name;
	ConfigParam Type;
	ConfigParam RGain;
	ConfigParam BGain;
	ConfigParam FirstLine;
	ConfigParam UsableLines;
	ConfigParam FirstPixel;
	ConfigParam UsablePixels;
	ConfigParam UsableRatio;
	ConfigParam Interface;
	ConfigParam NaturalColorShutterTime;
	ConfigParam NaturalColorBrightness;
	ConfigParam NaturalColorIris;
	ConfigParam NaturalColorExposureCompensation;
	ConfigParam NaturalColorGainPeak;
	ConfigParam ArtificialColorShutterTime;
	ConfigParam ArtificialColorBrightness;
	ConfigParam ArtificialColorIris;
	ConfigParam ArtificialColorExposureCompensation;
	ConfigParam ArtificialColorGainPeak;
} ConfigCamera;

typedef struct {
	const char *name;
	ConfigParam ZoomMin;
	ConfigParam ZoomMax;
	ConfigParam ZoomSpeed;
	ConfigParam Focus;
	ConfigParam FocusRangeMin;
	ConfigParam FocusRangeMax;
	ConfigParam FocusSpeed;
	ConfigParam NaturalColorExposure;
	ConfigParam ArtificialColorExposure;
	ConfigParam WhiteBalance;
} ConfigCameraMode;

typedef struct {
	const char *name;
	ConfigParam ColorGainR;
	ConfigParam ColorGainG;
	ConfigParam ColorGainB;
	ConfigParam Contrast;
	ConfigParam Brightness;
	ConfigParam Saturation;
} ConfigMonitorMode;

typedef struct {
	const char *name;
	ConfigParam PaletteSelection;
	ConfigParam Type;
	ConfigParam LinesPerFrame;
	ConfigParam LinesActiveInCompletePicture;
	ConfigParam FirstActiveLine;
	ConfigParam HorizontalTotalLength;
	ConfigParam HorizontalSyncLength;
	ConfigParam HorizontalActiveVideoStart;
	ConfigParam HorizontalActiveVideoLength;
	ConfigParam VerticalSyncLength;
	ConfigParam VoltageDac;
	ConfigParam SyncRgb;
	ConfigParam AspectRatio;
} ConfigVideo;

typedef struct {
	const char *name;
	ConfigParam ReferenceLineWidth;
	ConfigParam ReferenceLineLowSpeed;
	ConfigParam ReferenceLineHighSpeed;
	ConfigParam ReferenceLineHighSpeedDelay;
	ConfigParam ReferenceLineSwitchDelay;
	ConfigParam PosNegSpeed;
} ConfigGraphics;

typedef struct {
	const char *name;
	ConfigParam LimitUp;
	ConfigParam LimitDown;
	ConfigParam LimitLeft;
	ConfigParam LimitRight;
} ConfigPTZ;

typedef struct {
	const char *name;
	ConfigParam Type;
	ConfigParam NominalCapacity;
	ConfigParam TempCoefficientCapacity;
	ConfigParam AgeCoefficientCapacity;
	ConfigParam LowCapacityLimit;
} ConfigBattery;

// Shared externs
// This is where all configuration parameters are stored.
// After parsing the lviconfig.conf file, these will be initialized with the values from the file.
extern ConfigParam confLicenseKey[CONFIG_LICENSE_KEY_AMT];
extern ConfigParam confFactoryDefaultVersion;
extern ConfigParam confProductId;
extern ConfigFunctions confFunctions;
extern ConfigColor confColor[CONFIG_COLOR_AMT];
extern ConfigCamera confCamera;
extern ConfigCameraMode confCameraMode[CONFIG_CAMERA_MODE_AMT];
extern ConfigMonitorMode confMonitorMode[CONFIG_MONITOR_MODE_AMT];
extern ConfigVideo confVideo[CONFIG_VIDEO_AMT];
extern ConfigGraphics confGraphics;
extern ConfigPTZ confPTZ;
extern ConfigBattery confBattery;

// Platform-specific EEPROM access functions
// These must be implemented by the target environment (kernel or user-space)
int platform_eeprom_write(uint32_t reg, uint8_t data);
int platform_eeprom_read(uint32_t reg, uint8_t *data);

// Core functions (shared)
ConfigParam ConfigParam_Init(const char *name, const void *data,
			     DataType type); // name should be const
void ConfigParam_InitAll(void);
ConfigParam *ConfigParam_FindByName(const char *path);
void ConfigParam_PrintParam(
	ConfigParam *param); // Primarily for user-space debugging
void ConfigParam_PrintAll(void); // Primarily for user-space debugging
void ConfigParam_SetData(ConfigParam *param, const void *data);
const void *ConfigParam_GetData(ConfigParam *param);

// Configuration parsing functions
// Kernel and user-space can use ParseBuffer if buffer is pre-loaded
void ConfigParam_ParseBuffer(const char *buffer, size_t buffer_len);
uint8_t ConfigParam_ParseSingleBuffer(const char *buffer, size_t buffer_len,
				      const char *target_key);
void ConfigParam_ParseEEPROM(void);

#ifndef __KERNEL__
// User-space only functions that handle file I/O
void ConfigParam_ParseFile(const char *filename);
uint8_t ConfigParam_ParseFileSingle(const char *filename,
				    const char *target_key);
#endif // !__KERNEL__

#endif // LVICONFIG_PARAMETERS_H