#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>
#include <linux/of.h> // For device tree support

#define DEVICE_NAME "lvibattery"
#define IOCTL_READ_DATA _IOR('b', 1, struct i2c_data)
#define IOCTL_WRITE_DATA _IOW('b', 2, struct i2c_data)
#define IOCTL_READ_STRING _IOR('b', 3, struct i2c_string_data)

static int major;
static struct class *lvibattery_class;
static struct i2c_client *i2c_client;

struct i2c_data {
	unsigned char reg;
	unsigned short value;
};

#define MAX_STRING_SIZE 32

struct i2c_string_data {
	unsigned char reg; // Start register
	unsigned char length; // Length of string (max 32 bytes)
	char value[MAX_STRING_SIZE]; // Buffer for the string
};

/* OF match table to match the device tree node */
static const struct of_device_id lvibattery_of_match[] = {
	{
		.compatible = "lvi,lvibattery",
	},
	{},
};
MODULE_DEVICE_TABLE(of, lvibattery_of_match);

static long lvibattery_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	struct i2c_data reg_data;
	struct i2c_string_data string_data;
	int ret;
	struct i2c_msg msgs[2];
	unsigned char reg;
	unsigned char buffer[2]; // Buffer for 16-bit values (2 bytes)

	pr_info("[%s] IOCTL command received: cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_READ_DATA:
		pr_info("[%s] IOCTL_READ_DATA command\n", __func__);

		if (copy_from_user(&reg_data, (struct i2c_data __user *)arg,
				   sizeof(reg_data))) {
			pr_err("[%s] Failed to copy data from user space\n",
			       __func__);
			return -EFAULT;
		}

		reg = reg_data.reg;

		/* Send the register address first */
		msgs[0].addr = i2c_client->addr; // Smart battery address
		msgs[0].flags = 0; // Write (to send register address)
		msgs[0].len = 1; // Writing the register address
		msgs[0].buf = &reg;

		/* Then read 2 bytes (16-bit data) from the register */
		msgs[1].addr = i2c_client->addr; // Smart battery address
		msgs[1].flags = I2C_M_RD; // Read
		msgs[1].len = 2; // Read 2 bytes (16-bit)
		msgs[1].buf = buffer; // Buffer to hold the 16-bit value

		ret = i2c_transfer(i2c_client->adapter, msgs, 2);
		if (ret < 0) {
			pr_err("[%s] Failed to read from I2C device: %d\n",
			       __func__, ret);
			return ret;
		}

		/* Combine the 2 bytes into a 16-bit value */
		reg_data.value = (buffer[1] << 8) | buffer[0];

		pr_info("[%s] Data read from I2C device: reg=0x%x, value=0x%x\n",
			__func__, reg, reg_data.value);

		if (copy_to_user((struct i2c_data __user *)arg, &reg_data,
				 sizeof(reg_data))) {
			pr_err("[%s] Failed to copy data to user space\n",
			       __func__);
			return -EFAULT;
		}
		return 0;

	case IOCTL_WRITE_DATA: {
		unsigned char write_buffer
			[3]; // Increased size to avoid magic numbers for len
		pr_info("[%s] IOCTL_WRITE_DATA command\n", __func__);

		if (copy_from_user(&reg_data, (struct i2c_data __user *)arg,
				   sizeof(reg_data))) {
			pr_err("[%s] Failed to copy data from user space\n",
			       __func__);
			return -EFAULT;
		}

		pr_info("[%s] Writing to I2C device: reg=0x%x, value=0x%x\n",
			__func__, reg_data.reg, reg_data.value);

		write_buffer[0] = reg_data.reg;
		write_buffer[1] = reg_data.value & 0xFF; // LSB
		write_buffer[2] = (reg_data.value >> 8) & 0xFF; // MSB

		/* Send the register address and the data to write */
		msgs[0].addr = i2c_client->addr; // Smart battery address
		msgs[0].flags = 0; // Write
		msgs[0].len = 3; // Register + 2 bytes of data
		msgs[0].buf = write_buffer;

		ret = i2c_transfer(i2c_client->adapter, msgs, 1);
		if (ret < 0) {
			pr_err("[%s] Failed to write to I2C device: %d\n",
			       __func__, ret);
			return ret;
		}
		return 0;
	}

	case IOCTL_READ_STRING:
		pr_info("[%s] IOCTL_READ_STRING command\n", __func__);

		if (copy_from_user(&string_data,
				   (struct i2c_string_data __user *)arg,
				   sizeof(string_data))) {
			pr_err("[%s] Failed to copy string data from user space\n",
			       __func__);
			return -EFAULT;
		}

		if (string_data.length > MAX_STRING_SIZE) {
			pr_err("[%s] Requested length exceeds maximum allowed size\n",
			       __func__);
			return -EINVAL;
		}

		reg = string_data.reg;

		/* Send the register address first */
		msgs[0].addr = i2c_client->addr; // Smart battery address
		msgs[0].flags = 0; // Write
		msgs[0].len = 1;
		msgs[0].buf = &reg;

		/* Then read the requested number of bytes */
		msgs[1].addr = i2c_client->addr;
		msgs[1].flags = I2C_M_RD; // Read
		msgs[1].len = string_data.length;
		msgs[1].buf = string_data.value;

		ret = i2c_transfer(i2c_client->adapter, msgs, 2);
		if (ret < 0) {
			pr_err("[%s] Failed to read string from I2C device: %d\n",
			       __func__, ret);
			return ret;
		}

		pr_info("[%s] String read from I2C device: reg=0x%x, length=%d\n",
			__func__, reg, string_data.length);

		if (copy_to_user((struct i2c_string_data __user *)arg,
				 &string_data, sizeof(string_data))) {
			pr_err("[%s] Failed to copy string data to user space\n",
			       __func__);
			return -EFAULT;
		}
		return 0;

	default:
		pr_err("[%s] Invalid IOCTL command\n", __func__);
		return -EINVAL;
	}
}

