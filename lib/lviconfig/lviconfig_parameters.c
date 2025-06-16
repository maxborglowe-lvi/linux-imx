#include "lviconfig_parameters.h"
#include "lviconfig_ctrl.h"
#ifdef __KERNEL__
#define LVI_PRINT(fmt, ...)                                                    \
	printk(KERN_INFO "lviconfig_parameters: " fmt, ##__VA_ARGS__)
#define LVI_ERR_PRINT(fmt, ...)                                                \
	printk(KERN_ERR "lviconfig_parameters: " fmt, ##__VA_ARGS__)
#define LVI_MALLOC(size) kmalloc(size, GFP_KERNEL)
#define LVI_FREE(ptr) kfree(ptr)
#define LVI_STRDUP(s) kstrdup(s, GFP_KERNEL)

#define LVI_STRTOKEN(kernel_stringp_ref, user_string_val_ignored, delim)       \
	strsep(kernel_stringp_ref, delim)

static inline int lvi_atoi(const char *s)
{
	long res;
	if (kstrtol(s, 10, &res) == 0)
		return (int)res;
	return 0;
}
#define LVI_ATOI(s) lvi_atoi(s)

#else
// User-space specific utilities
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// User-space strnchr fallback
static inline char *lvi_user_strnchr(const char *s, size_t count, int c)
{
	if (s == NULL)
		return NULL;
	for (size_t i = 0; i < count && s[i] != '\0'; ++i) // Corrected to '\0'
	{
		if (s[i] == (char)c) {
			return (char *)(s + i);
		}
	}
	return NULL;
}
#define LVI_STRNCHR(s, count, c) lvi_user_strnchr(s, count, c)

#define LVI_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LVI_ERR_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define LVI_MALLOC(size) malloc(size)
#define LVI_FREE(ptr) free(ptr)
#define LVI_STRDUP(s) strdup(s)
#define LVI_ATOI(s) atoi(s)

#define LVI_STRTOKEN(kernel_stringp_ref_ignored, user_string_val, delim)       \
	strtok(user_string_val, delim)

#endif

#ifdef __KERNEL__
// Ifdef for LVI_STRNCHR in kernel mode
#define LVI_STRNCHR(s, count, c) strnchr(s, count, c)
#endif

// Global variables
static uint32_t addr = 0;

ConfigParam confLicenseKey[CONFIG_LICENSE_KEY_AMT];
ConfigParam confFactoryDefaultVersion;
ConfigParam confProductId;
ConfigFunctions confFunctions;
ConfigColor confColor[CONFIG_COLOR_AMT];
ConfigCamera confCamera;
ConfigCameraMode confCameraMode[CONFIG_CAMERA_MODE_AMT];
ConfigMonitorMode confMonitorMode[CONFIG_MONITOR_MODE_AMT];
ConfigVideo confVideo[CONFIG_VIDEO_AMT];
ConfigGraphics confGraphics;
ConfigPTZ confPTZ;
ConfigBattery confBattery;

#ifdef __KERNEL__
EXPORT_SYMBOL(confLicenseKey);
EXPORT_SYMBOL(confFactoryDefaultVersion);
EXPORT_SYMBOL(confProductId);
EXPORT_SYMBOL(confFunctions);
EXPORT_SYMBOL(confColor);
EXPORT_SYMBOL(confCamera);
EXPORT_SYMBOL(confCameraMode);
EXPORT_SYMBOL(confMonitorMode);
EXPORT_SYMBOL(confVideo);
EXPORT_SYMBOL(confGraphics);
EXPORT_SYMBOL(confPTZ);
EXPORT_SYMBOL(confBattery);
#endif

ConfigParam ConfigParam_Init(const char *name_str, const void *data,
			     DataType type)
{
	ConfigParam param;
	param.name = LVI_STRDUP(name_str);
	if (!param.name) {
		param.size = 0;
		param.address = 0;
		param.data = NULL;
		return param;
	}
	param.address = addr;
	param.type = type;

	switch (type) {
	case TYPE_8:
		param.size = 1;
		break;
	case TYPE_16:
		param.size = 2;
		break;
	case TYPE_32:
		param.size = 4;
		break;
	case TYPE_ARRAY:
		param.size = TYPE_ARRAY_SIZE_MAX;
		break;
	case TYPE_STRING:
		if (data) {
			param.size = strlen((const char *)data) + 1;
		} else {
			param.size = 1;
		}
		break;
	default:
		LVI_FREE((void *)param.name);
		param.name = NULL;
		param.size = 0;
		param.address = 0;
		param.data = NULL;
		return param;
	}

	param.data = (uint8_t *)LVI_MALLOC(param.size);
	if (param.data == NULL) {
		LVI_FREE((void *)param.name);
		param.name = NULL;
		param.size = 0;
		param.address = 0;
		return param;
	}

	if (data) {
		switch (type) {
		case TYPE_8:
			memcpy(param.data, data, param.size);
			break;
		case TYPE_16:
			param.data[0] = ((uint16_t *)data)[0] & 0xFF;
			param.data[1] = (((uint16_t *)data)[0] >> 8) & 0xFF;
			break;
		case TYPE_32:
			param.data[0] = ((uint32_t *)data)[0] & 0xFF;
			param.data[1] = (((uint32_t *)data)[0] >> 8) & 0xFF;
			param.data[2] = (((uint32_t *)data)[0] >> 16) & 0xFF;
			param.data[3] = (((uint32_t *)data)[0] >> 24) & 0xFF;
			break;
		case TYPE_ARRAY:
			memcpy(param.data, data, param.size);
			break;
		case TYPE_STRING:
			strncpy((char *)param.data, (const char *)data,
				param.size - 1);
			param.data[param.size - 1] = '\0';
			break;
		default:
			break;
		}
	} else {
		memset(param.data, 0, param.size);
	}

	addr += param.size;
	return param;
}

void ConfigParam_InitAll(void)
{
	char name[64];
	uint32_t default_numeric_data = 0;
	const char *default_string_data = "";
	uint8_t i;

	addr = 0;

	/* INIT PRODUCT ID */
	confProductId = ConfigParam_Init("confProductId", &default_numeric_data,
					 TYPE_32);

	/* INIT FACTORY DEFAULT */
	confFactoryDefaultVersion = ConfigParam_Init(
		"confFactoryDefaultVersion", "D12345B", TYPE_STRING);

	/* INIT LICENSE KEYS */
	for (i = 0; i < CONFIG_LICENSE_KEY_AMT; i++) {
		const char *string_val;
		switch (i) {
		case 0:
			string_val = "ABCDEFGH12345678";
			break;
		case 1:
			string_val = "BCDEFGHI23456789";
			break;
		case 2:
			string_val = "CDEFGHIJ34567890";
			break;
		default:
			string_val = default_string_data;
			break;
		}
#ifdef __KERNEL__
		snprintf(name, sizeof(name), "confLicenseKey[%u]", i);
#else
		sprintf(name, "confLicenseKey[%u]", i);
#endif
		confLicenseKey[i] =
			ConfigParam_Init(name, string_val, TYPE_STRING);
	}

	/* INIT FUNCTIONS */
#ifdef __KERNEL__
	snprintf(name, sizeof(name), "confFunctions");
#else
	sprintf(name, "confFunctions");
#endif
	confFunctions.name = LVI_STRDUP(name);
	confFunctions.Seesaw =
		ConfigParam_Init("Seesaw", &default_numeric_data, TYPE_8);
	confFunctions.ReferenceLine = ConfigParam_Init(
		"ReferenceLine", &default_numeric_data, TYPE_8);
	confFunctions.Curtain =
		ConfigParam_Init("Curtain", &default_numeric_data, TYPE_8);

	/* INIT COLOR */
	for (i = 0; i < CONFIG_COLOR_AMT; i++) {
#ifdef __KERNEL__
		snprintf(name, sizeof(name), "confColor[%u]", i);
#else
		sprintf(name, "confColor[%u]", i);
#endif
		confColor[i].name = LVI_STRDUP(name);
		confColor[i].Group = ConfigParam_Init(
			"Group", &default_numeric_data, TYPE_32);
		confColor[i].ArtificialColorPoint1 =
			ConfigParam_Init("ArtificialColorPoint1",
					 &default_numeric_data, TYPE_32);
		confColor[i].ArtificialColorPoint2 =
			ConfigParam_Init("ArtificialColorPoint2",
					 &default_numeric_data, TYPE_32);
		confColor[i].ArtificialColorPoint3 =
			ConfigParam_Init("ArtificialColorPoint3",
					 &default_numeric_data, TYPE_32);
		confColor[i].ArtificialColorPoint4 =
			ConfigParam_Init("ArtificialColorPoint4",
					 &default_numeric_data, TYPE_32);
		confColor[i].CameraBackground = ConfigParam_Init(
			"CameraBackground", &default_numeric_data, TYPE_32);
		confColor[i].ReferenceLine = ConfigParam_Init(
			"ReferenceLine", &default_numeric_data, TYPE_32);
		confColor[i].Restricted = ConfigParam_Init(
			"Restricted", &default_numeric_data, TYPE_32);
	}

	/* INIT CAMERA */
#ifdef __KERNEL__
	snprintf(name, sizeof(name), "confCamera");
#else
	sprintf(name, "confCamera");
#endif
	confCamera.name = LVI_STRDUP(name);
	confCamera.Type =
		ConfigParam_Init("Type", &default_numeric_data, TYPE_8);
	confCamera.RGain =
		ConfigParam_Init("RGain", &default_numeric_data, TYPE_8);
	confCamera.BGain =
		ConfigParam_Init("BGain", &default_numeric_data, TYPE_8);
	confCamera.FirstLine =
		ConfigParam_Init("FirstLine", &default_numeric_data, TYPE_16);
	confCamera.FirstPixel =
		ConfigParam_Init("FirstPixel", &default_numeric_data, TYPE_16);
	confCamera.UsableLines =
		ConfigParam_Init("UsableLines", &default_numeric_data, TYPE_16);
	confCamera.UsablePixels = ConfigParam_Init(
		"UsablePixels", &default_numeric_data, TYPE_16);
	confCamera.UsableRatio =
		ConfigParam_Init("UsableRatio", &default_numeric_data, TYPE_16);
	confCamera.Interface =
		ConfigParam_Init("Interface", &default_numeric_data, TYPE_8);
	confCamera.NaturalColorShutterTime = ConfigParam_Init(
		"NaturalColorShutterTime", &default_numeric_data, TYPE_8);
	confCamera.NaturalColorBrightness = ConfigParam_Init(
		"NaturalColorBrightness", &default_numeric_data, TYPE_8);
	confCamera.NaturalColorIris = ConfigParam_Init(
		"NaturalColorIris", &default_numeric_data, TYPE_8);
	confCamera.NaturalColorExposureCompensation =
		ConfigParam_Init("NaturalColorExposureCompensation",
				 &default_numeric_data, TYPE_8);
	confCamera.NaturalColorGainPeak = ConfigParam_Init(
		"NaturalColorGainPeak", &default_numeric_data, TYPE_8);
	confCamera.ArtificialColorShutterTime = ConfigParam_Init(
		"ArtificialColorShutterTime", &default_numeric_data, TYPE_8);
	confCamera.ArtificialColorBrightness = ConfigParam_Init(
		"ArtificialColorBrightness", &default_numeric_data, TYPE_8);
	confCamera.ArtificialColorIris = ConfigParam_Init(
		"ArtificialColorIris", &default_numeric_data, TYPE_8);
	confCamera.ArtificialColorExposureCompensation =
		ConfigParam_Init("ArtificialColorExposureCompensation",
				 &default_numeric_data, TYPE_8);
	confCamera.ArtificialColorGainPeak = ConfigParam_Init(
		"ArtificialColorGainPeak", &default_numeric_data, TYPE_8);

	/* INIT CAMERA MODE */
	for (i = 0; i < CONFIG_CAMERA_MODE_AMT; i++) {
#ifdef __KERNEL__
		snprintf(name, sizeof(name), "confCameraMode[%u]", i);
#else
		sprintf(name, "confCameraMode[%u]", i);
#endif
		confCameraMode[i].name = LVI_STRDUP(name);
		confCameraMode[i].ZoomMin = ConfigParam_Init(
			"ZoomMin", &default_numeric_data, TYPE_16);
		confCameraMode[i].ZoomMax = ConfigParam_Init(
			"ZoomMax", &default_numeric_data, TYPE_16);
		confCameraMode[i].ZoomSpeed = ConfigParam_Init(
			"ZoomSpeed", &default_numeric_data, TYPE_16);
		confCameraMode[i].Focus = ConfigParam_Init(
			"Focus", &default_numeric_data, TYPE_32);
		confCameraMode[i].FocusRangeMin = ConfigParam_Init(
			"FocusRangeMin", &default_numeric_data, TYPE_32);
		confCameraMode[i].FocusRangeMax = ConfigParam_Init(
			"FocusRangeMax", &default_numeric_data, TYPE_32);
		confCameraMode[i].FocusSpeed = ConfigParam_Init(
			"FocusSpeed", &default_numeric_data, TYPE_32);
		confCameraMode[i].NaturalColorExposure = ConfigParam_Init(
			"NaturalColorExposure", &default_numeric_data, TYPE_32);
		confCameraMode[i].ArtificialColorExposure =
			ConfigParam_Init("ArtificialColorExposure",
					 &default_numeric_data, TYPE_32);
		confCameraMode[i].WhiteBalance = ConfigParam_Init(
			"WhiteBalance", &default_numeric_data, TYPE_32);
	}

	/* INIT MONITOR MODE */
	for (i = 0; i < CONFIG_MONITOR_MODE_AMT; i++) {
#ifdef __KERNEL__
		snprintf(name, sizeof(name), "confMonitorMode[%u]", i);
#else
		sprintf(name, "confMonitorMode[%u]", i);
#endif
		confMonitorMode[i].name = LVI_STRDUP(name);
		confMonitorMode[i].ColorGainR = ConfigParam_Init(
			"ColorGainR", &default_numeric_data, TYPE_8);
		confMonitorMode[i].ColorGainG = ConfigParam_Init(
			"ColorGainG", &default_numeric_data, TYPE_8);
		confMonitorMode[i].ColorGainB = ConfigParam_Init(
			"ColorGainB", &default_numeric_data, TYPE_8);
		confMonitorMode[i].Contrast = ConfigParam_Init(
			"Contrast", &default_numeric_data, TYPE_8);
		confMonitorMode[i].Brightness = ConfigParam_Init(
			"Brightness", &default_numeric_data, TYPE_8);
		confMonitorMode[i].Saturation = ConfigParam_Init(
			"Saturation", &default_numeric_data, TYPE_8);
	}

	/* INIT VIDEO */
	for (i = 0; i < CONFIG_VIDEO_AMT; i++) {
#ifdef __KERNEL__
		snprintf(name, sizeof(name), "confVideo[%u]", i);
#else
		sprintf(name, "confVideo[%u]", i);
#endif
		confVideo[i].name = LVI_STRDUP(name);
		confVideo[i].PaletteSelection = ConfigParam_Init(
			"PaletteSelection", &default_numeric_data, TYPE_8);
		confVideo[i].Type =
			ConfigParam_Init("Type", &default_numeric_data, TYPE_8);
		confVideo[i].LinesPerFrame = ConfigParam_Init(
			"LinesPerFrame", &default_numeric_data, TYPE_16);
		confVideo[i].LinesActiveInCompletePicture =
			ConfigParam_Init("LinesActiveInCompletePicture",
					 &default_numeric_data, TYPE_16);
		confVideo[i].FirstActiveLine = ConfigParam_Init(
			"FirstActiveLine", &default_numeric_data, TYPE_8);
		confVideo[i].HorizontalTotalLength =
			ConfigParam_Init("HorizontalTotalLength",
					 &default_numeric_data, TYPE_16);
		confVideo[i].HorizontalSyncLength = ConfigParam_Init(
			"HorizontalSyncLength", &default_numeric_data, TYPE_8);
		confVideo[i].HorizontalActiveVideoStart =
			ConfigParam_Init("HorizontalActiveVideoStart",
					 &default_numeric_data, TYPE_8);
		confVideo[i].HorizontalActiveVideoLength =
			ConfigParam_Init("HorizontalActiveVideoLength",
					 &default_numeric_data, TYPE_16);
		confVideo[i].VerticalSyncLength = ConfigParam_Init(
			"VerticalSyncLength", &default_numeric_data, TYPE_8);
		confVideo[i].VoltageDac = ConfigParam_Init(
			"VoltageDac", &default_numeric_data, TYPE_8);
		confVideo[i].SyncRgb = ConfigParam_Init(
			"SyncRgb", &default_numeric_data, TYPE_8);
		confVideo[i].AspectRatio = ConfigParam_Init(
			"AspectRatio", &default_numeric_data, TYPE_16);
	}

	/* INIT GRAPHICS */
#ifdef __KERNEL__
	snprintf(name, sizeof(name), "confGraphics");
#else
	sprintf(name, "confGraphics");
#endif
	confGraphics.name = LVI_STRDUP(name);
	confGraphics.ReferenceLineWidth = ConfigParam_Init(
		"ReferenceLineWidth", &default_numeric_data, TYPE_16);
	confGraphics.ReferenceLineLowSpeed = ConfigParam_Init(
		"ReferenceLineLowSpeed", &default_numeric_data, TYPE_16);
	confGraphics.ReferenceLineHighSpeed = ConfigParam_Init(
		"ReferenceLineHighSpeed", &default_numeric_data, TYPE_16);
	confGraphics.ReferenceLineHighSpeedDelay = ConfigParam_Init(
		"ReferenceLineHighSpeedDelay", &default_numeric_data, TYPE_16);
	confGraphics.ReferenceLineSwitchDelay = ConfigParam_Init(
		"ReferenceLineSwitchDelay", &default_numeric_data, TYPE_16);
	confGraphics.PosNegSpeed =
		ConfigParam_Init("PosNegSpeed", &default_numeric_data, TYPE_16);

	/* INIT PTZ */
#ifdef __KERNEL__
	snprintf(name, sizeof(name), "confPTZ");
#else
	sprintf(name, "confPTZ");
#endif
	confPTZ.name = LVI_STRDUP(name);
	confPTZ.LimitUp =
		ConfigParam_Init("LimitUp", &default_numeric_data, TYPE_32);
	confPTZ.LimitDown =
		ConfigParam_Init("LimitDown", &default_numeric_data, TYPE_32);
	confPTZ.LimitLeft =
		ConfigParam_Init("LimitLeft", &default_numeric_data, TYPE_32);
	confPTZ.LimitRight =
		ConfigParam_Init("LimitRight", &default_numeric_data, TYPE_32);

	/* INIT BATTERY */
#ifdef __KERNEL__
	snprintf(name, sizeof(name), "confBattery");
#else
	sprintf(name, "confBattery");
#endif
	confBattery.name = LVI_STRDUP(name);
	confBattery.Type =
		ConfigParam_Init("Type", &default_numeric_data, TYPE_8);
	confBattery.NominalCapacity = ConfigParam_Init(
		"NominalCapacity", &default_numeric_data, TYPE_16);
	confBattery.TempCoefficientCapacity = ConfigParam_Init(
		"TempCoefficientCapacity", &default_numeric_data, TYPE_16);
	confBattery.AgeCoefficientCapacity = ConfigParam_Init(
		"AgeCoefficientCapacity", &default_numeric_data, TYPE_16);
	confBattery.LowCapacityLimit = ConfigParam_Init(
		"LowCapacityLimit", &default_numeric_data, TYPE_16);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(ConfigParam_InitAll);
#endif

ConfigParam *ConfigParam_FindByName(const char *path)
{
	char *path_copy;
	char *token;
	ConfigParam *param = NULL;
	int index = 0;
	char *strtok_next_str;
	char *strsep_current_ptr;

	path_copy = LVI_STRDUP(path);
	if (!path_copy) {
		return NULL;
	}

	strtok_next_str = path_copy;
	strsep_current_ptr = path_copy;

	token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str, ".[]");
	strtok_next_str = NULL;

	if (!token) {
		LVI_FREE(path_copy);
		return NULL;
	}

	if (strcmp(token, "confLicenseKey") == 0) {
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str,
				     "[]");
		if (token) {
			index = LVI_ATOI(token);
			if (index >= 0 && index < CONFIG_LICENSE_KEY_AMT) {
				param = &confLicenseKey[index];
			}
		}
	} else if (strcmp(token, "confFactoryDefaultVersion") == 0) {
		param = &confFactoryDefaultVersion;
	} else if (strcmp(token, "confProductId") == 0) {
		param = &confProductId;
	} else if (strcmp(token, "confFunctions") == 0) {
#ifdef __KERNEL__
		if (strsep_current_ptr && *strsep_current_ptr == '.') {
			strsep_current_ptr++; // Consume the dot
		}
		// Check if the rest of the string is empty or if we are at the end
		if (!strsep_current_ptr || *strsep_current_ptr == '\0') {
			token = NULL; // No more sub-fields
		} else {
			token = strsep(&strsep_current_ptr, ".");
		}
#else
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str, ".");
#endif
		if (token) {
			if (strcmp(token, "Seesaw") == 0)
				param = &confFunctions.Seesaw;
			else if (strcmp(token, "ReferenceLine") == 0)
				param = &confFunctions.ReferenceLine;
			else if (strcmp(token, "Curtain") == 0)
				param = &confFunctions.Curtain;
		}
	} else if (strcmp(token, "confColor") == 0) {
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str,
				     "[]"); // Get index
		if (token) {
			index = LVI_ATOI(token);
			if (index >= 0 && index < CONFIG_COLOR_AMT) {
#ifdef __KERNEL__
				if (strsep_current_ptr &&
				    *strsep_current_ptr == '.') {
					strsep_current_ptr++;
				}
				if (!strsep_current_ptr ||
				    *strsep_current_ptr == '\0') // FIX HERE
				{
					token = NULL;
				} else {
					token = strsep(&strsep_current_ptr,
						       ".");
				}
#else
				token = LVI_STRTOKEN(&strsep_current_ptr,
						     strtok_next_str, ".");
#endif
				if (token) {
					if (strcmp(token, "Group") == 0)
						param = &confColor[index].Group;
					else if (strcmp(token,
							"ArtificialColorPoint1") ==
						 0)
						param = &confColor[index]
								 .ArtificialColorPoint1;
					else if (strcmp(token,
							"ArtificialColorPoint2") ==
						 0)
						param = &confColor[index]
								 .ArtificialColorPoint2;
					else if (strcmp(token,
							"ArtificialColorPoint3") ==
						 0)
						param = &confColor[index]
								 .ArtificialColorPoint3;
					else if (strcmp(token,
							"ArtificialColorPoint4") ==
						 0)
						param = &confColor[index]
								 .ArtificialColorPoint4;
					else if (strcmp(token,
							"CameraBackground") ==
						 0)
						param = &confColor[index]
								 .CameraBackground;
					else if (strcmp(token,
							"ReferenceLine") == 0)
						param = &confColor[index]
								 .ReferenceLine;
					else if (strcmp(token, "Restricted") ==
						 0)
						param = &confColor[index]
								 .Restricted;
				}
			}
		}
	} else if (strcmp(token, "confCamera") == 0) {
#ifdef __KERNEL__
		if (strsep_current_ptr && *strsep_current_ptr == '.') {
			strsep_current_ptr++; // Consume the dot
		}
		// Check if the rest of the string is empty or if we are at the end
		if (!strsep_current_ptr ||
		    *strsep_current_ptr ==
			    '\0') // This was one of the warning lines
		{
			token = NULL; // No more sub-fields
		} else {
			token = strsep(&strsep_current_ptr, ".");
		}
#else
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str, ".");
#endif
		if (token) {
			if (strcmp(token, "Type") == 0)
				param = &confCamera.Type;
			else if (strcmp(token, "RGain") == 0)
				param = &confCamera.RGain;
			else if (strcmp(token, "BGain") == 0)
				param = &confCamera.BGain;
			else if (strcmp(token, "FirstLine") == 0)
				param = &confCamera.FirstLine;
			else if (strcmp(token, "UsableLines") == 0)
				param = &confCamera.UsableLines;
			else if (strcmp(token, "FirstPixel") == 0)
				param = &confCamera.FirstPixel;
			else if (strcmp(token, "UsablePixels") == 0)
				param = &confCamera.UsablePixels;
			else if (strcmp(token, "UsableRatio") == 0)
				param = &confCamera.UsableRatio;
			else if (strcmp(token, "Interface") == 0)
				param = &confCamera.Interface;
			else if (strcmp(token, "ArtificialColorShutterTime") ==
				 0)
				param = &confCamera.ArtificialColorShutterTime;
			else if (strcmp(token, "ArtificialColorBrightness") ==
				 0)
				param = &confCamera.ArtificialColorBrightness;
			else if (strcmp(token, "ArtificialColorIris") == 0)
				param = &confCamera.ArtificialColorIris;
			else if (strcmp(token,
					"ArtificialColorExposureCompensation") ==
				 0)
				param = &confCamera
						 .ArtificialColorExposureCompensation;
			// Added missing confCamera parameters based on error log
			else if (strcmp(token, "NaturalColorShutterTime") == 0)
				param = &confCamera.NaturalColorShutterTime;
			else if (strcmp(token, "NaturalColorBrightness") == 0)
				param = &confCamera.NaturalColorBrightness;
			else if (strcmp(token, "NaturalColorIris") == 0)
				param = &confCamera.NaturalColorIris;
			else if (strcmp(token,
					"NaturalColorExposureCompensation") ==
				 0)
				param = &confCamera
						 .NaturalColorExposureCompensation;
			else if (strcmp(token, "NaturalColorGainPeak") == 0)
				param = &confCamera.NaturalColorGainPeak;
			else if (strcmp(token, "ArtificialColorGainPeak") == 0)
				param = &confCamera.ArtificialColorGainPeak;
		}
	}
	// Added missing sections based on error log
	else if (strcmp(token, "confCameraMode") == 0) {
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str,
				     "[]"); // Get index
		if (token) {
			index = LVI_ATOI(token);
			if (index >= 0 && index < CONFIG_CAMERA_MODE_AMT) {
#ifdef __KERNEL__
				if (strsep_current_ptr &&
				    *strsep_current_ptr == '.') {
					strsep_current_ptr++;
				}
				if (!strsep_current_ptr ||
				    *strsep_current_ptr == '\0') // FIX HERE
				{
					token = NULL;
				} else {
					token = strsep(&strsep_current_ptr,
						       ".");
				}
#else
				token = LVI_STRTOKEN(&strsep_current_ptr,
						     strtok_next_str, ".");
#endif
				if (token) {
					if (strcmp(token, "ZoomMin") == 0)
						param = &confCameraMode[index]
								 .ZoomMin;
					else if (strcmp(token, "ZoomMax") == 0)
						param = &confCameraMode[index]
								 .ZoomMax;
					else if (strcmp(token, "ZoomSpeed") ==
						 0)
						param = &confCameraMode[index]
								 .ZoomSpeed;
					else if (strcmp(token, "Focus") == 0)
						param = &confCameraMode[index]
								 .Focus;
					else if (strcmp(token,
							"FocusRangeMin") == 0)
						param = &confCameraMode[index]
								 .FocusRangeMin;
					else if (strcmp(token,
							"FocusRangeMax") == 0)
						param = &confCameraMode[index]
								 .FocusRangeMax;
					else if (strcmp(token, "FocusSpeed") ==
						 0)
						param = &confCameraMode[index]
								 .FocusSpeed;
					else if (strcmp(token,
							"NaturalColorExposure") ==
						 0)
						param = &confCameraMode[index]
								 .NaturalColorExposure;
					else if (strcmp(token,
							"ArtificialColorExposure") ==
						 0)
						param = &confCameraMode[index]
								 .ArtificialColorExposure;
					else if (strcmp(token,
							"WhiteBalance") == 0)
						param = &confCameraMode[index]
								 .WhiteBalance;
				}
			}
		}
	} else if (strcmp(token, "confMonitorMode") == 0) {
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str,
				     "[]"); // Get index
		if (token) {
			index = LVI_ATOI(token);
			if (index >= 0 && index < CONFIG_MONITOR_MODE_AMT) {
#ifdef __KERNEL__
				if (strsep_current_ptr &&
				    *strsep_current_ptr == '.') {
					strsep_current_ptr++;
				}
				if (!strsep_current_ptr ||
				    *strsep_current_ptr == '\0') // FIX HERE
				{
					token = NULL;
				} else {
					token = strsep(&strsep_current_ptr,
						       ".");
				}
#else
				token = LVI_STRTOKEN(&strsep_current_ptr,
						     strtok_next_str, ".");
#endif
				if (token) {
					if (strcmp(token, "ColorGainR") == 0)
						param = &confMonitorMode[index]
								 .ColorGainR;
					else if (strcmp(token, "ColorGainG") ==
						 0)
						param = &confMonitorMode[index]
								 .ColorGainG;
					else if (strcmp(token, "ColorGainB") ==
						 0)
						param = &confMonitorMode[index]
								 .ColorGainB;
					else if (strcmp(token, "Contrast") == 0)
						param = &confMonitorMode[index]
								 .Contrast;
					else if (strcmp(token, "Brightness") ==
						 0)
						param = &confMonitorMode[index]
								 .Brightness;
					else if (strcmp(token, "Saturation") ==
						 0)
						param = &confMonitorMode[index]
								 .Saturation;
				}
			}
		}
	} else if (strcmp(token, "confVideo") == 0) {
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str,
				     "[]"); // Get index
		if (token) {
			index = LVI_ATOI(token);
			if (index >= 0 && index < CONFIG_VIDEO_AMT) {
#ifdef __KERNEL__
				if (strsep_current_ptr &&
				    *strsep_current_ptr == '.') {
					strsep_current_ptr++;
				}
				if (!strsep_current_ptr ||
				    *strsep_current_ptr == '\0') // FIX HERE
				{
					token = NULL;
				} else {
					token = strsep(&strsep_current_ptr,
						       ".");
				}
#else
				token = LVI_STRTOKEN(&strsep_current_ptr,
						     strtok_next_str, ".");
#endif
				if (token) {
					if (strcmp(token, "PaletteSelection") ==
					    0)
						param = &confVideo[index]
								 .PaletteSelection;
					else if (strcmp(token, "Type") == 0)
						param = &confVideo[index].Type;
					else if (strcmp(token,
							"LinesPerFrame") == 0)
						param = &confVideo[index]
								 .LinesPerFrame;
					else if (strcmp(token,
							"LinesActiveInCompletePicture") ==
						 0)
						param = &confVideo[index]
								 .LinesActiveInCompletePicture;
					else if (strcmp(token,
							"FirstActiveLine") == 0)
						param = &confVideo[index]
								 .FirstActiveLine;
					else if (strcmp(token,
							"HorizontalTotalLength") ==
						 0)
						param = &confVideo[index]
								 .HorizontalTotalLength;
					else if (strcmp(token,
							"HorizontalSyncLength") ==
						 0)
						param = &confVideo[index]
								 .HorizontalSyncLength;
					else if (strcmp(token,
							"HorizontalActiveVideoStart") ==
						 0)
						param = &confVideo[index]
								 .HorizontalActiveVideoStart;
					else if (strcmp(token,
							"HorizontalActiveVideoLength") ==
						 0)
						param = &confVideo[index]
								 .HorizontalActiveVideoLength;
					else if (strcmp(token,
							"VerticalSyncLength") ==
						 0)
						param = &confVideo[index]
								 .VerticalSyncLength;
					else if (strcmp(token, "VoltageDac") ==
						 0)
						param = &confVideo[index]
								 .VoltageDac;
					else if (strcmp(token, "SyncRgb") == 0)
						param = &confVideo[index]
								 .SyncRgb;
					else if (strcmp(token, "AspectRatio") ==
						 0)
						param = &confVideo[index]
								 .AspectRatio;
				}
			}
		}
	} else if (strcmp(token, "confGraphics") == 0) {
#ifdef __KERNEL__
		if (strsep_current_ptr && *strsep_current_ptr == '.') {
			strsep_current_ptr++;
		}
		if (!strsep_current_ptr || *strsep_current_ptr == '\0') {
			token = NULL;
		} else {
			token = strsep(&strsep_current_ptr, ".");
		}
#else
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str, ".");
#endif
		if (token) {
			if (strcmp(token, "ReferenceLineWidth") == 0)
				param = &confGraphics.ReferenceLineWidth;
			else if (strcmp(token, "ReferenceLineLowSpeed") == 0)
				param = &confGraphics.ReferenceLineLowSpeed;
			else if (strcmp(token, "ReferenceLineHighSpeed") == 0)
				param = &confGraphics.ReferenceLineHighSpeed;
			else if (strcmp(token, "ReferenceLineHighSpeedDelay") ==
				 0)
				param = &confGraphics
						 .ReferenceLineHighSpeedDelay;
			else if (strcmp(token, "ReferenceLineSwitchDelay") == 0)
				param = &confGraphics.ReferenceLineSwitchDelay;
			else if (strcmp(token, "PosNegSpeed") == 0)
				param = &confGraphics.PosNegSpeed;
		}
	} else if (strcmp(token, "confPTZ") == 0) {
#ifdef __KERNEL__
		if (strsep_current_ptr && *strsep_current_ptr == '.') {
			strsep_current_ptr++;
		}
		if (!strsep_current_ptr || *strsep_current_ptr == '\0') {
			token = NULL;
		} else {
			token = strsep(&strsep_current_ptr, ".");
		}
#else
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str, ".");
#endif
		if (token) {
			if (strcmp(token, "LimitUp") == 0)
				param = &confPTZ.LimitUp;
			else if (strcmp(token, "LimitDown") == 0)
				param = &confPTZ.LimitDown;
			else if (strcmp(token, "LimitLeft") == 0)
				param = &confPTZ.LimitLeft;
			else if (strcmp(token, "LimitRight") == 0)
				param = &confPTZ.LimitRight;
		}
	} else if (strcmp(token, "confBattery") == 0) {
#ifdef __KERNEL__
		if (strsep_current_ptr && *strsep_current_ptr == '.') {
			strsep_current_ptr++;
		}
		if (!strsep_current_ptr || *strsep_current_ptr == '\0') {
			token = NULL;
		} else {
			token = strsep(&strsep_current_ptr, ".");
		}
#else
		token = LVI_STRTOKEN(&strsep_current_ptr, strtok_next_str, ".");
#endif
		if (token) {
			if (strcmp(token, "Type") == 0)
				param = &confBattery.Type;
			else if (strcmp(token, "NominalCapacity") == 0)
				param = &confBattery.NominalCapacity;
			else if (strcmp(token, "TempCoefficientCapacity") == 0)
				param = &confBattery.TempCoefficientCapacity;
			else if (strcmp(token, "AgeCoefficientCapacity") == 0)
				param = &confBattery.AgeCoefficientCapacity;
			else if (strcmp(token, "LowCapacityLimit") == 0)
				param = &confBattery.LowCapacityLimit;
		}
	}
	/* Add other config sections as needed */

	LVI_FREE(path_copy);
	return param;
}

