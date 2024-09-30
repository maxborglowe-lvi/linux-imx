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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/delay.h>


#define DEVICE_NAME "lviconfig"
#define EEPROM_SIZE 32768
#define IOCTL_WRITE_DATA _IOW('e', 1, struct eeprom_data)
#define IOCTL_READ_DATA _IOR('e', 2, struct eeprom_data)

static int major;
static struct i2c_client *i2c_client;
static struct class *lviconfig_class;

#define REG_SIZE_8  2
#define REG_SIZE_16 3
#define REG_SIZE_32 5

uint8_t reg_size = REG_SIZE_16;

struct eeprom_data {
    uint32_t reg;
    uint8_t data;
};

// Device tree match table
static const struct of_device_id lviconfig_of_match[] = {
    { .compatible = "lviconfig", },
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
static long lviconfig_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    uint8_t buf32[5]; /**< Buffer for 32-bit addressing of EEPROM (4 address bytes, 1 data byte) */
    uint8_t buf16[3]; /**< Buffer for 16-bit addressing of EEPROM (2 address bytes, 1 data byte) */
    uint8_t buf8[2];  /**< Buffer for 8-bit addressing of EEPROM (1 address byte, 1 data byte) */
    struct i2c_msg msgs[2]; /**< I2C message array for transaction */
    int ret; /**< Return value for I2C transfer */
    
    struct eeprom_data data; /**< Data structure for the current IOCTL operation */

    if (copy_from_user(&data, (struct eeprom_data *)arg, sizeof(struct eeprom_data))) {
        return -EFAULT;
    }

    // Prevent writing to addresses larger than EEPROM_SIZE
    if (data.reg >= EEPROM_SIZE) {
        printk(KERN_ERR "EEPROM: Address 0x%08X is out of range\n", data.reg);
        return -EINVAL;
    }

    switch (cmd) {
        case IOCTL_WRITE_DATA:
           
            switch (reg_size) {
                case REG_SIZE_8:
                    printk(KERN_INFO "EEPROM: Writing to reg 0x%02X data 0x%02X\n", data.reg, data.data);
                    buf8[0] = data.reg;   // 8-bit register address
                    buf8[1] = data.data;  // Data to write
                    msgs[0].buf = buf8;
                    msgs[0].len = 2;
                    break;
                case REG_SIZE_16:
                    printk(KERN_INFO "EEPROM: Writing to reg 0x%04X data 0x%02X\n", data.reg, data.data);
                    buf16[0] = (data.reg >> 8) & 0xff;   // MSB of 16-bit register address
                    buf16[1] = data.reg & 0xff;          // LSB of 16-bit register address
                    buf16[2] = data.data;  
                    msgs[0].buf = buf16;
                    msgs[0].len = 3;
                    break;
                case REG_SIZE_32:
                    printk(KERN_INFO "EEPROM: Writing to reg 0x%08X data 0x%02X\n", data.reg, data.data);
                    buf32[0] = (data.reg >> 24) & 0xff;  // MSB of 32-bit register address
                    buf32[1] = (data.reg >> 16) & 0xff; 
                    buf32[2] = (data.reg >> 8) & 0xff; 
                    buf32[3] = data.reg & 0xff;          // LSB of 32-bit register address
                    buf32[4] = data.data;  // Data to write
                    msgs[0].buf = buf32;
                    msgs[0].len = 5;
                    break;
                default:
                    break;
            }

            msgs[0].addr = i2c_client->addr;
            msgs[0].flags = 0;

            ret = i2c_transfer(i2c_client->adapter, msgs, 1);
            if (ret < 0) {
                printk(KERN_ERR "EEPROM: Failed to write to the EEPROM\n");
                return -EIO;
            }

            msleep(1);  // 10 milliseconds delay to ensure EEPROM is ready

            break;

        case IOCTL_READ_DATA:
            if(reg_size == REG_SIZE_8){
                printk(KERN_INFO "EEPROM: Reading from reg 0x%02X\n", data.reg);

                buf8[0] = data.reg;   // 8-bit register address

                msgs[0].addr = i2c_client->addr;
                msgs[0].flags = 0;
                msgs[0].len = 1;
                msgs[0].buf = buf8;

                msgs[1].addr = i2c_client->addr;
                msgs[1].flags = I2C_M_RD;
                msgs[1].len = 1;
                msgs[1].buf = &data.data;

            } else if (reg_size == REG_SIZE_16) {
                printk(KERN_INFO "EEPROM: Reading from reg 0x%04X\n", data.reg);

                buf16[0] = (data.reg >> 8) & 0xff;  // MSB
                buf16[1] = data.reg & 0xff;         // LSB

                msgs[0].addr = i2c_client->addr;
                msgs[0].flags = 0;
                msgs[0].len = 2;
                msgs[0].buf = buf16;

                msgs[1].addr = i2c_client->addr;
                msgs[1].flags = I2C_M_RD;
                msgs[1].len = 1;
                msgs[1].buf = &data.data;
            }

            ret = i2c_transfer(i2c_client->adapter, msgs, 2);
            if (ret < 0) {
                printk(KERN_ERR "EEPROM: Failed to read from the EEPROM\n");
                return -EIO;
            }

            if (copy_to_user((struct eeprom_data *)arg, &data, sizeof(struct eeprom_data))) {
                return -EFAULT;
            }

            break;

        default:
            return -EINVAL;
    }

    return 0;
}

static int lviconfig_open(struct inode *inode, struct file *file) {
    return 0;
}

static int lviconfig_release(struct inode *inode, struct file *file) {
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = lviconfig_open,
    .release = lviconfig_release,
    .unlocked_ioctl = lviconfig_ioctl,
};

/**
 * @brief Probe function for I2C driver.
 */
static int lviconfig_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    pr_info("[lviconfig_probe] call\n");

    int ret;

    i2c_client = client;

    // Register the character device
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        pr_err("Failed to register character device\n");
        return major;
    }

    // Create the device class
    lviconfig_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(lviconfig_class)) {
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(lviconfig_class);
    }

    // Create the device node
    if (device_create(lviconfig_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME) == NULL) {
        class_destroy(lviconfig_class);
        unregister_chrdev(major, DEVICE_NAME);
        return -1;
    }

    pr_info("lviconfig I2C client successfully initialized: addr=0x%x\n", client->addr);
    return 0;
}

/**
 * @brief Remove function for I2C driver.
 */
static int lviconfig_remove(struct i2c_client *client) {
    pr_info("[lviconfig_remove] call\n");

    // Cleanup: Destroy device and class, unregister char device
    device_destroy(lviconfig_class, MKDEV(major, 0));
    class_destroy(lviconfig_class);
    unregister_chrdev(major, DEVICE_NAME);

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
MODULE_DESCRIPTION("EEPROM I2C driver kernel module");
MODULE_VERSION("1.0");
