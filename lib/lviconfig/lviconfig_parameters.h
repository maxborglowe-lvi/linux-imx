#ifndef LVICONFIG_PARAMETERS_H
#define LVICONFIG_PARAMETERS_H

// Kernel space definitions
#include <linux/types.h>
#include <linux/string.h> // For strcmp, strlen, etc. in kernel
#include <linux/slab.h> // For kmalloc/kfree
#include <linux/kernel.h> // For printk, KERN_ERR, etc.

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
	ConfigParam Zoom;
	ConfigParam ZoomMin;
	ConfigParam ZoomMax;
	ConfigParam ZoomSpeed;
	ConfigParam Focus;
	ConfigParam FocusMin;
	ConfigParam FocusMax;
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

typedef struct {
	const char *name;
	ConfigParam Intensity; /* Only this parameter is actually useful for regular unicolor LED-strips*/
	// ConfigParam ColorTemperature; /* Additional example parameters for e.g neopixels */
} ConfigLighting;

// Shared externs
// This is where all configuration parameters are stored.
// After initializing these variables and parsing the EEPROM, these structs will contain all of the current configuration values.
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
extern ConfigLighting confLighting;

// Platform-specific EEPROM access functions
// probably only kernel use is required
int lviconfig_platform_eeprom_write(uint32_t reg, uint8_t data);
int lviconfig_platform_eeprom_read(uint32_t reg, uint8_t *data);

ConfigParam ConfigParam_Init(const char *name, const void *data, DataType type); // name should be const
ConfigParam *ConfigParam_FindByName(const char *path);
void ConfigParam_InitAll(void);
void ConfigParam_ParseEEPROM(void);
void ConfigParam_PrintParam(ConfigParam *param); // Primarily for user-space debugging
void ConfigParam_PrintAll(void); // Primarily for user-space debugging
void ConfigParam_SetData(ConfigParam *param, const void *data);
const void *ConfigParam_GetData(ConfigParam *param);

#endif // LVICONFIG_PARAMETERS_H