void ConfigParam_SetData(ConfigParam *param, const void *data)
{
	uint8_t i;

	if (!param || !param->data || !data)
		return;

	switch (param->type) {
	case TYPE_8:
		memcpy(param->data, data, param->size);
		break;
	case TYPE_16:
		param->data[0] = ((uint16_t *)data)[0] & 0xFF;
		param->data[1] = (((uint16_t *)data)[0] >> 8) & 0xFF;
		break;
	case TYPE_32:
		param->data[0] = ((uint32_t *)data)[0] & 0xFF;
		param->data[1] = (((uint32_t *)data)[0] >> 8) & 0xFF;
		param->data[2] = (((uint32_t *)data)[0] >> 16) & 0xFF;
		param->data[3] = (((uint32_t *)data)[0] >> 24) & 0xFF;
		break;
	case TYPE_ARRAY:
		memcpy(param->data, data, param->size);
		break;
	case TYPE_STRING:
		strncpy((char *)param->data, (const char *)data,
			param->size - 1);
		param->data[param->size - 1] = '\0';
		break;
	default:
		return;
	}

	for (i = 0; i < param->size; i++) {
		if (platform_eeprom_write(param->address + i, param->data[i]) !=
		    0) {
#ifdef DEBUG
			LVI_ERR_PRINT("EEPROM write failed at address 0x%04x\n",
				      param->address + i);
#endif
		}
	}
}

