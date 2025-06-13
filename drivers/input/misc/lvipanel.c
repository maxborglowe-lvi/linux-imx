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

#define DEVICE_NAME "lvipanel"
#define IOCTL_READ_DATA _IOR('i', 1, char)
#define IOCTL_WRITE_DATA _IOW('i', 2, char)
#define IOCTL_POLL_DATA _IOW('i', 3, char)

// Signal to send to the user space application
#define DATA_AVAILABLE_SIGNAL SIGUSR1

static struct task_struct *polling_thread; // Kernel thread for polling
static bool keep_polling = true; // Control flag for polling

static int major;
static struct class *lvipanel_class;
static struct i2c_client *i2c_client;
static pid_t user_pid; // To store the PID of the user space application

char i2c_data;

/* OF match table to match the device tree node */
static const struct of_device_id lvipanel_of_match[] = {
	{
		.compatible = "lvipanel",
	},
	{},
};
MODULE_DEVICE_TABLE(of, lvipanel_of_match);

static int lvipanel_write_command(char data)
{
	int ret;
	struct i2c_msg msgs[1];

	if (!i2c_client || !i2c_client->adapter) {
		pr_err("[%s] I2C client or adapter not initialized\n",
		       __func__);
		return -ENODEV;
	}

	msgs[0].addr = i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &data;

	ret = i2c_transfer(i2c_client->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("[%s] Failed to write to I2C device: %d\n", __func__,
		       ret);
		return ret;
	}

	pr_info("[%s] Successfully wrote data to I2C device\n", __func__);
	return 0;
}

static int lvipanel_light_green_flash(void)
{
	lvipanel_write_command(SYSTEM_EVENT_LIGHT_GREEN_FLASH);
	return 0;
}

static int lvipanel_light_green_solid(void)
{
	lvipanel_write_command(SYSTEM_EVENT_LIGHT_GREEN_SOLID);
	return 0;
}

static int lvipanel_light_yellow_flash(void)
{
	lvipanel_write_command(SYSTEM_EVENT_LIGHT_YELLOW_FLASH);
	return 0;
}

static int lvipanel_light_yellow_solid(void)
{
	lvipanel_write_command(SYSTEM_EVENT_LIGHT_YELLOW_SOLID);
	return 0;
}

static int lvipanel_light_off(void)
{
	lvipanel_write_command(SYSTEM_EVENT_LIGHT_OFF);
	return 0;
}

static int lvipanel_polling_thread(void *data)
{
	struct i2c_msg msgs[1];
	int ret;

	pr_info("[%s] Polling thread started\n", __func__);

	while (!kthread_should_stop() && keep_polling) {
		// Prepare I2C read message
		msgs[0].addr = i2c_client->addr;
		msgs[0].flags = I2C_M_RD;
		msgs[0].len = 1;
		msgs[0].buf = &i2c_data;

		// Perform I2C read
		ret = i2c_transfer(i2c_client->adapter, msgs, 1);
		if (ret < 0) {
			// Handle error (optional)
			pr_err("[%s] Failed to read from I2C in polling thread: %d\n",
			       __func__, ret);
		} else {
			// pr_info("Data read from I2C device: 0x%x\n", i2c_data);

			// Check for PANEL_EVENT_ONOFF_RELEASE and trigger shutdown
			if (i2c_data == PANEL_EVENT_SYSTEM_SHUTDOWN) {
				pr_info("[%s] PANEL_EVENT_SYSTEM_SHUTDOWN detected, initiating orderly shutdown\n",
					__func__);
				orderly_poweroff(true); // Initiates shutdown
				break;
			}

			// Check if data is non-zero and send signal if needed
			if (i2c_data != 0 && user_pid > 0) {
				// pr_info("Sending signal to user space process PID: %d\n", user_pid);
				ret = kill_pid(find_vpid(user_pid),
					       DATA_AVAILABLE_SIGNAL, 1);
				if (ret < 0) {
					pr_err("[%s] Failed to send signal to user space: %d\n",
					       __func__, ret);
				}
			}
		}

		// Sleep to avoid busy polling (adjust delay as needed)
		msleep(50); // 50 milliseconds delay
	}

	pr_info("[%s] Polling thread stopping\n", __func__);
	return 0;
}

