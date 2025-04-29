#ifndef _IMX_LCDIFV3_USER_H
#define _IMX_LCDIFV3_USER_H

#include <linux/types.h>

/* Magic number, choose any unused byte */
#define IMX_LCDIFV3_IOC_MAGIC  0xF3

/* struct to carry 6 parameters: brightness, contrast, saturation, r/g/b gains */
struct lcdifv3_color_params {
    __u8 brightness;  /* 0–255, 128 = neutral */
    __u8 contrast;    /* 0–255, 128 = 1× */
    __u8 saturation;  /* 0–255, 128 = 1× */
    __u8 r_gain;      /* 0–255, 128 = 1× */
    __u8 g_gain;      /* 0–255, 128 = 1× */
    __u8 b_gain;      /* 0–255, 128 = 1× */
};

/* User → Kernel: set all 6 params at once */
#define IMX_LCDIFV3_IOC_SET_COLOR    _IOW(IMX_LCDIFV3_IOC_MAGIC, 1, struct lcdifv3_color_params)

/* (Optional) Kernel → User: get current params */
#define IMX_LCDIFV3_IOC_GET_COLOR    _IOR(IMX_LCDIFV3_IOC_MAGIC, 2, struct lcdifv3_color_params)

#endif /* _IMX_LCDIFV3_USER_H */
