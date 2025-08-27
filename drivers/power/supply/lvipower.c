#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <stdbool.h>
#include "lvipanel_events.h"

#define DEVICE_NAME "lvipower"

static int major;
static struct class *lvipower_class;
static struct i2c_client *i2c_client;

/* OF match table to match the device tree node */
static const struct of_device_id lvipower_of_match[] = {
	{
		.compatible = "lvi,lvipower",
	},
	{},
};
MODULE_DEVICE_TABLE(of, lvipanel_of_match);

static int lvipower_write_command(char data)
{
	int ret;
	struct i2c_msg msgs[1];

	if (!i2c_client || !i2c_client->adapter) {
		pr_err("[%s] I2C client or adapter not initialized\n", __func__);
		return -ENODEV;
	}

	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &data;

	ret = i2c_transfer(i2c_client->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("[%s] Failed to write to I2C device: %d\n", __func__, ret);
		return ret;
	}

	pr_info("[%s] Successfully wrote data to I2C device\n", __func__);
	return 0;
}

static int lvipower_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int lvipower_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = lvipower_open,
	.release = lvipower_release,
};

static int lvipower_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;

	pr_info("[%s] Probing driver\n", __func__); // Added __func__

	pr_info("[%s] I2C client successfully initialized: addr=0x%x\n", __func__, client->addr);
	return 0;
}

static int lvipower_remove(struct i2c_client *client)
{
	pr_info("[%s] Driver removed\n", __func__);
	return 0;
}

static struct i2c_driver lvipower_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = lvipower_of_match,
    },
    .probe = lvipower_probe,
    .remove = lvipower_remove,
};

module_i2c_driver(lvipower_driver); // Registers and unregisters the driver automatically

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION("I2C Kernel Module using i2c_msg named lvipanel with DT support");