// Function to be called during shutdown
static int lvipanel_reboot_notifier(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	switch (action) {
	case SYS_DOWN:
	case SYS_HALT:
	case SYS_POWER_OFF:
		pr_info("[%s] System is shutting down. Sending SYSTEM_EVENT_LIGHT_OFF\n",
			__func__);
		lvipanel_write_command(
			SYSTEM_EVENT_LIGHT_OFF); // Send the light off command
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

// Declare the notifier block
static struct notifier_block lvipanel_reboot_nb = {
	.notifier_call = lvipanel_reboot_notifier,
};

static long lvipanel_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	char data;
	int ret;
	struct i2c_msg msgs[1];

	pr_info("[%s] IOCTL command received: cmd=%d\n", __func__, cmd);

	switch (cmd) {
	case IOCTL_READ_DATA:
		pr_info("[%s] IOCTL_READ_DATA command\n", __func__);

		// msgs[0].addr  = i2c_client->addr;
		// msgs[0].flags = I2C_M_RD;
		// msgs[0].len   = 1;
		// msgs[0].buf   = &data;

		// ret = i2c_transfer(i2c_client->adapter, msgs, 1);
		// if (ret < 0) {
		//     pr_err("Failed to read from I2C device: %d\n", ret);
		//     return ret;
		// }

		// pr_info("[%s] Data read from I2C device: 0x%x\n", __func__, data);

		if (copy_to_user((char __user *)arg, &i2c_data,
				 sizeof(i2c_data))) {
			pr_err("[%s] Failed to copy data to user space\n",
			       __func__);
			return -EFAULT;
		}

		// Check if the data is not zero and notify the user space application
		//  if (data != 0 && user_pid > 0) {
		//     pr_info("[%s] Sending signal to user space process PID: %d\n", __func__, user_pid);
		//     int ret = kill_pid(find_vpid(user_pid), DATA_AVAILABLE_SIGNAL, 1);
		//     if (ret < 0) {
		//         pr_err("[%s] Failed to send signal to user space: %d\n", __func__, ret);
		//     }
		// }
		return 0;

	case IOCTL_WRITE_DATA:
		pr_info("[%s] IOCTL_WRITE_DATA command\n", __func__);

		if (copy_from_user(&data, (char __user *)arg, sizeof(data))) {
			pr_err("[%s] Failed to copy data from user space\n",
			       __func__);
			return -EFAULT;
		}

		pr_info("[%s] Data to be written to I2C device: 0x%x\n",
			__func__, data);

		msgs[0].addr = i2c_client->addr;
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &data;

		ret = i2c_transfer(i2c_client->adapter, msgs, 1);
		if (ret < 0) {
			pr_err("[%s] Failed to write to I2C device: %d\n",
			       __func__, ret);
			return ret;
		}
		return 0;

	default:
		pr_err("[%s] Invalid IOCTL command\n", __func__);
		return -EINVAL;
	}
}

static int lvipanel_open(struct inode *inode, struct file *file)
{
	lvipanel_light_green_solid();

	user_pid = current->pid;
	pr_info("[%s] lvipanel: Stored user PID: %d\n", __func__, user_pid);
	return 0;
}

static int lvipanel_release(struct inode *inode, struct file *file)
{
	lvipanel_light_off();

	pr_info("[%s] lvipanel device released\n", __func__);
	user_pid = 0; // Reset the PID on release
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = lvipanel_open,
	.release = lvipanel_release,
	.unlocked_ioctl = lvipanel_ioctl,
};

static int lvipanel_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;

	pr_info("[%s] Probing lvipanel driver\n", __func__); // Added __func__

	register_reboot_notifier(&lvipanel_reboot_nb);

	if (!np) {
		pr_err("[%s] Device tree node not found\n", __func__);
		return -EINVAL;
	}

	if (!client->adapter) {
		pr_err("[%s] I2C adapter not available\n", __func__);
		return -EPROBE_DEFER; // Defer probe until adapter is ready
	}

	i2c_client = client;

	/* Register the character device */
	major = register_chrdev(0, DEVICE_NAME, &fops);
	if (major < 0) {
		pr_err("[%s] Failed to register character device: %d\n",
		       __func__, major);
		return major;
	}

	pr_info("[%s] lvipanel device registered with major number %d\n",
		__func__, major);

	/* Create the device class */
	lvipanel_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(lvipanel_class)) {
		unregister_chrdev(major, DEVICE_NAME);
		pr_err("[%s] Failed to create device class\n",
		       __func__); // Added __func__
		return PTR_ERR(lvipanel_class);
	}

	/* Create the device node */
	if (device_create(lvipanel_class, NULL, MKDEV(major, 0), NULL,
			  DEVICE_NAME) == NULL) {
		class_destroy(lvipanel_class);
		unregister_chrdev(major, DEVICE_NAME);
		pr_err("[%s] Failed to create device node\n", __func__);
		return -1;
	}

	lvipanel_light_green_solid();

	// Start the polling thread
	keep_polling = true;
	polling_thread =
		kthread_run(lvipanel_polling_thread, NULL, "lvipanel_polling");
	if (IS_ERR(polling_thread)) {
		pr_err("[%s] Failed to create polling thread\n", __func__);
		device_destroy(lvipanel_class, MKDEV(major, 0));
		class_destroy(lvipanel_class);
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(polling_thread);
	}

	pr_info("[%s] I2C client successfully initialized: addr=0x%x\n",
		__func__, client->addr);
	return 0;
}

static int lvipanel_remove(struct i2c_client *client)
{
	pr_info("[%s] Removing lvipanel driver\n", __func__);

	unregister_reboot_notifier(&lvipanel_reboot_nb);

	// Stop the polling thread
	keep_polling = false;
	if (polling_thread) {
		kthread_stop(polling_thread);
	}

	device_destroy(lvipanel_class, MKDEV(major, 0));
	class_destroy(lvipanel_class);
	unregister_chrdev(major, DEVICE_NAME);

	pr_info("[%s] lvipanel driver removed\n", __func__);
	return 0;
}

static struct i2c_device_id lvipanel_id[] = { { DEVICE_NAME, 0 }, {} };
MODULE_DEVICE_TABLE(i2c, lvipanel_id);

static struct i2c_driver lvipanel_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = lvipanel_of_match,
    },
    .probe = lvipanel_probe,
    .remove = lvipanel_remove,
    .id_table = lvipanel_id,
};

module_i2c_driver(
	lvipanel_driver); // Registers and unregisters the driver automatically

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION(
	"I2C Kernel Module using i2c_msg named lvipanel with DT support");
