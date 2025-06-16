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

#define DEVICE_NAME "lviconfig_ctrl"
#define EEPROM_CELLS 4 // amount of addressable eeprom cells
#define EEPROM_BIT_DEPTH 8 // bits per eeprom subaddress
#define EEPROM_CELL_SIZE 256 // amount of subaddresses per eeprom cell
#define EEPROM_SUBADDRESSES                                                    \
	EEPROM_CELLS *EEPROM_CELL_SIZE // size of each subaddress in bytes
#define EEPROM_SIZE                                                            \
	(EEPROM_CELLS * EEPROM_BIT_DEPTH *                                     \
	 EEPROM_CELL_SIZE) // total eeprom size

#define IOCTL_WRITE_DATA _IOW('e', 1, struct eeprom_data)
#define IOCTL_READ_DATA _IOR('e', 2, struct eeprom_data)

static int major;
static struct i2c_client *i2c_client_g;
static struct class *lviconfig_class;

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
int platform_eeprom_write(uint32_t reg, uint8_t data_val)
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
		pr_err("[%s]: EEPROM Address 0x%08X is out of range for platform_write\n",
		       __func__, reg);
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
		pr_err("[%s] i2c_transfer failed (ret %d) for addr 0x%x reg 0x%x\n",
		       __func__, ret, i2c_dev_addr, internal_addr);
		return -EIO;
	}
	msleep(5); // Delay for EEPROM write cycle (adjust as per datasheet, 5ms is common)
	return 0;
}

