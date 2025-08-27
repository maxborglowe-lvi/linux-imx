/**
 * @file lviconfig.c
 * @brief EEPROM I2C driver kernel module
 *
 * This kernel module provides an interface to interact with an EEPROM device
 * over the I2C bus. The module supports reading and writing data using
 * IOCTL commands.
 *
 * The driver handles different EEPROM address sizes (8-bit, 16-bit, 32-bit)
 * and ensures proper access to the EEPROM memory within the defined size.
 *
 * @author Max Borglowe
 * @date 2024-08-20
 * @version 1.0
 */

#include "lviconfig_ctrl.h"
#include <linux/gpio/consumer.h>

#define DEVICE_NAME "lviconfig_ctrl"
#define EEPROM_CELLS 4 // amount of addressable eeprom cells
#define EEPROM_BIT_DEPTH 8 // bits per eeprom subaddress
#define EEPROM_CELL_SIZE 256 // amount of subaddresses per eeprom cell
#define EEPROM_SUBADDRESSES EEPROM_CELLS *EEPROM_CELL_SIZE // size of each subaddress in bytes
#define EEPROM_SIZE (EEPROM_CELLS * EEPROM_BIT_DEPTH * EEPROM_CELL_SIZE) // total eeprom size

#define IOCTL_WRITE_DATA _IOW('e', 1, struct eeprom_data)
#define IOCTL_READ_DATA _IOR('e', 2, struct eeprom_data)

struct lviconfig_mediator {
	char *param_string;
	void *data;
};

#define IOCTL_SET_CONFIG_PARAM _IOW('C', 3, struct lviconfig_mediator)
#define IOCTL_GET_CONFIG_PARAM _IOR('C', 4, struct lviconfig_mediator)
#define IOCTL_SET_CONFIG_PARAM_EEPROM _IOW('C', 5, struct lviconfig_mediator)

static int major;
static struct i2c_client *i2c_client_g;
static struct class *lviconfig_class;

static struct gpio_desc *write_control_gpio;

#define REG_SIZE_8 2
#define REG_SIZE_16 3
#define REG_SIZE_32 5

static uint8_t reg_size = REG_SIZE_8;

struct eeprom_data {
	uint32_t reg;
	uint8_t data;
};

// Kernel-space implementations of platform_eeprom functions
#ifdef __KERNEL__
int lviconfig_platform_eeprom_write(uint32_t reg, uint8_t data_val)
{
	struct i2c_msg msgs[1];
	uint8_t buf[5]; // Max: 4 address bytes + 1 data byte (for some EEPROM types, though we send 1 byte addr + 1 data for M24C like)
	// The original driver's REG_SIZE_XX implies how many address bytes.
	// M24C series often use 1 or 2 address bytes.
	// The addr_cell logic implies M24C type EEPROMs.
	int ret;
	uint8_t i2c_dev_addr;
	uint8_t internal_addr;

	if (!i2c_client_g) {
		pr_err("[%s]:  i2c_client not initialized\n", __func__);
		return -ENODEV;
	}

	if (reg >= EEPROM_SUBADDRESSES) {
		pr_err("[%s]: EEPROM Address 0x%08X is out of range for platform_write\n", __func__, reg);
		return -EINVAL;
	}

	// Determine I2C device address and internal register based on M24C-like scheme
	// This assumes EEPROM_CELL_SIZE is 256 (for 8-bit internal address)
	// and EEPROM_CELLS determines how many such devices are multiplexed on i2c_client_g->addr
	i2c_dev_addr = i2c_client_g->addr + (reg / EEPROM_CELL_SIZE);
	internal_addr = reg % EEPROM_CELL_SIZE;

	// For M24C01/M24C02 (1k/2k bit), it's 1 byte address.
	// For M24C04/08/16 (4k/8k/16k bit), it's 1 byte address, but A0-A2 on device select page.
	// For M24C32/64 (32k/64k bit), it's 2 byte address.
	// The driver's reg_size_g seems to try to handle this, but addr_cell/addr_sub is more M24C like.
	// Let's stick to the addr_cell/addr_sub logic for now, implying 1 byte internal address.

	buf[0] = internal_addr; // Internal EEPROM address
	buf[1] = data_val; // Data to write

	msgs[0].addr = i2c_dev_addr;
	msgs[0].flags = 0; // Write
	msgs[0].len = 2; // 1 byte address + 1 byte data
	msgs[0].buf = buf;

	ret = i2c_transfer(i2c_client_g->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("[%s] i2c_transfer failed (ret %d) for addr 0x%x reg 0x%x\n", __func__, ret, i2c_dev_addr, internal_addr);
		return -EIO;
	}
	msleep(5); // Delay for EEPROM write cycle (adjust as per datasheet, 5ms is common)
	return 0;
}

