
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/firmware.h> // Required for firmware loading

int platform_eeprom_read(uint32_t reg, uint8_t *data_val);
int platform_eeprom_write(uint32_t reg, uint8_t data_val);