static int lvibattery_open(struct inode *inode, struct file *file)
{
	pr_info("[%s] lvibattery device opened\n", __func__);
	return 0;
}

static int lvibattery_release(struct inode *inode, struct file *file)
{
	pr_info("[%s] lvibattery device released\n", __func__);
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = lvibattery_open,
	.release = lvibattery_release,
	.unlocked_ioctl = lvibattery_ioctl,
};

static int lvibattery_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	unsigned char test_byte;
	struct i2c_msg msgs[1];

	pr_info("[%s] Probing lvibattery driver\n", __func__);

	if (!client) {
		pr_err("[%s] I2C client is NULL\n", __func__);
		return -EINVAL;
	}

	i2c_client = client;

	// Attempt a test read to verify device presence
	msgs[0].addr = client->addr;
	msgs[0].flags = I2C_M_RD; // Read
	msgs[0].len = 1;
	msgs[0].buf = &test_byte;

	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("[%s] Failed to communicate with I2C device at addr 0x%x: %d\n",
		       __func__, client->addr, ret);
		i2c_client = NULL; // Clear the global client if probe fails
		return -ENODEV; // Return device not found
	}

	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		pr_err("[%s] Failed to register character device: %d\n",
		       __func__, major);
		return major;
	}

	lvibattery_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(lvibattery_class)) {
		unregister_chrdev(major, DEVICE_NAME);
		pr_err("[%s] Failed to create device class\n", __func__);
		return PTR_ERR(lvibattery_class);
	}

	if (IS_ERR(device_create(lvibattery_class, NULL, MKDEV(major, 0), NULL,
				 DEVICE_NAME))) {
		class_destroy(lvibattery_class);
		unregister_chrdev(major, DEVICE_NAME);
		pr_err("[%s] Failed to create device node\n", __func__);
		return -1;
	}

	pr_info("[%s] I2C client successfully initialized: addr=0x%x\n",
		__func__, client->addr);
	return 0;
}

static int lvibattery_remove(struct i2c_client *client)
{
	pr_info("[%s] Removing lvibattery driver\n", __func__);

	device_destroy(lvibattery_class, MKDEV(major, 0));
	class_destroy(lvibattery_class);
	unregister_chrdev(major, DEVICE_NAME);

	pr_info("[%s] lvibattery driver removed\n", __func__);

	return 0;
}

static struct i2c_device_id lvibattery_id[] = { { DEVICE_NAME, 0 }, {} };
MODULE_DEVICE_TABLE(i2c, lvibattery_id);

static struct i2c_driver lvibattery_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = lvibattery_of_match,
    },
    .probe = lvibattery_probe,
    .remove = lvibattery_remove,
    .id_table = lvibattery_id,
};

module_i2c_driver(
	lvibattery_driver); // Registers and unregisters the driver automatically

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION(
	"I2C Kernel Module for Smart Battery at 0x0b with DT support");
