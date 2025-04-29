#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/err.h>

#define TCA6416_REG_I2C_ADDRESS 0x20
#define TCA6416_REG_OUTPUT_PORT0   0x02
#define TCA6416_REG_OUTPUT_PORT1   0x03
#define TCA6416_REG_CONFIGURATION_PORT0 0x06
#define TCA6416_REG_CONFIGURATION_PORT1 0x07

// Desired Values
#define CONFIG_PORT0  0x66
#define CONFIG_PORT1  0x00
#define OUTPUT_PORT0  0xEE
#define OUTPUT_PORT1  0x3F

// Define the I2C client structure for your GPIO expander
struct tca6416_data {
    struct i2c_client *client;
};

// Write a register in the TCA6416 device
static int tca6416_write_register(struct i2c_client *client, u8 reg, u8 value)
{
    if (!client || !client->adapter) {
        pr_err("Invalid I2C client or adapter\n");
        return -EINVAL;
    }

    pr_debug("Writing to reg: 0x%x, value: 0x%x\n", reg, value);

    // Check that the I2C adapter supports the SMBus byte data functionality
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        pr_err("I2C adapter does not support SMBus byte data\n");
        return -EIO;
    }

    // Write the byte to the I2C device
    return i2c_smbus_write_byte_data(client, reg, value);
}

// Initialization function for the TCA6416 GPIO expander
static int tca6416_init(struct i2c_client *client)
{
    struct tca6416_data *data;

    // Validate the I2C client
    if (!client) {
        pr_err("Invalid I2C client during initialization\n");
        return -EINVAL;
    }

    // Allocate memory for the driver data
    data = devm_kzalloc(&client->dev, sizeof(struct tca6416_data), GFP_KERNEL);
    if (!data) {
        pr_err("Failed to allocate memory for driver data\n");
        return -ENOMEM;
    }

    // Set up the I2C client data structure
    data->client = client;
    i2c_set_clientdata(client, data);

    // Configure GPIO directions
    if (tca6416_write_register(client, TCA6416_REG_CONFIGURATION_PORT0, CONFIG_PORT0) < 0)
        return -EIO;
    if (tca6416_write_register(client, TCA6416_REG_CONFIGURATION_PORT1, CONFIG_PORT1) < 0)
        return -EIO;

    // Set default output values
    if (tca6416_write_register(client, TCA6416_REG_OUTPUT_PORT0, OUTPUT_PORT0) < 0)
        return -EIO;
    if (tca6416_write_register(client, TCA6416_REG_OUTPUT_PORT1, OUTPUT_PORT1) < 0)
        return -EIO;

    pr_info("TCA6416 GPIO expander initialized\n");

    return 0;
}

// Probe function called when the device is detected
static int tca6416_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    pr_info("TCA6416 probe function called\n");

    // Initialize the device
    return tca6416_init(client);
}

// Remove function for cleanup
static int tca6416_remove(struct i2c_client *client)
{
    pr_info("TCA6416 remove function called\n");

    // Cleanup code (if necessary)
    return 0;
}

// Device tree matching table
static const struct of_device_id tca6416_of_match[] = {
    { .compatible = "ti,tca6416", },
    { },
};
MODULE_DEVICE_TABLE(of, tca6416_of_match);

// I2C driver structure
static struct i2c_driver tca6416_driver = {
    .driver = {
        .name = "tca6416",
        .of_match_table = tca6416_of_match,
    },
    .probe = tca6416_probe,
    .remove = tca6416_remove,
};

// Register the driver
module_i2c_driver(tca6416_driver);

MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION("TCA6416 GPIO Expander Driver");
MODULE_LICENSE("GPL");