int platform_eeprom_read(uint32_t reg, uint8_t *data_val)
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
		pr_err("[%s]: EEPROM Address 0x%08X is out of range for platform_read\n",
		       __func__, reg);
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
		pr_err("[%s]: i2c_transfer failed (ret %d) for addr 0x%x reg 0x%x\n",
		       __func__, ret, i2c_dev_addr, internal_addr_buf[0]);
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
static long lviconfig_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	uint8_t buf32[5]; /**< Buffer for 32-bit addressing of EEPROM (4 address bytes, 1 data byte) */
	uint8_t buf16[3]; /**< Buffer for 16-bit addressing of EEPROM (2 address bytes, 1 data byte) */
	uint8_t buf8[2]; /**< Buffer for 8-bit addressing of EEPROM (1 address byte, 1 data byte) */
	struct i2c_msg msgs[2]; /**< I2C message array for transaction */
	int ret; /**< Return value for I2C transfer */

	uint8_t addr_cell, addr_sub;

	struct eeprom_data
		data; /**< Data structure for the current IOCTL operation */

	if (copy_from_user(&data, (struct eeprom_data *)arg,
			   sizeof(struct eeprom_data))) {
		return -EFAULT;
	}

	// Prevent writing to addresses larger than the amount of available EEPROM subaddresses
	if (data.reg >= EEPROM_SUBADDRESSES) {
		pr_err("[%s] Address 0x%08X is out of range\n", __func__,
		       data.reg);
		return -EINVAL;
	}

	switch (cmd) {
	case IOCTL_WRITE_DATA:

		switch (reg_size) {
		case REG_SIZE_8:
			addr_cell =
				data.reg /
				EEPROM_CELL_SIZE; // Calculate the cell address
			addr_sub = data.reg %
				   EEPROM_CELL_SIZE; // Calculate the subaddress

			pr_info("[%s] Writing to reg 0x%02X data 0x%02X\n",
				__func__, addr_sub, data.data);
			buf8[0] = data.reg; // 8-bit register address
			buf8[1] = data.data; // Data to write
			msgs[0].buf = buf8;
			msgs[0].len = 2;
			break;
		case REG_SIZE_16:
			pr_info("[%s] Writing to reg 0x%04X data 0x%02X\n",
				__func__, data.reg, data.data);
			buf16[0] = (data.reg >> 8) &
				   0xff; // MSB of 16-bit register address
			buf16[1] = data.reg &
				   0xff; // LSB of 16-bit register address
			buf16[2] = data.data;
			msgs[0].buf = buf16;
			msgs[0].len = 3;
			break;
		case REG_SIZE_32:
			pr_info("[%s] Writing to reg 0x%08X data 0x%02X\n",
				__func__, data.reg, data.data);
			buf32[0] = (data.reg >> 24) &
				   0xff; // MSB of 32-bit register address
			buf32[1] = (data.reg >> 16) & 0xff;
			buf32[2] = (data.reg >> 8) & 0xff;
			buf32[3] = data.reg &
				   0xff; // LSB of 32-bit register address
			buf32[4] = data.data; // Data to write
			msgs[0].buf = buf32;
			msgs[0].len = 5;
			break;
		default:
			break;
		}
		// Set the I2C address and flags for the message
		msgs[0].addr = i2c_client_g->addr + addr_cell;
		msgs[0].flags = 0;

		ret = i2c_transfer(i2c_client_g->adapter, msgs, 1);
		if (ret < 0) {
			pr_err("[%s] Failed to write to the EEPROM\n",
			       __func__);
			return -EIO;
		}

		msleep(1); // 10 milliseconds delay to ensure EEPROM is ready

		break;

	case IOCTL_READ_DATA:
		if (reg_size == REG_SIZE_8) {
			pr_info("[%s] Reading from reg 0x%02X\n", __func__,
				data.reg);

			buf8[0] = data.reg; // 8-bit register address

			msgs[0].addr = i2c_client_g->addr;
			msgs[0].flags = 0;
			msgs[0].len = 1;
			msgs[0].buf = buf8;

			msgs[1].addr = i2c_client_g->addr;
			msgs[1].flags = I2C_M_RD;
			msgs[1].len = 1;
			msgs[1].buf = &data.data;
		} else if (reg_size == REG_SIZE_16) {
			pr_info("[%s] Reading from reg 0x%04X\n", __func__,
				data.reg);

			buf16[0] = (data.reg >> 8) & 0xff; // MSB
			buf16[1] = data.reg & 0xff; // LSB

			msgs[0].addr = i2c_client_g->addr;
			msgs[0].flags = 0;
			msgs[0].len = 2;
			msgs[0].buf = buf16;

			msgs[1].addr = i2c_client_g->addr;
			msgs[1].flags = I2C_M_RD;
			msgs[1].len = 1;
			msgs[1].buf = &data.data;
		}

		ret = i2c_transfer(i2c_client_g->adapter, msgs, 2);
		if (ret < 0) {
			pr_err("[%s] Failed to read from the EEPROM\n",
			       __func__);
			return -EIO;
		}

		if (copy_to_user((struct eeprom_data *)arg, &data,
				 sizeof(struct eeprom_data))) {
			return -EFAULT;
		}

		break;

	default:
		return -EINVAL;
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
static int lviconfig_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret; // For return values
	const struct firmware *fw_entry = NULL;

	pr_info("[%s]: probe called for I2C client at addr 0x%x\n", __func__,
		client->addr);

	i2c_client_g =
		client; // Store the client globally for platform functions

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

	if (device_create(lviconfig_class, NULL, MKDEV(major, 0), NULL,
			  DEVICE_NAME) == NULL) {
		pr_err("[%s]: Failed to create device node\n", __func__);
		class_destroy(lviconfig_class);
		unregister_chrdev(major, DEVICE_NAME);
		i2c_client_g = NULL;
		return -ENODEV; // Use a standard error code
	}

	// pr_info("[%s]: Character device created successfully.\n", __func__);

	// Initialize parameter structures and their EEPROM addresses
	// ConfigParam_InitAll();
	// pr_info("[%s]: ConfigParam structures initialized.\n", __func__);

	// ConfigParam_ParseEEPROM(); // Read existing EEPROM content into parameters

	// ConfigParam_PrintAll();

	pr_info("[%s]: I2C client successfully initialized and configured: addr=0x%x\n",
		__func__, client->addr);

	return 0;
}

/**
 * @brief Remove function for I2C driver.
 */
static int lviconfig_remove(struct i2c_client *client)
{
	pr_info("[%s]: remove called for I2C client at addr 0x%x\n", __func__,
		client->addr);

	// Cleanup: Destroy device and class, unregister char device
	device_destroy(lviconfig_class, MKDEV(major, 0));
	class_destroy(lviconfig_class);
	unregister_chrdev(major, DEVICE_NAME);
	i2c_client_g = NULL; // Clear the global client pointer

	// Call a cleanup function in lviconfig_parameters to free allocated memory
	// This function needs to be implemented in lviconfig_parameters.c
	// Example: ConfigParam_DeInitAll();
	pr_info("[%s]: Resources released. Implement ConfigParam_DeInitAll() for full cleanup.\n",
		__func__);

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
MODULE_DESCRIPTION(
	"EEPROM I2C driver kernel module with integrated config parsing");
MODULE_VERSION("1.1");
MODULE_FIRMWARE("lviconfig.conf");