/**
 * Reads the data from the EEPROM for a given ConfigParam.
 * Returns a pointer to the data if successful, or NULL if there was an error.
 */
const void *ConfigParam_GetData(ConfigParam *param)
{
	uint8_t read_success;
	uint8_t i;

	if (param == NULL || param->data == NULL)
		return NULL;

	read_success = 1;
	for (i = 0; i < param->size; i++) {
		if (platform_eeprom_read(param->address + i, &param->data[i]) !=
		    0) {
			read_success = 0;
#ifdef DEBUG
			LVI_ERR_PRINT("EEPROM read failed at address 0x%04x\n",
				      param->address + i);
#endif
			break;
		}
	}

	if (!read_success) {
		return NULL;
	}
	return (const void *)param->data;
}

void ConfigParam_ParseEEPROM(void)
{
	uint8_t i;

	// confProductId
	ConfigParam_GetData(&confProductId);

	// confFactoryDefaultVersion
	ConfigParam_GetData(&confFactoryDefaultVersion);

	// confLicenseKey
	for (i = 0; i < CONFIG_LICENSE_KEY_AMT; i++) {
		ConfigParam_GetData(&confLicenseKey[i]);
	}

	// confFunctions
	ConfigParam_GetData(&confFunctions.Seesaw);
	ConfigParam_GetData(&confFunctions.ReferenceLine);
	ConfigParam_GetData(&confFunctions.Curtain);

	// confColor
	for (i = 0; i < CONFIG_COLOR_AMT; i++) {
		ConfigParam_GetData(&confColor[i].Group);
		ConfigParam_GetData(&confColor[i].ArtificialColorPoint1);
		ConfigParam_GetData(&confColor[i].ArtificialColorPoint2);
		ConfigParam_GetData(&confColor[i].ArtificialColorPoint3);
		ConfigParam_GetData(&confColor[i].ArtificialColorPoint4);
		ConfigParam_GetData(&confColor[i].CameraBackground);
		ConfigParam_GetData(&confColor[i].ReferenceLine);
		ConfigParam_GetData(&confColor[i].Restricted);
	}

	// confCamera
	ConfigParam_GetData(&confCamera.Type);
	ConfigParam_GetData(&confCamera.RGain);
	ConfigParam_GetData(&confCamera.BGain);
	ConfigParam_GetData(&confCamera.FirstLine);
	ConfigParam_GetData(&confCamera.FirstPixel);
	ConfigParam_GetData(&confCamera.UsableLines);
	ConfigParam_GetData(&confCamera.UsablePixels);
	ConfigParam_GetData(&confCamera.UsableRatio);
	ConfigParam_GetData(&confCamera.Interface);
	ConfigParam_GetData(&confCamera.NaturalColorShutterTime);
	ConfigParam_GetData(&confCamera.NaturalColorBrightness);
	ConfigParam_GetData(&confCamera.NaturalColorIris);
	ConfigParam_GetData(&confCamera.NaturalColorExposureCompensation);
	ConfigParam_GetData(&confCamera.NaturalColorGainPeak);
	ConfigParam_GetData(&confCamera.ArtificialColorShutterTime);
	ConfigParam_GetData(&confCamera.ArtificialColorBrightness);
	ConfigParam_GetData(&confCamera.ArtificialColorIris);
	ConfigParam_GetData(&confCamera.ArtificialColorExposureCompensation);
	ConfigParam_GetData(&confCamera.ArtificialColorGainPeak);

	// confCameraMode
	for (i = 0; i < CONFIG_CAMERA_MODE_AMT; i++) {
		ConfigParam_GetData(&confCameraMode[i].ZoomMin);
		ConfigParam_GetData(&confCameraMode[i].ZoomMax);
		ConfigParam_GetData(&confCameraMode[i].ZoomSpeed);
		ConfigParam_GetData(&confCameraMode[i].Focus);
		ConfigParam_GetData(&confCameraMode[i].FocusRangeMin);
		ConfigParam_GetData(&confCameraMode[i].FocusRangeMax);
		ConfigParam_GetData(&confCameraMode[i].FocusSpeed);
		ConfigParam_GetData(&confCameraMode[i].NaturalColorExposure);
		ConfigParam_GetData(&confCameraMode[i].ArtificialColorExposure);
		ConfigParam_GetData(&confCameraMode[i].WhiteBalance);
	}

	// confMonitorMode
	for (i = 0; i < CONFIG_MONITOR_MODE_AMT; i++) {
		ConfigParam_GetData(&confMonitorMode[i].ColorGainR);
		ConfigParam_GetData(&confMonitorMode[i].ColorGainG);
		ConfigParam_GetData(&confMonitorMode[i].ColorGainB);
		ConfigParam_GetData(&confMonitorMode[i].Contrast);
		ConfigParam_GetData(&confMonitorMode[i].Brightness);
		ConfigParam_GetData(&confMonitorMode[i].Saturation);
	}

	// confVideo
	for (i = 0; i < CONFIG_VIDEO_AMT; i++) {
		ConfigParam_GetData(&confVideo[i].PaletteSelection);
		ConfigParam_GetData(&confVideo[i].Type);
		ConfigParam_GetData(&confVideo[i].LinesPerFrame);
		ConfigParam_GetData(&confVideo[i].LinesActiveInCompletePicture);
		ConfigParam_GetData(&confVideo[i].FirstActiveLine);
		ConfigParam_GetData(&confVideo[i].HorizontalTotalLength);
		ConfigParam_GetData(&confVideo[i].HorizontalSyncLength);
		ConfigParam_GetData(&confVideo[i].HorizontalActiveVideoStart);
		ConfigParam_GetData(&confVideo[i].HorizontalActiveVideoLength);
		ConfigParam_GetData(&confVideo[i].VerticalSyncLength);
		ConfigParam_GetData(&confVideo[i].VoltageDac);
		ConfigParam_GetData(&confVideo[i].SyncRgb);
		ConfigParam_GetData(&confVideo[i].AspectRatio);
	}

	// confGraphics
	ConfigParam_GetData(&confGraphics.ReferenceLineWidth);
	ConfigParam_GetData(&confGraphics.ReferenceLineLowSpeed);
	ConfigParam_GetData(&confGraphics.ReferenceLineHighSpeed);
	ConfigParam_GetData(&confGraphics.ReferenceLineHighSpeedDelay);
	ConfigParam_GetData(&confGraphics.ReferenceLineSwitchDelay);
	ConfigParam_GetData(&confGraphics.PosNegSpeed);

	// confPTZ
	ConfigParam_GetData(&confPTZ.LimitUp);
	ConfigParam_GetData(&confPTZ.LimitDown);
	ConfigParam_GetData(&confPTZ.LimitLeft);
	ConfigParam_GetData(&confPTZ.LimitRight);

	// confBattery
	ConfigParam_GetData(&confBattery.Type);
	ConfigParam_GetData(&confBattery.NominalCapacity);
	ConfigParam_GetData(&confBattery.TempCoefficientCapacity);
	ConfigParam_GetData(&confBattery.AgeCoefficientCapacity);
	ConfigParam_GetData(&confBattery.LowCapacityLimit);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(ConfigParam_ParseEEPROM);
#endif

#ifdef __KERNEL__
// Kernel space stubs for print functions
void ConfigParam_PrintParam(ConfigParam *param)
{
	const void *data;
	const int name_width = 36; // Consistent with user-space for padding
	uint32_t i; // Loop variable for arrays
	const uint8_t *array_data;

	if (!param) {
		LVI_PRINT("ConfigParam_PrintParam: Parameter is NULL\n");
		return;
	}
	// It's good practice to also check param->name, though ConfigParam_InitAll should set it.
	if (!param->name) {
		LVI_PRINT(
			"ConfigParam_PrintParam: Parameter name is NULL (Addr: 0x%04X)\n",
			param->address);
		// Still try to print what we can if data is available
	}

	data = ConfigParam_GetData(param); // Reads from EEPROM
	if (!data) {
		LVI_PRINT(
			"ConfigParam_PrintParam: No data for %s (Addr: 0x%04X)\n",
			param->name ? param->name : "UNKNOWN", param->address);
		return;
	}

	switch (param->type) {
	case TYPE_8:
		LVI_PRINT(
			"\tParameter: %-*s Addr: 0x%04X  Size: %2u  Type: TYPE_8         Value: 0x%02X\n",
			name_width, param->name ? param->name : "N/A",
			param->address, param->size, *(const uint8_t *)data);
		break;
	case TYPE_16:
		LVI_PRINT(
			"\tParameter: %-*s Addr: 0x%04X  Size: %2u  Type: TYPE_16        Value: 0x%04X\n",
			name_width, param->name ? param->name : "N/A",
			param->address, param->size, *(const uint16_t *)data);
		break;
	case TYPE_32:
		LVI_PRINT(
			"\tParameter: %-*s Addr: 0x%04X  Size: %2u  Type: TYPE_32        Value: 0x%08X\n",
			name_width, param->name ? param->name : "N/A",
			param->address, param->size, *(const uint32_t *)data);
		break;
	case TYPE_STRING:
		LVI_PRINT(
			"\tParameter: %-*s Addr: 0x%04X  Size: %2u  Type: TYPE_STRING    Value: \"%s\"\n",
			name_width, param->name ? param->name : "N/A",
			param->address, param->size, (const char *)data);
		break;
	case TYPE_ARRAY:
		// Start the line using LVI_PRINT (which might add KERN_INFO etc.)
		LVI_PRINT(
			"\tParameter: %-*s Addr: 0x%04X  Size: %2u  Type: TYPE_ARRAY     Value: [",
			name_width, param->name ? param->name : "N/A",
			param->address, param->size); // No newline here

		array_data = (const uint8_t *)data;
		for (i = 0; i < param->size; ++i) {
			// Continue the line with KERN_CONT
			// Limit printed elements to avoid excessive log spam for very large arrays
			if (i >= 32 &&
			    param->size >
				    36) { // Print up to ~32 elements, then "..."
				printk(KERN_CONT "...");
				break;
			}
			printk(KERN_CONT "0x%02X", array_data[i]);
			if (i < param->size - 1) {
				printk(KERN_CONT ", ");
			}
		}
		printk(KERN_CONT "]\n"); // End the line
		break;
	default:
		LVI_PRINT(
			"\tParameter: %-*s Addr: 0x%04X  Size: %2u  Type: UNKNOWN TYPE\n",
			name_width, param->name ? param->name : "N/A",
			param->address, param->size);
		break;
	}
}

void ConfigParam_PrintAll(void)
{
	int i;

	LVI_PRINT("[%s] --- All Configuration Parameters (Kernel) ---\n",
		  __func__);

	// confProductId
	LVI_PRINT("[%s] --- ProductID (%s) ---\n", __func__,
		  confProductId.name ? confProductId.name : "N/A");
	ConfigParam_PrintParam(&confProductId);

	// confFactoryDefaultVersion
	LVI_PRINT("[%s] --- FactoryDefaultVersion (%s) ---\n", __func__,
		  confFactoryDefaultVersion.name ?
			  confFactoryDefaultVersion.name :
			  "N/A");
	ConfigParam_PrintParam(&confFactoryDefaultVersion);

	// confLicenseKey
	LVI_PRINT("[%s] --- LicenseKey ---\n", __func__);
	for (i = 0; i < CONFIG_LICENSE_KEY_AMT; i++) {
		// Assuming confLicenseKey[i].name is set during init, e.g. "LicenseKey[0]"
		// ConfigParam_PrintParam will print its specific name.
		ConfigParam_PrintParam(&confLicenseKey[i]);
	}

	// confFunctions
	LVI_PRINT("[%s] --- Functions (%s) ---\n", __func__,
		  confFunctions.name ? confFunctions.name : "N/A");
	ConfigParam_PrintParam(&confFunctions.Seesaw);
	ConfigParam_PrintParam(&confFunctions.ReferenceLine);
	ConfigParam_PrintParam(&confFunctions.Curtain);

	// confColor
	LVI_PRINT("[%s] --- Color Profiles (confColor) ---\n", __func__);
	for (i = 0; i < CONFIG_COLOR_AMT; i++) {
		LVI_PRINT("[%s]   -- Color Profile [%d] (%s) --\n", __func__, i,
			  confColor[i].name ? confColor[i].name : "N/A");
		ConfigParam_PrintParam(&confColor[i].ArtificialColorPoint1);
		ConfigParam_PrintParam(&confColor[i].ArtificialColorPoint2);
		ConfigParam_PrintParam(&confColor[i].ArtificialColorPoint3);
		ConfigParam_PrintParam(&confColor[i].ArtificialColorPoint4);
		ConfigParam_PrintParam(&confColor[i].CameraBackground);
		ConfigParam_PrintParam(&confColor[i].ReferenceLine);
		ConfigParam_PrintParam(&confColor[i].Restricted);
	}

	// confCamera
	LVI_PRINT("[%s] --- Camera (%s) ---\n", __func__,
		  confCamera.name ? confCamera.name : "N/A");
	ConfigParam_PrintParam(&confCamera.Type);
	ConfigParam_PrintParam(&confCamera.RGain);
	ConfigParam_PrintParam(&confCamera.BGain);
	ConfigParam_PrintParam(&confCamera.FirstLine);
	ConfigParam_PrintParam(&confCamera.UsableLines);
	ConfigParam_PrintParam(&confCamera.FirstPixel);
	ConfigParam_PrintParam(&confCamera.UsablePixels);
	ConfigParam_PrintParam(&confCamera.UsableRatio);
	ConfigParam_PrintParam(&confCamera.Interface);
	ConfigParam_PrintParam(&confCamera.ArtificialColorShutterTime);
	ConfigParam_PrintParam(&confCamera.ArtificialColorBrightness);
	ConfigParam_PrintParam(&confCamera.ArtificialColorIris);
	ConfigParam_PrintParam(&confCamera.ArtificialColorExposureCompensation);
	ConfigParam_PrintParam(&confCamera.NaturalColorShutterTime);
	ConfigParam_PrintParam(&confCamera.NaturalColorBrightness);
	ConfigParam_PrintParam(&confCamera.NaturalColorIris);
	ConfigParam_PrintParam(&confCamera.NaturalColorExposureCompensation);
	ConfigParam_PrintParam(&confCamera.NaturalColorGainPeak);
	ConfigParam_PrintParam(&confCamera.ArtificialColorGainPeak);

	// confCameraMode
	LVI_PRINT("[%s] --- Camera Modes (confCameraMode) ---\n", __func__);
	for (i = 0; i < CONFIG_CAMERA_MODE_AMT; i++) {
		LVI_PRINT("[%s]   -- Camera Mode [%d] (%s) --\n", __func__, i,
			  confCameraMode[i].name ? confCameraMode[i].name :
						   "N/A");
		ConfigParam_PrintParam(&confCameraMode[i].ZoomMin);
		ConfigParam_PrintParam(&confCameraMode[i].ZoomMax);
		ConfigParam_PrintParam(&confCameraMode[i].ZoomSpeed);
		ConfigParam_PrintParam(&confCameraMode[i].Focus);
		ConfigParam_PrintParam(&confCameraMode[i].FocusRangeMin);
		ConfigParam_PrintParam(&confCameraMode[i].FocusRangeMax);
		ConfigParam_PrintParam(&confCameraMode[i].FocusSpeed);
		ConfigParam_PrintParam(&confCameraMode[i].NaturalColorExposure);
		ConfigParam_PrintParam(
			&confCameraMode[i].ArtificialColorExposure);
		ConfigParam_PrintParam(&confCameraMode[i].WhiteBalance);
	}

	// confMonitorMode
	LVI_PRINT("[%s] --- Monitor Modes (confMonitorMode) ---\n", __func__);
	for (i = 0; i < CONFIG_MONITOR_MODE_AMT; i++) {
		LVI_PRINT("[%s]   -- Monitor Mode [%d] (%s) --\n", __func__, i,
			  confMonitorMode[i].name ? confMonitorMode[i].name :
						    "N/A");
		ConfigParam_PrintParam(&confMonitorMode[i].ColorGainR);
		ConfigParam_PrintParam(&confMonitorMode[i].ColorGainG);
		ConfigParam_PrintParam(&confMonitorMode[i].ColorGainB);
		ConfigParam_PrintParam(&confMonitorMode[i].Contrast);
		ConfigParam_PrintParam(&confMonitorMode[i].Brightness);
		ConfigParam_PrintParam(&confMonitorMode[i].Saturation);
	}

	// confVideo
	LVI_PRINT("[%s] --- Video Settings (confVideo) ---\n", __func__);
	for (i = 0; i < CONFIG_VIDEO_AMT; i++) {
		LVI_PRINT("[%s]   -- Video Setting [%d] (%s) --\n", __func__, i,
			  confVideo[i].name ? confVideo[i].name : "N/A");
		ConfigParam_PrintParam(&confVideo[i].PaletteSelection);
		ConfigParam_PrintParam(&confVideo[i].Type);
		ConfigParam_PrintParam(&confVideo[i].LinesPerFrame);
		ConfigParam_PrintParam(
			&confVideo[i].LinesActiveInCompletePicture);
		ConfigParam_PrintParam(&confVideo[i].FirstActiveLine);
		ConfigParam_PrintParam(&confVideo[i].HorizontalTotalLength);
		ConfigParam_PrintParam(&confVideo[i].HorizontalSyncLength);
		ConfigParam_PrintParam(
			&confVideo[i].HorizontalActiveVideoStart);
		ConfigParam_PrintParam(
			&confVideo[i].HorizontalActiveVideoLength);
		ConfigParam_PrintParam(&confVideo[i].VerticalSyncLength);
		ConfigParam_PrintParam(&confVideo[i].VoltageDac);
		ConfigParam_PrintParam(&confVideo[i].SyncRgb);
		ConfigParam_PrintParam(&confVideo[i].AspectRatio);
	}

	// confGraphics
	LVI_PRINT("[%s] --- Graphics (%s) ---\n", __func__,
		  confGraphics.name ? confGraphics.name : "N/A");
	ConfigParam_PrintParam(&confGraphics.ReferenceLineWidth);
	ConfigParam_PrintParam(&confGraphics.ReferenceLineLowSpeed);
	ConfigParam_PrintParam(&confGraphics.ReferenceLineHighSpeed);
	ConfigParam_PrintParam(&confGraphics.ReferenceLineHighSpeedDelay);
	ConfigParam_PrintParam(&confGraphics.ReferenceLineSwitchDelay);
	ConfigParam_PrintParam(&confGraphics.PosNegSpeed);

	// confPTZ
	LVI_PRINT("[%s] --- PTZ (%s) ---\n", __func__,
		  confPTZ.name ? confPTZ.name : "N/A");
	ConfigParam_PrintParam(&confPTZ.LimitUp);
	ConfigParam_PrintParam(&confPTZ.LimitDown);
	ConfigParam_PrintParam(&confPTZ.LimitLeft);
	ConfigParam_PrintParam(&confPTZ.LimitRight);

	// confBattery
	LVI_PRINT("[%s] --- Battery (%s) ---\n", __func__,
		  confBattery.name ? confBattery.name : "N/A");
	ConfigParam_PrintParam(&confBattery.Type);
	ConfigParam_PrintParam(&confBattery.NominalCapacity);
	ConfigParam_PrintParam(&confBattery.TempCoefficientCapacity);
	ConfigParam_PrintParam(&confBattery.AgeCoefficientCapacity);
	ConfigParam_PrintParam(&confBattery.LowCapacityLimit);

	LVI_PRINT("[%s] --- End of Configuration Parameters (Kernel) ---\n",
		  __func__);
}

#else
// User-space functions
void ConfigParam_PrintParam(ConfigParam *param)
{
	const void *data;
	const int name_width = 40;
	uint32_t i;
	const uint8_t *array;

	if (!param) {
		printf("\nParameter not found\n");
		return;
	}

	data = ConfigParam_GetData(param);
	if (!data) {
		printf("No data available in %s\n", param->name);
		return;
	}

	printf("\tParameter: %-*s", name_width, param->name);
	printf("Addr: 0x%04X\tSize: %u\tType: ", param->address, param->size);

	switch (param->type) {
	case TYPE_8:
		printf("TYPE_8\t\tValue: 0x%02X\n", *(const uint8_t *)data);
		break;
	case TYPE_16:
		printf("TYPE_16\t\tValue: 0x%04X\n", *(const uint16_t *)data);
		break;
	case TYPE_32:
		printf("TYPE_32\t\tValue: 0x%08X\n", *(const uint32_t *)data);
		break;
	case TYPE_STRING:
		printf("TYPE_STRING\tValue: \"%s\"\n", (const char *)data);
		break;
	case TYPE_ARRAY:
		printf("TYPE_ARRAY\tValue: [");
		array = (const uint8_t *)data;
		for (i = 0; i < param->size; ++i) {
			printf("0x%02X", array[i]);
			if (i < param->size - 1)
				printf(", ");
		}
		printf("]\n");
		break;
	default:
		printf("UNKNOWN\n");
		break;
	}
}

void ConfigParam_PrintAll(void)
{
	uint8_t i;

	printf("\nProductID Configuration: %s\n", confProductId.name);
	ConfigParam_PrintParam(&confProductId);

	printf("\nFactory Default Version Configuration: %s\n",
	       confFactoryDefaultVersion.name);
	ConfigParam_PrintParam(&confFactoryDefaultVersion);

	printf("\nLicense Key Configurations:\n");
	for (i = 0; i < CONFIG_LICENSE_KEY_AMT; i++) {
		ConfigParam_PrintParam(&confLicenseKey[i]);
	}

	printf("\nFunction Configurations: %s\n", confFunctions.name);
	ConfigParam_PrintParam(&confFunctions.Seesaw);
	ConfigParam_PrintParam(&confFunctions.ReferenceLine);
	ConfigParam_PrintParam(&confFunctions.Curtain);

	/* Add other sections as needed */
}

#endif

/* * Parses a configuration file from a buffer.
 * The buffer should contain lines of key-value pairs, with optional comments.
 * Lines starting with '#' are treated as comments and ignored.
 */
static void parse_config_from_buffer(const char *buffer, size_t buffer_len)
{
	char line_buffer[CONFIG_LINE_MAX_LENGTH];
	const char *current_pos = buffer;
	const char *buffer_end = buffer + buffer_len;
	const char *line_end;
	size_t line_len;
	char *newline_char;
	char *key_part;
	char *value_part;
	char *line_ptr;
	ConfigParam *param;
	int is_string_value;
	char *actual_value_str;
	size_t val_len;
	uint32_t parsed_value;
	char *key_end; // Moved declaration up
	char *value_end; // Moved declaration up

	while (current_pos < buffer_end) {
		line_end = LVI_STRNCHR(current_pos, buffer_end - current_pos,
				       '\n');

		if (line_end) {
			line_len = line_end - current_pos;
		} else {
			line_len = buffer_end - current_pos;
		}

		if (line_len >= CONFIG_LINE_MAX_LENGTH) {
			line_len = CONFIG_LINE_MAX_LENGTH - 1;
		}

		memcpy(line_buffer, current_pos, line_len);
		line_buffer[line_len] = '\0';

		current_pos += line_len + (line_end ? 1 : 0);

		if (line_buffer[0] == '#' || line_buffer[0] == '\0' ||
		    line_buffer[0] == '\r')
			continue;

		newline_char = strpbrk(line_buffer, "\r\n");
		if (newline_char)
			*newline_char = '\0';

		line_ptr = line_buffer;

#ifdef __KERNEL__
		key_part = strsep(&line_ptr,
				  " \t="); // Key is first word, also handle '='
		if (line_ptr) { // line_ptr is now the rest of the string
			while (*line_ptr == ' ' || *line_ptr == '\t' ||
			       *line_ptr == '=') // Skip spaces, tabs, and '='
			{
				if (*line_ptr == '\0')
					break;
				line_ptr++; // Ensure pointer advances
			}
		}
		value_part = line_ptr; // Value is the trimmed rest
#else
		key_part = strtok(line_ptr,
				  " \t="); // Key is first word, also handle '='
		value_part =
			strtok(NULL, ""); // Value is the rest of the string
		if (value_part) {
			char *temp_val = value_part;
			while (*temp_val == ' ' || *temp_val == '\t' ||
			       *temp_val == '=') // Skip spaces, tabs, and '='
			{
				if (*temp_val == '\0')
					break;
				temp_val++; // Ensure pointer advances
			}
			value_part = temp_val;
		}
#endif

		if (!key_part || !value_part || *value_part == '\0')
			continue;

		// Trim trailing whitespace from key_part
		key_end = key_part + strlen(key_part) - 1;
		while (key_end > key_part &&
		       (*key_end == ' ' || *key_end == '\t')) {
			*key_end-- = '\0';
		}

		// Trim trailing whitespace from value_part
		value_end = value_part + strlen(value_part) - 1;
		while (value_end > value_part &&
		       (*value_end == ' ' || *value_end == '\t')) {
			*value_end-- = '\0';
		}

		param = ConfigParam_FindByName(key_part);
		if (!param) {
			LVI_ERR_PRINT("Unknown configuration parameter: %s\n",
				      key_part);
			continue;
		}

		is_string_value = (value_part[0] == '"');
		actual_value_str = value_part;

		if (is_string_value) {
			actual_value_str++;
			val_len = strlen(actual_value_str);
			if (val_len > 0 &&
			    actual_value_str[val_len - 1] == '"') {
				actual_value_str[val_len - 1] = '\0';
			}
		}

		if (param->type == TYPE_STRING) {
			ConfigParam_SetData(param, actual_value_str);
		} else {
#ifdef __KERNEL__
			if (kstrtouint(actual_value_str, 0, &parsed_value) !=
			    0) {
				LVI_ERR_PRINT(
					"Invalid numeric value: %s for key %s\n",
					actual_value_str, key_part);
				continue;
			}
#else
			if (sscanf(actual_value_str, "0x%x", &parsed_value) !=
				    1 &&
			    sscanf(actual_value_str, "%u", &parsed_value) !=
				    1) {
				LVI_ERR_PRINT(
					"Invalid numeric value: %s for key %s\n",
					actual_value_str, key_part);
				continue;
			}
#endif
			switch (param->type) {
			case TYPE_8: {
				uint8_t val8 = (uint8_t)parsed_value;
				ConfigParam_SetData(param, &val8);
				break;
			}
			case TYPE_16: {
				uint16_t val16 = (uint16_t)parsed_value;
				ConfigParam_SetData(param, &val16);
				break;
			}
			case TYPE_32: {
				ConfigParam_SetData(param, &parsed_value);
				break;
			}
			case TYPE_ARRAY:
				LVI_ERR_PRINT(
					"TYPE_ARRAY parsing from simple value not fully supported: %s\n",
					key_part);
				break;
			default:
				LVI_ERR_PRINT(
					"Unsupported data type for direct value parsing: %s\n",
					key_part);
				break;
			}
		}
	}
}

void ConfigParam_ParseBuffer(const char *buffer, size_t buffer_len)
{
	if (!buffer || buffer_len == 0) {
		LVI_ERR_PRINT("Buffer is null or empty for parsing.\n");
		return;
	}
	parse_config_from_buffer(buffer, buffer_len);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(ConfigParam_ParseBuffer);
#endif

/* * Parses a single configuration parameter from a buffer.
 * Returns 1 on success, 0 on failure.
 * The target_key must match the name of a known ConfigParam.
 *
 * This function is kernel and user-space compatible.
 */
uint8_t ConfigParam_ParseSingleBuffer(const char *buffer, size_t buffer_len,
				      const char *target_key)
{
	char line_buffer[CONFIG_LINE_MAX_LENGTH];
	const char *current_pos = buffer;
	const char *buffer_end = buffer + buffer_len;
	const char *line_end;
	size_t line_len;
	char *newline_char;
	char *key_part;
	char *value_part;
	char *line_ptr;
	ConfigParam *param;
	int is_string_value;
	char *actual_value_str;
	size_t val_len;
	uint32_t parsed_value;
	char *key_end;
	char *value_end;

	if (!buffer || buffer_len == 0 || !target_key) {
		LVI_ERR_PRINT("Invalid arguments for ParseSingleBuffer.\n");
		return 0;
	}

	while (current_pos < buffer_end) {
		line_end = LVI_STRNCHR(current_pos, buffer_end - current_pos,
				       '\n');

		if (line_end) {
			line_len = line_end - current_pos;
		} else {
			line_len = buffer_end - current_pos;
		}

		if (line_len >= CONFIG_LINE_MAX_LENGTH) {
			line_len = CONFIG_LINE_MAX_LENGTH - 1;
		}

		memcpy(line_buffer, current_pos, line_len);
		line_buffer[line_len] = '\0';

		current_pos += line_len + (line_end ? 1 : 0);

		if (line_buffer[0] == '#' || line_buffer[0] == '\0' ||
		    line_buffer[0] == '\r')
			continue;

		newline_char = strpbrk(line_buffer, "\r\n");
		if (newline_char)
			*newline_char = '\0';

		line_ptr = line_buffer;

#ifdef __KERNEL__
		key_part = strsep(&line_ptr, " \t=");
		if (line_ptr) {
			while (*line_ptr == ' ' || *line_ptr == '\t' ||
			       *line_ptr == '=') {
				if (*line_ptr == '\0')
					break;
				line_ptr++;
			}
		}
		value_part = line_ptr;
#else
		key_part = strtok(line_ptr, " \t=");
		value_part = strtok(NULL, "");
		if (value_part) {
			char *temp_val = value_part;
			while (*temp_val == ' ' || *temp_val == '\t' ||
			       *temp_val == '=') {
				if (*temp_val == '\0')
					break;
				temp_val++;
			}
			value_part = temp_val;
		}
#endif

		if (!key_part || !value_part || *value_part == '\0')
			continue;

		key_end = key_part + strlen(key_part) - 1;
		while (key_end > key_part &&
		       (*key_end == ' ' || *key_end == '\t')) {
			*key_end-- = '\0';
		}

		value_end = value_part + strlen(value_part) - 1;
		while (value_end > value_part &&
		       (*value_end == ' ' || *value_end == '\t')) {
			*value_end-- = '\0';
		}

		if (strcmp(key_part, target_key) == 0) {
			param = ConfigParam_FindByName(key_part);
			if (!param) {
				LVI_ERR_PRINT(
					"Unknown configuration parameter: %s\n",
					key_part);
				return 0; // Key matches target but not found in known params
			}

			is_string_value = (value_part[0] == '"');
			actual_value_str = value_part;

			if (is_string_value) {
				actual_value_str++;
				val_len = strlen(actual_value_str);
				if (val_len > 0 &&
				    actual_value_str[val_len - 1] == '"') {
					actual_value_str[val_len - 1] = '\0';
				}
			}

			if (param->type == TYPE_STRING) {
				ConfigParam_SetData(param, actual_value_str);
			} else {
#ifdef __KERNEL__
				if (kstrtouint(actual_value_str, 0,
					       &parsed_value) != 0) {
					LVI_ERR_PRINT(
						"Invalid numeric value: %s for key %s\n",
						actual_value_str, key_part);
					return 0;
				}
#else
				if (sscanf(actual_value_str, "0x%x",
					   &parsed_value) != 1 &&
				    sscanf(actual_value_str, "%u",
					   &parsed_value) != 1) {
					LVI_ERR_PRINT(
						"Invalid numeric value: %s for key %s\n",
						actual_value_str, key_part);
					return 0;
				}
#endif
				switch (param->type) {
				case TYPE_8: {
					uint8_t val8 = (uint8_t)parsed_value;
					ConfigParam_SetData(param, &val8);
					break;
				}
				case TYPE_16: {
					uint16_t val16 = (uint16_t)parsed_value;
					ConfigParam_SetData(param, &val16);
					break;
				}
				case TYPE_32: {
					ConfigParam_SetData(param,
							    &parsed_value);
					break;
				}
				case TYPE_ARRAY:
					LVI_ERR_PRINT(
						"TYPE_ARRAY parsing from simple value not fully supported: %s\n",
						key_part);
					return 0;
				default:
					LVI_ERR_PRINT(
						"Unsupported data type for direct value parsing: %s\n",
						key_part);
					return 0;
				}
			}
			return 1; // Key found and parsed
		}
	}
	return 0; // Target key not found in buffer
}

#ifndef __KERNEL__
/* * Parses a configuration file and updates the parameters.
 * The file should contain lines in the format: key = value
 * Supports both numeric and string values.
 * If a line starts with '#', it is treated as a comment.
 *
 * This function is user-space only.
 */
void ConfigParam_ParseFile(const char *filename)
{
	FILE *file;
	long file_size;
	char *buffer;
	size_t read_size;

	file = fopen(filename, "r");
	if (!file) {
		perror("Error opening configuration file");
		return;
	}

	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (file_size <= 0) {
		LVI_ERR_PRINT("File is empty or invalid size: %s\n", filename);
		fclose(file);
		return;
	}

	buffer = (char *)LVI_MALLOC(file_size + 1);
	if (!buffer) {
		perror("Failed to allocate buffer for file content");
		fclose(file);
		return;
	}

	read_size = fread(buffer, 1, file_size, file);
	buffer[read_size] = '\0';
	fclose(file);

	if (read_size != file_size) {
		LVI_ERR_PRINT("Failed to read entire file: %s\n", filename);
		LVI_FREE(buffer);
		return;
	}

	parse_config_from_buffer(buffer, read_size);
	LVI_FREE(buffer);
}

/* * Parses a single configuration parameter from a file.
 * Returns 1 on success, 0 on failure.
 * The target_key must match the name of a known ConfigParam.
 *
 * This function is user-space only.
 */
uint8_t ConfigParam_ParseFileSingle(const char *filename,
				    const char *target_key)
{
	FILE *file;
	long file_size;
	char *buffer;
	size_t read_size;
	uint8_t result;

	file = fopen(filename, "r");
	if (!file) {
		perror("Error opening configuration file for single parse");
		return 0;
	}

	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (file_size <= 0) {
		fclose(file);
		return 0;
	}

	buffer = (char *)LVI_MALLOC(file_size + 1);
	if (!buffer) {
		fclose(file);
		return 0;
	}

	read_size = fread(buffer, 1, file_size, file);
	buffer[read_size] = '\0';
	fclose(file);

	if (read_size != file_size) {
		LVI_FREE(buffer);
		return 0;
	}

	result = ConfigParam_ParseSingleBuffer(buffer, read_size, target_key);
	LVI_FREE(buffer);
	return result;
}
#endif // !__KERNEL__