int lviconfig_platform_eeprom_read(uint32_t reg, uint8_t *data_val)
{
	struct i2c_msg msgs[2];
	uint8_t internal_addr_buf[1];
	int ret;
	uint8_t i2c_dev_addr;

	if (!i2c_client_g) {
		pr_err("[%s]  i2c_client not initialized\n", __func__);
		return -ENODEV;
	}
	if (!data_val) {
		pr_err("[%s]: data_val pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (reg >= EEPROM_SUBADDRESSES) {
		pr_err("[%s]: EEPROM Address 0x%08X is out of range for platform_read\n", __func__, reg);
		return -EINVAL;
	}

	i2c_dev_addr = i2c_client_g->addr + (reg / EEPROM_CELL_SIZE);
	internal_addr_buf[0] = reg % EEPROM_CELL_SIZE;

	// Write internal address
	msgs[0].addr = i2c_dev_addr;
	msgs[0].flags = 0; // Write
	msgs[0].len = 1;
	msgs[0].buf = internal_addr_buf;

	// Read data
	msgs[1].addr = i2c_dev_addr;
	msgs[1].flags = I2C_M_RD; // Read
	msgs[1].len = 1;
	msgs[1].buf = data_val;

	ret = i2c_transfer(i2c_client_g->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("[%s]: i2c_transfer failed (ret %d) for addr 0x%x reg 0x%x\n", __func__, ret, i2c_dev_addr, internal_addr_buf[0]);
		return -EIO;
	}
	return 0;
}
#endif // __KERNEL__

// Device tree match table
static const struct of_device_id lviconfig_of_match[] = {
	{
		.compatible = "lvi,lviconfig",
	},
	{},
};
MODULE_DEVICE_TABLE(of, lviconfig_of_match);

/**
 * @brief IOCTL handler for the EEPROM device.
 *
 * This function handles IOCTL commands for reading and writing data
 * to and from the EEPROM.
 *
 * @param file Pointer to the file structure.
 * @param cmd IOCTL command number.
 * @param arg Pointer to the user-space data structure.
 * @return 0 on success, negative error code on failure.
 */
static long lviconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	ConfigParam *param;

	struct lviconfig_mediator mediator; /**< Mediator structure for config parameters */

	if (copy_from_user(&mediator, (struct lviconfig_mediator *)arg, sizeof(struct lviconfig_mediator))) {
		return -EFAULT;
	}

	switch (cmd) {
	case IOCTL_SET_CONFIG_PARAM:
		if (mediator.param_string == NULL) {
			return -EINVAL; // Invalid parameter string
		}
		if (mediator.data == NULL) {
			return -EINVAL; // Invalid data pointer
		}
		pr_info("[%s]: called with param_string: %s\n", __func__, mediator.param_string);

		param = ConfigParam_FindByName(mediator.param_string);

		param->data = mediator.data; // Set the data pointer for the parameter

		if (!param) {
			pr_err("[%s]: Parameter not found: %s\n", __func__, mediator.param_string);
			return -ENOENT; // Parameter not found
		}

		break;
	case IOCTL_SET_CONFIG_PARAM_EEPROM:
		if (mediator.param_string == NULL) {
			return -EINVAL; // Invalid parameter string
		}
		if (mediator.data == NULL) {
			return -EINVAL; // Invalid data pointer
		}
		pr_info("[%s]: called with param_string: %s\n", __func__, mediator.param_string);

		param = ConfigParam_FindByName(mediator.param_string);

		if (!param) {
			pr_err("[%s]: Parameter not found: %s\n", __func__, mediator.param_string);
			return -ENOENT; // Parameter not found
		}

		ConfigParam_SetData(param, mediator.data); // Set the data for the parameter

		break;

	case IOCTL_GET_CONFIG_PARAM:

		if (mediator.param_string == NULL) {
			return -EINVAL; // Invalid parameter string
		}
		if (mediator.data == NULL) {
			return -EINVAL; // Invalid data pointer
		}
		pr_info("[%s]: called with param_string: %s\n", __func__, mediator.param_string);

		param = ConfigParam_FindByName(mediator.param_string);

		if (!param) {
			pr_err("[%s]: Parameter not found: %s\n", __func__, mediator.param_string);
			return -ENOENT; // Parameter not found
		}

		memcpy(mediator.data, param->data,
		       param->size); // Copy the data to user space

		if (copy_to_user((struct lviconfig_mediator *)arg, &mediator, sizeof(struct lviconfig_mediator))) {
			return -EFAULT; // Copy to user space failed
		}

		break;
	default:
		break;
	}

	return 0;
}

static int lviconfig_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int lviconfig_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = lviconfig_open,
	.release = lviconfig_release,
	.unlocked_ioctl = lviconfig_ioctl,
};

// lviconfig_probe: Initialize parameters here
static int lviconfig_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret; // For return values

	pr_info("[%s]: probe called for I2C client at addr 0x%x\n", __func__, client->addr);

	i2c_client_g = client; // Store the client globally for platform functions

	write_control_gpio = devm_gpiod_get_optional(&client->dev, "write-control", GPIOD_OUT_LOW);
	if (IS_ERR(write_control_gpio)) {
		ret = PTR_ERR(write_control_gpio);
		pr_err("[%s]: failed to acquire write-control gpio: %d\n", __func__, ret);
		i2c_client_g = NULL;
		return ret;
	}

	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		pr_err("[%s]: Failed to register character device\n", __func__);
		i2c_client_g = NULL;
		return major;
	}

	lviconfig_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(lviconfig_class)) {
		pr_err("[%s]: Failed to create device class\n", __func__);
		unregister_chrdev(major, DEVICE_NAME);
		i2c_client_g = NULL;
		return PTR_ERR(lviconfig_class);
	}

	if (device_create(lviconfig_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME) == NULL) {
		pr_err("[%s]: Failed to create device node\n", __func__);
		class_destroy(lviconfig_class);
		unregister_chrdev(major, DEVICE_NAME);
		i2c_client_g = NULL;
		return -ENODEV; // Use a standard error code
	}

	pr_info("[%s]: Character device created successfully.\n", __func__);

	// Initialize parameter structures and their EEPROM addresses
	ConfigParam_InitAll();
	pr_info("[%s]: ConfigParam structures initialized.\n", __func__);

	ConfigParam_ParseEEPROM(); // Read existing EEPROM content into parameters
	pr_info("[%s]: ConfigParam structures populated from EEPROM.\n", __func__);

	// ConfigParam_PrintAll();

	pr_info("[%s]: I2C client successfully initialized and configured: addr=0x%x\n", __func__, client->addr);

	return 0;
}

/**
 * @brief Remove function for I2C driver.
 */
static int lviconfig_remove(struct i2c_client *client)
{
	pr_info("[%s]: remove called for I2C client at addr 0x%x\n", __func__, client->addr);

	// Cleanup: Destroy device and class, unregister char device
	device_destroy(lviconfig_class, MKDEV(major, 0));
	class_destroy(lviconfig_class);
	unregister_chrdev(major, DEVICE_NAME);
	i2c_client_g = NULL; // Clear the global client pointer

	// Call a cleanup function in lviconfig_parameters to free allocated memory
	// This function needs to be implemented in lviconfig_parameters.c
	// Example: ConfigParam_DeInitAll();
	pr_info("[%s]: Resources released. Implement ConfigParam_DeInitAll() for full cleanup.\n", __func__);

	return 0;
}

// I2C device ID table
static const struct i2c_device_id lviconfig_id[] = {
	{ DEVICE_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, lviconfig_id);

/**
 * @brief I2C driver structure.
 */
static struct i2c_driver lviconfig_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = lviconfig_of_match,
    },
    .probe = lviconfig_probe,
    .remove = lviconfig_remove,
    .id_table = lviconfig_id,
};

module_i2c_driver(lviconfig_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION("EEPROM I2C driver kernel module with integrated config parsing");
MODULE_VERSION("1.1");